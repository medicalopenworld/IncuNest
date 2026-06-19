import io
import os
import re
import sys
import tempfile
import threading
from pathlib import Path
from typing import Callable, Optional

import esptool

import nvs_gen
from detector import Board


# Force plain-text esptool output for all threads (no ANSI / rich).
os.environ.setdefault('NO_COLOR', '1')

_tl = threading.local()          # per-thread writer slot
_proxy_lock = threading.Lock()
_proxy_installed = False


class _EsptoolProxy(io.TextIOBase):
    """Thread-local stdout/stderr proxy for concurrent esptool calls.

    Installed once as sys.stdout and sys.stderr so that parallel flash
    threads each route output only to their own per-slot writer.
    """

    @property
    def encoding(self) -> str:
        return 'utf-8'

    def write(self, s: str) -> int:
        w = getattr(_tl, 'writer', None)
        if w is not None:
            return w.write(s)
        return len(s)  # discard; don't block esptool callers

    def flush(self) -> None:
        w = getattr(_tl, 'writer', None)
        if w is not None:
            try:
                w.flush()
            except Exception:
                pass


def _install_proxy() -> None:
    global _proxy_installed
    if _proxy_installed:
        return
    with _proxy_lock:
        if not _proxy_installed:
            proxy = _EsptoolProxy()
            sys.stdout = proxy
            sys.stderr = proxy
            _proxy_installed = True


_BOARD_FOLDER = {
    Board.MOTHERBOARD: 'motherboard',
    Board.DISPLAY_HMI: 'display_hmi',
}

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
        m = re.search(r'\((\d+)\s*%\)', text)
        if m:
            pct = int(m.group(1))
            self._last = pct
            return pct

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
    """Return True if the app partition (0x10000) contains firmware."""
    _install_proxy()

    fd, tmp = tempfile.mkstemp(suffix='.bin')
    os.close(fd)

    _tl.writer = io.StringIO()  # discard all esptool output
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
        return True  # conservative: assume firmware present, preserve NVS
    finally:
        _tl.writer = None
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
    _install_proxy()

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
        'write-flash',
        '--flash-mode', 'keep',
        '--flash-freq', 'keep',
        '--flash-size', 'detect',
        '--compress',
    ]
    for addr, fname in file_pairs:
        args += [addr, fname if os.path.isabs(fname) else str(folder / fname)]

    reset_seen = False

    class _Writer(io.StringIO):
        def write(self, text: str) -> int:
            nonlocal reset_seen
            result = super().write(text)
            if 'resetting' in text.lower():
                reset_seen = True
            if text.strip():
                progress_callback(text.rstrip(), tracker.parse(text))
            return result

    _tl.writer = _Writer()
    try:
        esptool.main(args)
    except SystemExit as e:
        if e.code != 0:
            raise RuntimeError(
                f"esptool terminó con error (código {e.code}). "
                "Flasheo fallido. Vuelve a intentarlo."
            )
    except Exception as e:
        if not reset_seen:
            raise RuntimeError(str(e))
    finally:
        _tl.writer = None
        if nvs_tmp:
            try:
                os.unlink(nvs_tmp)
            except OSError:
                pass
