import json
import hashlib
import urllib.request
from pathlib import Path
from typing import Callable, Optional

_REPO = "medicalopenworld/IncuNest"
_API_URL = f"https://api.github.com/repos/{_REPO}/releases/latest"
_HEADERS = {"User-Agent": "IncuNest-Flasher/1.0"}

# GitHub release asset name → local path relative to firmware_base.
#
# Tiene que cubrir todo lo que flasher.py::_BOARD_FILES espera, menos los
# ota_data_initial.bin, que se generan en local (flasher.write_initial_ota_data)
# y por eso no hacen falta como asset. Un fichero que falte aqui NO da error de
# flasheo: se queda la copia vieja en data/firmware/ y la placa sale de fabrica
# con una mezcla de versiones. Hasta 2026-09 faltaba la SensorBoard entera y la
# imagen SPIFFS del HMI.
_ASSET_MAP = {
    "motherboard_bootloader.bin":       "motherboard/bootloader.bin",
    "motherboard_partitions.bin":       "motherboard/partitions.bin",
    "motherboard_firmware.bin":         "motherboard/firmware.bin",
    "display_hmi_bootloader.bin":       "display_hmi/bootloader.bin",
    "display_hmi_partitions.bin":       "display_hmi/partitions.bin",
    "display_hmi_firmware.bin":         "display_hmi/firmware.bin",
    "display_hmi_ota_data_initial.bin": "display_hmi/ota_data_initial.bin",
    "display_hmi_spiffs.bin":           "display_hmi/spiffs.bin",
    "sensorboard_bootloader.bin":       "sensorboard/bootloader.bin",
    "sensorboard_partitions.bin":       "sensorboard/partitions.bin",
    "sensorboard_firmware.bin":         "sensorboard/firmware.bin",
    "sensorboard_ota_data_initial.bin": "sensorboard/ota_data_initial.bin",
}


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _fetch_json(url: str) -> Optional[dict]:
    try:
        req = urllib.request.Request(url, headers=_HEADERS)
        with urllib.request.urlopen(req, timeout=10) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except Exception:
        return None


def _fetch_bytes(url: str,
                 progress_cb: Optional[Callable] = None,
                 asset_name: str = "") -> Optional[bytes]:
    try:
        req = urllib.request.Request(url, headers=_HEADERS)
        with urllib.request.urlopen(req, timeout=60) as resp:
            total = int(resp.headers.get("Content-Length", 0))
            chunks: list[bytes] = []
            received = 0
            while True:
                chunk = resp.read(16384)
                if not chunk:
                    break
                chunks.append(chunk)
                received += len(chunk)
                if progress_cb:
                    progress_cb(asset_name, received, total)
            return b"".join(chunks)
    except Exception:
        return None


def get_local_release(firmware_base: Path) -> Optional[str]:
    path = firmware_base / "manifest.json"
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8")).get("release")
    except Exception:
        return None


def check_update_available(firmware_base: Path) -> tuple[bool, Optional[str]]:
    """Returns (update_available, latest_tag). latest_tag is None on network error."""
    info = _fetch_json(_API_URL)
    if not info:
        return False, None
    latest = info.get("tag_name")
    local = get_local_release(firmware_base)
    return (local != latest), latest


def download_latest(firmware_base: Path,
                    progress_cb: Optional[Callable[[str, int, int], None]] = None,
                    log_cb: Optional[Callable[[str, str], None]] = None) -> bool:
    """Download all firmware assets from the latest GitHub release.

    progress_cb(asset_name, downloaded_bytes, total_bytes)
    log_cb(mensaje, nivel) — para que un asset ausente o con hash malo salga en
    el registro. Antes se saltaba en silencio, que es como el hueco de la
    SensorBoard aguanto sin que nadie lo viera.
    Returns True on success.
    """
    def _log(msg: str, level: str = 'info') -> None:
        if log_cb:
            log_cb(msg, level)

    info = _fetch_json(_API_URL)
    if not info:
        return False

    assets_by_name = {a["name"]: a["browser_download_url"] for a in info.get("assets", [])}

    manifest_url = assets_by_name.get("manifest.json")
    if not manifest_url:
        return False
    manifest_data = _fetch_bytes(manifest_url)
    if not manifest_data:
        return False
    try:
        manifest = json.loads(manifest_data.decode("utf-8"))
    except Exception:
        return False

    checksums: dict[str, str] = manifest.get("files", {})

    skipped: list[str] = []

    for asset_name, local_rel in _ASSET_MAP.items():
        url = assets_by_name.get(asset_name)
        if not url:
            skipped.append(asset_name)
            continue
        data = _fetch_bytes(url, progress_cb, asset_name)
        if data is None:
            _log(f"Error al descargar {asset_name}.", 'error')
            return False
        expected = checksums.get(asset_name)
        if expected and _sha256(data) != expected:
            _log(f"Checksum incorrecto en {asset_name}.", 'error')
            return False
        dest = firmware_base / Path(local_rel)
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)

    if skipped:
        _log(
            "La release no trae " + ", ".join(skipped)
            + " — se mantiene la copia local de esos ficheros.",
            'error',
        )

    (firmware_base / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8"
    )
    return True
