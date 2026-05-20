import io
import sys
from enum import Enum

import esptool


class Board(Enum):
    MOTHERBOARD = "motherBoard"
    DISPLAY_HMI = "Display HMI"


class BoardDetectionError(Exception):
    pass


_FLASH_SIZE_MAP = {
    '8MB': Board.MOTHERBOARD,
    '16MB': Board.DISPLAY_HMI,
}


def detect_board(port: str) -> Board:
    flash_size = _read_flash_size(port)
    board = _FLASH_SIZE_MAP.get(flash_size)
    if board is None:
        raise BoardDetectionError(
            f"Placa no reconocida ({flash_size}). Solo se admiten motherBoard y Display HMI."
        )
    return board


def _read_flash_size(port: str) -> str:
    buf = io.StringIO()
    old_stdout, old_stderr = sys.stdout, sys.stderr
    sys.stdout = buf
    sys.stderr = buf
    try:
        esptool.main(['--port', port, '--no-stub', 'flash_id'])
    except SystemExit as e:
        if e.code != 0:
            raise BoardDetectionError(
                f"No se pudo conectar al puerto {port}. "
                "Comprueba el cable y que no esté abierto en otro programa."
            )
    finally:
        sys.stdout = old_stdout
        sys.stderr = old_stderr

    output = buf.getvalue()
    for line in output.splitlines():
        if 'Detected flash size' in line:
            return line.split(':')[-1].strip()

    raise BoardDetectionError(
        "No se pudo detectar el tamaño de flash. "
        "Comprueba que la placa esté conectada correctamente."
    )
