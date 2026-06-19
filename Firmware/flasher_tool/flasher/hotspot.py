"""Windows Wi-Fi hotspot management via netsh wlan hosted network."""
import subprocess
import sys

HOTSPOT_SSID = 'in3wifi'
HOTSPOT_PASSWORD = '12345678'


def _netsh(*args: str) -> tuple[bool, str]:
    result = subprocess.run(
        ['netsh', 'wlan'] + list(args),
        capture_output=True, text=True, encoding='utf-8', errors='replace',
        creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0,
    )
    return result.returncode == 0, (result.stdout + result.stderr).strip()


def is_supported() -> bool:
    """Return True if this Windows machine supports hosted networks."""
    if sys.platform != 'win32':
        return False
    ok, out = _netsh('show', 'hostednetwork')
    return ok


def start_hotspot(ssid: str = HOTSPOT_SSID, password: str = HOTSPOT_PASSWORD) -> None:
    """Configure and start the Windows hosted-network hotspot.

    Raises RuntimeError with a human-readable message on failure (missing
    admin rights, unsupported adapter, etc.).
    """
    ok, out = _netsh('set', 'hostednetwork', 'mode=allow', f'ssid={ssid}', f'key={password}')
    if not ok:
        if 'acceso' in out.lower() or 'access' in out.lower() or 'denied' in out.lower():
            raise RuntimeError(
                "Se requieren permisos de administrador.\n"
                "Ejecuta el flasher como administrador e inténtalo de nuevo."
            )
        raise RuntimeError(f"No se pudo configurar el hotspot:\n{out}")

    ok, out = _netsh('start', 'hostednetwork')
    if not ok:
        if 'no se puede' in out.lower() or 'cannot' in out.lower() or 'unable' in out.lower():
            raise RuntimeError(
                "El adaptador WiFi no soporta hosted network.\n"
                "Activa el hotspot manualmente en Configuración → Red → Zona Wi-Fi."
            )
        raise RuntimeError(f"No se pudo iniciar el hotspot:\n{out}")


def stop_hotspot() -> None:
    """Stop the hosted-network hotspot (best effort, no exception raised)."""
    _netsh('stop', 'hostednetwork')
