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


def _get_fw_version(ip: str, timeout: float = 2.0) -> str:
    """GET /get_fw_version and return the version string, or '?' on any failure."""
    try:
        resp = requests.get(f'http://{ip}/get_fw_version', timeout=timeout)
        if resp.status_code == 200:
            return resp.json().get('version', '?')
    except Exception:
        pass
    return '?'


def _identify_board_type(ip: str, timeout: float = 0.5) -> Board:
    """Determine board type by probing /get_freq (Display HMI only endpoint)."""
    try:
        resp = requests.get(f'http://{ip}/get_freq', timeout=timeout)
        if resp.status_code == 200:
            return Board.DISPLAY_HMI
    except Exception:
        pass
    return Board.MOTHERBOARD


def _discover_mdns(timeout_s: float) -> list[WifiBoard]:
    """Browse mDNS _http._tcp.local for IncuNest services."""
    from zeroconf import Zeroconf, ServiceBrowser, ServiceStateChange

    found: list[tuple[str, Board, str]] = []
    lock = threading.Lock()

    def on_change(zeroconf, service_type, name, state_change):
        if state_change is not ServiceStateChange.Added:
            return
        hostname = name.split('.')[0]
        board = _board_from_hostname(hostname)
        if board is None:
            return
        info = zeroconf.get_service_info(service_type, name)
        if info and info.addresses:
            ip = socket.inet_ntoa(info.addresses[0])
            with lock:
                if not any(e[0] == ip for e in found):
                    found.append((ip, board, hostname))

    zc = Zeroconf()
    try:
        browser = ServiceBrowser(zc, _MDNS_SERVICE, handlers=[on_change])
        time.sleep(timeout_s)
        browser.cancel()
    finally:
        zc.close()

    return [
        WifiBoard(ip=ip, board=board, fw_version=_get_fw_version(ip), hostname=hostname)
        for ip, board, hostname in found
    ]


def _local_subnet() -> str:
    """Return the first two octets + third of the local IP (e.g. '192.168.137').
    Falls back to Windows hotspot default '192.168.137' on failure."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
        s.close()
        if not ip.startswith('127.'):
            return ip.rsplit('.', 1)[0]
    except Exception:
        pass
    return '192.168.137'


def _probe_ip(ip: str) -> Optional[WifiBoard]:
    """Return a WifiBoard if ip hosts an IncuNest device, else None."""
    try:
        resp = requests.get(f'http://{ip}/get_fw_version', timeout=0.3)
        if resp.status_code != 200:
            return None
        fw_version = resp.json().get('version', '?')
        board = _identify_board_type(ip)
        return WifiBoard(ip=ip, board=board, fw_version=fw_version)
    except Exception:
        return None


def _discover_subnet(timeout_s: float) -> list[WifiBoard]:
    """Scan all 254 hosts in the local /24 subnet concurrently."""
    base = _local_subnet()
    ips = [f'{base}.{i}' for i in range(1, 255)]
    results: list[WifiBoard] = []

    with ThreadPoolExecutor(max_workers=50) as ex:
        futures = {ex.submit(_probe_ip, ip) for ip in ips}
        try:
            for future in as_completed(futures, timeout=timeout_s):
                try:
                    wb = future.result()
                    if wb is not None:
                        results.append(wb)
                except Exception:
                    pass
        except FuturesTimeout:
            pass

    return results


def discover_boards(timeout_s: float = 5.0) -> list[WifiBoard]:
    """Discover IncuNest boards: mDNS first, subnet scan as fallback."""
    try:
        mdns_results = _discover_mdns(timeout_s / 2)
    except Exception:
        mdns_results = []

    if mdns_results:
        return mdns_results

    return _discover_subnet(timeout_s / 2)
