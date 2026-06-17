import socket
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from concurrent.futures import TimeoutError as FuturesTimeout
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Optional

import requests
from requests.auth import HTTPBasicAuth

from detector import Board

_BOARD_FOLDER: dict[Board, str] = {
    Board.MOTHERBOARD: 'motherboard',
    Board.DISPLAY_HMI: 'display_hmi',
}

_AUTH_SEQUENCES: dict[Board, list] = {
    Board.DISPLAY_HMI: [
        HTTPBasicAuth('incunestadmin', 'savinglives'),
        HTTPBasicAuth('in3admin', 'savinglives'),
    ],
    Board.MOTHERBOARD: [
        HTTPBasicAuth('incunestadmin', 'savinglives'),
        HTTPBasicAuth('in3admin', 'savinglives'),
        None,
    ],
}

_MDNS_SERVICE = '_http._tcp.local.'


@dataclass
class WifiBoard:
    ip: str
    board: Board
    fw_version: str
    hostname: str = ''


def _board_from_hostname(hostname: str) -> Optional[Board]:
    """Parse an mDNS hostname (with or without .local) into a Board type."""
    name = hostname.split('.')[0]
    if name.startswith('IncuNest_Display'):
        return Board.DISPLAY_HMI
    if name.startswith('IncuNest'):
        return Board.MOTHERBOARD
    return None


def flash_board_wifi(
    ip: str,
    board: Board,
    firmware_base: Path,
    progress_cb: Callable[[str, Optional[int]], None],
    timeout_s: float = 120.0,
) -> None:
    """Flash firmware.bin to an ESP32 via HTTP OTA POST /update.

    Tries auth credentials in order: incunestadmin, in3admin, None (MB only).
    Raises RuntimeError on auth failure, FAIL response, or unexpected response.
    """
    fw_path = firmware_base / _BOARD_FOLDER[board] / 'firmware.bin'
    if not fw_path.exists():
        raise FileNotFoundError(f'firmware.bin no encontrado: {fw_path}')

    progress_cb('Conectando…', None)

    for auth in _AUTH_SEQUENCES[board]:
        with open(fw_path, 'rb') as f:
            resp = requests.post(
                f'http://{ip}/update',
                files={'update': ('firmware.bin', f, 'application/octet-stream')},
                auth=auth,
                timeout=timeout_s,
            )
        if resp.status_code == 401:
            continue
        body = resp.text.strip()
        if body == 'FAIL':
            raise RuntimeError(f'El dispositivo reportó FAIL ({ip})')
        if body != 'OK':
            raise RuntimeError(f'Respuesta inesperada ({ip}): {body[:50]}')
        progress_cb('', 99)
        return

    raise RuntimeError(f'Autenticación fallida en {ip} — ninguna credencial funcionó')
