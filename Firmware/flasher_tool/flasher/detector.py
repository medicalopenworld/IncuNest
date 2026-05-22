import io
import sys
from enum import Enum
from typing import Optional

import esptool
import serial.tools.list_ports


class Board(Enum):
    MOTHERBOARD = "motherBoard"
    DISPLAY_HMI = "Display HMI"


class BoardDetectionError(Exception):
    pass


# USB fingerprint per board: vid required, pid optional for disambiguation
BOARD_VID_PID: dict[Board, dict] = {
    Board.MOTHERBOARD: {'vid': 0x303A},             # ESP32-S3 native USB
    Board.DISPLAY_HMI: {'vid': 0x1A86, 'pid': 0x7522},  # CH340K
}

_FLASH_SIZE_MAP = {
    '8MB': Board.MOTHERBOARD,
    '16MB': Board.DISPLAY_HMI,
}


def find_all_board_ports() -> dict[Board, str]:
    """Scan COM ports and return {Board: device} for every known board found."""
    result: dict[Board, str] = {}
    for port_info in serial.tools.list_ports.comports():
        board = board_from_vid_pid(port_info.vid, port_info.pid)
        if board is not None and board not in result:
            result[board] = port_info.device
    return result


def board_from_vid_pid(vid: Optional[int], pid: Optional[int]) -> Optional[Board]:
    """Return the Board type matching the given VID/PID, or None if unknown."""
    for board, criteria in BOARD_VID_PID.items():
        if vid == criteria['vid']:
            if 'pid' not in criteria or pid == criteria['pid']:
                return board
    return None


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
        esptool.main(['--port', port, '--no-stub', 'flash-id'])
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
