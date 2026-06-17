import socket
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from concurrent.futures import TimeoutError as FuturesTimeout
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Optional

import requests
from requests.auth import HTTPBasicAuth
from requests_toolbelt import MultipartEncoder, MultipartEncoderMonitor

from detector import Board

_BOARD_FOLDER: dict[Board, str] = {
    Board.MOTHERBOARD: 'motherboard',
    Board.DISPLAY_HMI: 'display_hmi',
}

# Credentials are intentionally embedded: this is a factory-floor tool used on a
# local network. The same credentials are compiled into the ESP32 firmware.
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
    sn: Optional[int] = None


def _board_from_hostname(hostname: str) -> Optional[Board]:
    """Parse an mDNS hostname (with or without .local) into a Board type."""
    name = hostname.split('.')[0]
    if name.startswith('IncuNest_Display'):
        return Board.DISPLAY_HMI
    if name.startswith('IncuNest'):
        return Board.MOTHERBOARD
    return None


def _sn_from_hostname(hostname: str) -> Optional[int]:
    """Extract serial number from hostname, e.g. 'IncuNest_Display-7' or 'IncuNest-42.local' → 7 / 42."""
    try:
        name = hostname.split('.')[0]  # strip .local
        return int(name.split('-')[-1])
    except (ValueError, IndexError):
        return None


def _resolve_hostname(ip: str) -> str:
    """Reverse-lookup hostname via OS mDNS resolver; returns '' on failure.

    Runs in a daemon thread so a slow resolver does not block the caller.
    """
    result: list[str] = ['']

    def _work() -> None:
        try:
            result[0] = socket.gethostbyaddr(ip)[0]
        except Exception:
            pass

    t = threading.Thread(target=_work, daemon=True)
    t.start()
    t.join(timeout=1.0)
    return result[0]


def flash_board_wifi(
    ip: str,
    board: Board,
    firmware_base: Path,
    progress_cb: Callable[[str, Optional[int]], None],
    timeout_s: float = 120.0,
) -> None:
    """Flash firmware.bin to an ESP32 via HTTP OTA POST /update.

    Uses MultipartEncoderMonitor for real network-level progress (0–98%).
    Tries auth credentials in order: incunestadmin, in3admin, None (MB only).
    Raises RuntimeError on auth failure, FAIL response, or unexpected response.
    """
    fw_path = firmware_base / _BOARD_FOLDER[board] / 'firmware.bin'
    if not fw_path.exists():
        raise FileNotFoundError(f'firmware.bin no encontrado: {fw_path}')

    progress_cb('Conectando…', None)

    for auth in _AUTH_SEQUENCES[board]:
        with open(fw_path, 'rb') as fw_file:
            encoder = MultipartEncoder(
                fields={'update': ('firmware.bin', fw_file, 'application/octet-stream')}
            )
            monitor = MultipartEncoderMonitor(
                encoder,
                lambda m: progress_cb('', int(m.bytes_read * 98 / m.len)),
            )
            resp = requests.post(
                f'http://{ip}/update',
                data=monitor,
                headers={'Content-Type': monitor.content_type},
                auth=auth,
                timeout=timeout_s,
            )
        if resp.status_code == 401:
            continue
        try:
            body = resp.text.strip()
        except Exception:
            body = ''  # ESP32 reset TCP mid-response after restart — treat 200 as success
        if body == 'FAIL':
            raise RuntimeError(f'El dispositivo reportó FAIL ({ip})')
        if body not in ('OK', ''):
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


def _identify_board_type(ip: str, timeout: float = 0.5) -> Optional[Board]:
    """Return board type by probing /get_freq (Display HMI only).

    Returns None when the response is ambiguous (connection error or unexpected
    status), so _probe_ip can skip the host rather than risk flashing wrong firmware.
    """
    try:
        resp = requests.get(f'http://{ip}/get_freq', timeout=timeout)
        if resp.status_code == 200:
            return Board.DISPLAY_HMI
        if resp.status_code == 404:
            return Board.MOTHERBOARD
    except Exception:
        pass
    return None


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
        WifiBoard(
            ip=ip, board=board,
            fw_version=_get_fw_version(ip),
            hostname=hostname,
            sn=_sn_from_hostname(hostname),
        )
        for ip, board, hostname in found
    ]


def _local_subnets() -> list[str]:
    """Return /24 subnet prefixes for every local non-loopback IPv4 interface.

    Uses ifaddr to enumerate all adapters so the hotspot interface (e.g.
    192.168.137.x) is included even when the PC also has internet access on a
    different adapter.  Falls back to the socket routing trick, then to the
    Windows hotspot default '192.168.137'.
    """
    try:
        import ifaddr
        seen: list[str] = []
        for adapter in ifaddr.get_adapters():
            for ip_info in adapter.ips:
                if not isinstance(ip_info.ip, str):
                    continue  # skip IPv6 tuples
                if ip_info.ip.startswith('127.') or ip_info.ip == '0.0.0.0':
                    continue
                base = ip_info.ip.rsplit('.', 1)[0]
                if base not in seen:
                    seen.append(base)
        if seen:
            return seen
    except Exception:
        pass
    # socket fallback: finds only the internet-facing interface
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.connect(('8.8.8.8', 80))
            ip = s.getsockname()[0]
        finally:
            s.close()
        if not ip.startswith('127.'):
            return [ip.rsplit('.', 1)[0]]
    except Exception:
        pass
    return ['192.168.137']


def _probe_ip(ip: str) -> Optional[WifiBoard]:
    """Return a WifiBoard if ip hosts an IncuNest device, else None."""
    try:
        resp = requests.get(f'http://{ip}/get_fw_version', timeout=0.3)
        if resp.status_code != 200:
            return None
        data = resp.json()
        fw_version = data.get('version', '?')
        sn: Optional[int] = data.get('sn', None)
        board = _identify_board_type(ip)
        if board is None:
            return None
        if sn is None:
            # Firmware without sn field — try OS mDNS reverse lookup
            hostname = _resolve_hostname(ip)
            sn = _sn_from_hostname(hostname)
        return WifiBoard(ip=ip, board=board, fw_version=fw_version, sn=sn)
    except Exception:
        return None


def _discover_subnet(timeout_s: float) -> list[WifiBoard]:
    """Scan all 254 hosts across every local /24 subnet concurrently."""
    subnets = _local_subnets()
    ips = [f'{base}.{i}' for base in subnets for i in range(1, 255)]
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
            for f in futures:
                f.cancel()

    return results


def discover_boards(timeout_s: float = 5.0) -> list[WifiBoard]:
    """Discover IncuNest boards via mDNS + subnet scan, merged and deduplicated.

    Both scans always run: mDNS may find devices with the new firmware while older
    devices (no MDNS.addService) are only reachable via subnet scan.
    """
    mdns_results: list[WifiBoard] = []
    try:
        mdns_results = _discover_mdns(timeout_s / 2)
    except Exception:
        pass

    subnet_results = _discover_subnet(timeout_s / 2)

    # Prefer mDNS entries (carry hostname + sn); fill in IPs only seen by subnet scan
    seen_ips = {wb.ip for wb in mdns_results}
    combined = list(mdns_results)
    for wb in subnet_results:
        if wb.ip not in seen_ips:
            combined.append(wb)
    return combined
