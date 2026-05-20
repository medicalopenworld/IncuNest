import io
import re
import sys
from pathlib import Path
from typing import Callable, Optional

import esptool

from detector import Board


_BOARD_FOLDER = {
    Board.MOTHERBOARD: 'motherboard',
    Board.DISPLAY_HMI: 'display_hmi',
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


def flash_board(
    port: str,
    board: Board,
    firmware_base: Path,
    progress_callback: Callable[[str, Optional[int]], None],
) -> None:
    folder = firmware_base / _BOARD_FOLDER[board]
    file_pairs = _BOARD_FILES[board]

    args = [
        '--port', port,
        '--chip', 'esp32s3',
        '--baud', '1500000',
        '--no-stub',
        'write_flash',
        '--flash_mode', 'qio',
        '--flash_size', 'detect',
    ]

    for addr, filename in file_pairs:
        filepath = folder / filename
        if not filepath.exists():
            raise FileNotFoundError(f"Archivo no encontrado: {filepath}")
        args += [addr, str(filepath)]

    class _ProgressWriter(io.StringIO):
        def write(self, text: str) -> int:
            result = super().write(text)
            if text.strip():
                progress_callback(text.rstrip(), _parse_percent(text))
            return result

    writer = _ProgressWriter()
    old_stdout, old_stderr = sys.stdout, sys.stderr
    sys.stdout = writer
    sys.stderr = writer
    try:
        esptool.main(args)
    except SystemExit as e:
        if e.code != 0:
            raise RuntimeError(
                f"esptool terminó con error (código {e.code}). "
                "Flasheo fallido. Vuelve a intentarlo."
            )
    finally:
        sys.stdout = old_stdout
        sys.stderr = old_stderr


def _parse_percent(text: str) -> Optional[int]:
    m = re.search(r'\((\d+)\s*%\)', text)
    return int(m.group(1)) if m else None
