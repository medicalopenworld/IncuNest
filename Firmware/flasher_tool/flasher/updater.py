import json
import hashlib
import urllib.request
from pathlib import Path
from typing import Callable, Optional

_REPO = "medicalopenworld/IncuNest"
_API_URL = f"https://api.github.com/repos/{_REPO}/releases/latest"
_HEADERS = {"User-Agent": "IncuNest-Flasher/1.0"}

# GitHub release asset name → local path relative to firmware_base
_ASSET_MAP = {
    "motherboard_bootloader.bin":       "motherboard/bootloader.bin",
    "motherboard_partitions.bin":       "motherboard/partitions.bin",
    "motherboard_firmware.bin":         "motherboard/firmware.bin",
    "display_hmi_bootloader.bin":       "display_hmi/bootloader.bin",
    "display_hmi_partitions.bin":       "display_hmi/partitions.bin",
    "display_hmi_firmware.bin":         "display_hmi/firmware.bin",
    "display_hmi_ota_data_initial.bin": "display_hmi/ota_data_initial.bin",
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
                    progress_cb: Optional[Callable[[str, int, int], None]] = None) -> bool:
    """Download all firmware assets from the latest GitHub release.

    progress_cb(asset_name, downloaded_bytes, total_bytes)
    Returns True on success.
    """
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

    for asset_name, local_rel in _ASSET_MAP.items():
        url = assets_by_name.get(asset_name)
        if not url:
            continue
        data = _fetch_bytes(url, progress_cb, asset_name)
        if data is None:
            return False
        expected = checksums.get(asset_name)
        if expected and _sha256(data) != expected:
            return False
        dest = firmware_base / Path(local_rel)
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)

    (firmware_base / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8"
    )
    return True
