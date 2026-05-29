import io
import os
import re
import sys
import tempfile
from pathlib import Path
from typing import Callable, Optional

import esptool

import nvs_gen
from detector import Board


_BOARD_FOLDER = {
    Board.MOTHERBOARD: 'motherboard',
    Board.DISPLAY_HMI: 'display_hmi',
}

# ESP32-S3 native USB supports automatic bootloader entry via USB reset sequence.
# CH340K boards use classic DTR/RTS toggle (default_reset).
_BOARD_BEFORE_RESET = {
    Board.MOTHERBOARD: 'default-reset',
    Board.DISPLAY_HMI: 'default-reset',
}

_BOARD_FILES = {
    Board.MOTHERBOARD: [
        ('0x0000', 'bootloader.bin'),
        ('0x8000', 'partitions.bin'),
        ('0x10000', 'firmware.bin'),
    ],
    Board.DISPLAY_HMI: [
        ('0x0000', 'bootloader.bin'),
        ('0x8000', 'partitions.bin'),
        ('0xE000', 'ota_data_initial.bin'),
        ('0x10000', 'firmware.bin'),
    ],
}


class _ProgressTracker:
    """Parse esptool output for progress percentage.

    Primary: match esptool's '(N %)' format.
    Fallback: estimate from 'Writing at 0x...' addresses when esptool
    suppresses percentage output (e.g. when rich detects non-TTY stdout).
    """

    def __init__(self, file_pairs: list[tuple[str, str]], folder: Path) -> None:
        segs: list[tuple[int, int]] = []
        for addr_str, fname in file_pairs:
            size = (folder / fname).stat().st_size
            start = int(addr_str, 16)
            segs.append((start, start + size))
        segs.sort()
        self._segs = segs
        self._total = sum(e - s for s, e in segs)
        self._last = 0

    def parse(self, text: str) -> Optional[int]:
        # Direct percentage from esptool (stub mode or plain-text mode)
        m = re.search(r'\((\d+)\s*%\)', text)
        if m:
            pct = int(m.group(1))
            self._last = pct
            return pct

        # Fallback: estimate from the write address
        m = re.search(r'[Ww]riting at 0x([0-9a-fA-F]+)', text)
        if m and self._total > 0:
            addr = int(m.group(1), 16)
            done = 0
            for seg_start, seg_end in self._segs:
                if addr >= seg_end:
                    done += seg_end - seg_start
                elif addr > seg_start:
                    done += addr - seg_start
            pct = min(int(done * 100 / self._total), 99)
            if pct > self._last:
                self._last = pct
                return pct

        return None


def has_firmware_flashed(port: str) -> bool:
    """Return True if the app partition (0x10000) contains firmware.

    Reads only 4 bytes using the ROM bootloader (--no-stub) so no stub
    is left in RAM and the port is cleanly released.  If the bytes are
    all 0xFF the flash is erased (new device); anything else means
    firmware has been written before.  Returns False on any error so the
    caller can fall back to showing the serial dialog.
    """
    fd, tmp = tempfile.mkstemp(suffix='.bin')
    os.close(fd)

    prev_no_color = os.environ.get('NO_COLOR')
    os.environ['NO_COLOR'] = '1'
    old_out, old_err = sys.stdout, sys.stderr
    sys.stdout = io.StringIO()
    sys.stderr = sys.stdout

    try:
        esptool.main([
            '--port', port,
            '--chip', 'esp32s3',
            '--no-stub',
            '--before', 'no-reset',
            '--after', 'no-reset',
            'read_flash',
            '0x10000', '4', tmp,
        ])
        data = open(tmp, 'rb').read()
        return len(data) == 4 and data != b'\xff\xff\xff\xff'
    except Exception:
        return False
    finally:
        sys.stdout = old_out
        sys.stderr = old_err
        if prev_no_color is None:
            os.environ.pop('NO_COLOR', None)
        else:
            os.environ['NO_COLOR'] = prev_no_color
        try:
            os.unlink(tmp)
        except OSError:
            pass


def flash_board(
    port: str,
    board: Board,
    firmware_base: Path,
    progress_callback: Callable[[str, Optional[int]], None],
    serial_number: Optional[int] = None,
) -> None:
    folder = firmware_base / _BOARD_FOLDER[board]
    file_pairs = list(_BOARD_FILES[board])

    for _, filename in file_pairs:
        filepath = folder / filename
        if not filepath.exists():
            raise FileNotFoundError(f"Archivo no encontrado: {filepath}")

    nvs_tmp: Optional[str] = None

    if serial_number is not None and board == Board.MOTHERBOARD:
        nvs_offset, nvs_size = nvs_gen.find_nvs_partition(folder / 'partitions.bin')
        nvs_data = nvs_gen.generate_serial_nvs(serial_number, nvs_size)

        fd, nvs_tmp = tempfile.mkstemp(suffix='.bin')
        try:
            os.write(fd, nvs_data)
        finally:
            os.close(fd)
        file_pairs = file_pairs + [(hex(nvs_offset), nvs_tmp)]

    tracker = _ProgressTracker(file_pairs, folder)

    args = [
        '--port', port,
        '--chip', 'esp32s3',
        '--baud', '921600',
        '--before', _BOARD_BEFORE_RESET[board],
        # soft-reset tells the stub to jump directly into the flashed app without
        # triggering a hardware reset. A hardware reset via USB-JTAG causes
        # USB_UART_CHIP_RESET which always enters download mode on ESP32-S3.
        '--after', 'soft-reset',
        'write-flash',
        '--flash-mode', 'keep',
        '--flash-freq', 'keep',
        '--flash-size', 'detect',
        '--compress',
    ]
    for addr, fname in file_pairs:
        args += [addr, fname if os.path.isabs(fname) else str(folder / fname)]

    class _Writer(io.StringIO):
        def write(self, text: str) -> int:
            nonlocal reset_seen
            result = super().write(text)
            if 'reset' in text.lower() or 'running' in text.lower():
                reset_seen = True
            if text.strip():
                progress_callback(text.rstrip(), tracker.parse(text))
            return result

    # Disable rich/color output so esptool writes plain parseable text
    _prev_no_color = os.environ.get('NO_COLOR')
    os.environ['NO_COLOR'] = '1'

    writer = _Writer()
    old_out, old_err = sys.stdout, sys.stderr
    sys.stdout = writer
    sys.stderr = writer
    reset_seen = False

    try:
        esptool.main(args)
    except SystemExit as e:
        if e.code != 0:
            raise RuntimeError(
                f"esptool terminó con error (código {e.code}). "
                "Flasheo fallido. Vuelve a intentarlo."
            )
    except Exception as e:
        # soft-reset causes the app's USB to take over; the port may briefly
        # disappear. Treat any serial error after seeing a reset as success.
        if not reset_seen:
            raise RuntimeError(str(e))
    finally:
        sys.stdout = old_out
        sys.stderr = old_err
        if _prev_no_color is None:
            os.environ.pop('NO_COLOR', None)
        else:
            os.environ['NO_COLOR'] = _prev_no_color
        if nvs_tmp:
            try:
                os.unlink(nvs_tmp)
            except OSError:
                pass
