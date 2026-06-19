"""Windows Wi-Fi hotspot management via the Windows Mobile Hotspot API (WinRT)."""
import asyncio
import sys

HOTSPOT_SSID = 'in3wifi'
HOTSPOT_PASSWORD = '12345678'


def is_supported() -> bool:
    return sys.platform == 'win32'


def start_hotspot(ssid: str = HOTSPOT_SSID, password: str = HOTSPOT_PASSWORD) -> None:
    """Start the Windows Mobile Hotspot.

    Raises RuntimeError with a user-readable message on failure.
    """
    if sys.platform != 'win32':
        raise RuntimeError("Hotspot solo disponible en Windows.")
    try:
        asyncio.run(_start_async(ssid, password))
    except RuntimeError:
        raise
    except Exception as exc:
        raise RuntimeError(
            f"No se pudo iniciar el hotspot:\n{exc}\n\n"
            "Activa el hotspot manualmente desde:\n"
            "Configuracion -> Red e Internet -> Zona Wi-Fi\n"
            f"SSID: {ssid}  |  Contrasena: {password}"
        ) from exc


async def _start_async(ssid: str, password: str) -> None:
    from winrt.windows.networking.connectivity import NetworkInformation
    from winrt.windows.networking.networkoperators import (
        NetworkOperatorTetheringManager,
        TetheringOperationStatus,
    )

    prof = NetworkInformation.get_internet_connection_profile()
    if prof is None:
        raise RuntimeError(
            "No se encontro ningún perfil de red.\n\n"
            "Activa el hotspot manualmente desde:\n"
            "Configuracion -> Red e Internet -> Zona Wi-Fi\n"
            f"SSID: {ssid}  |  Contrasena: {password}"
        )

    mgr = NetworkOperatorTetheringManager.create_from_connection_profile(prof)
    cfg = mgr.get_current_access_point_configuration()
    cfg.ssid = ssid
    cfg.passphrase = password
    await mgr.configure_access_point_async(cfg)
    result = await mgr.start_tethering_async()

    if result.status != TetheringOperationStatus.SUCCESS:
        raise RuntimeError(
            f"Windows rechazó el hotspot (status={result.status}).\n\n"
            "Activa el hotspot manualmente desde:\n"
            "Configuracion -> Red e Internet -> Zona Wi-Fi\n"
            f"SSID: {ssid}  |  Contrasena: {password}"
        )


def stop_hotspot() -> None:
    """Stop the hotspot (best effort)."""
    if sys.platform != 'win32':
        return
    try:
        asyncio.run(_stop_async())
    except Exception:
        pass


async def _stop_async() -> None:
    from winrt.windows.networking.connectivity import NetworkInformation
    from winrt.windows.networking.networkoperators import NetworkOperatorTetheringManager

    prof = NetworkInformation.get_internet_connection_profile()
    if prof is None:
        return
    mgr = NetworkOperatorTetheringManager.create_from_connection_profile(prof)
    await mgr.stop_tethering_async()
