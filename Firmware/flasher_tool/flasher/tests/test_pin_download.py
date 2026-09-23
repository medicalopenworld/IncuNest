"""La descarga de la ultima release respeta el ancla de data/firmware/.

Este es el fallo caro, y no se ve: manifest.json con "release": null hace que
check_update_available diga que hay actualizacion en cada arranque con red, y
download_latest sobrescribe data/firmware/ entera. Una carpeta preparada a
mano para una tanda --el build de fabrica con la activacion de SIM, o un
firmware recien compilado y aun sin publicar-- se pierde ahi, sin ningun aviso
en pantalla y sin que el flasheo de ningun error.
"""
import hashlib
import json

import pytest

import updater
from updater import PIN_MARKER, download_latest

_RELEASE_BYTES = b'FIRMWARE DE LA RELEASE'
_LOCAL_BYTES = b'FIRMWARE LOCAL PUESTO A MANO'


@pytest.fixture
def firmware_base(tmp_path):
    for folder in ('motherboard', 'display_hmi', 'sensorboard'):
        (tmp_path / folder).mkdir()
        (tmp_path / folder / 'firmware.bin').write_bytes(_LOCAL_BYTES)
    return tmp_path


@pytest.fixture
def fake_release(monkeypatch):
    """Una release que trae TODOS los assets, con checksums validos."""
    digest = hashlib.sha256(_RELEASE_BYTES).hexdigest()
    manifest = {
        "release": "v99.0",
        "files": {name: digest for name in updater._ASSET_MAP},
    }
    manifest_bytes = json.dumps(manifest).encode()

    assets = [{"name": "manifest.json", "browser_download_url": "http://x/manifest.json"}]
    assets += [
        {"name": name, "browser_download_url": f"http://x/{name}"}
        for name in updater._ASSET_MAP
    ]

    monkeypatch.setattr(updater, '_fetch_json', lambda url: {"tag_name": "v99.0", "assets": assets})
    monkeypatch.setattr(
        updater, '_fetch_bytes',
        lambda url, progress_cb=None, asset_name="":
            manifest_bytes if url.endswith('manifest.json') else _RELEASE_BYTES,
    )
    return manifest


def test_sin_ancla_la_release_sobrescribe_todo(firmware_base, fake_release):
    assert download_latest(firmware_base) is True
    for folder in ('motherboard', 'display_hmi', 'sensorboard'):
        assert (firmware_base / folder / 'firmware.bin').read_bytes() == _RELEASE_BYTES


def test_la_carpeta_anclada_no_se_toca(firmware_base, fake_release):
    (firmware_base / 'motherboard' / PIN_MARKER).write_text(
        'env=IncuNest_V18_factory\n', encoding='utf-8')

    assert download_latest(firmware_base) is True

    assert (firmware_base / 'motherboard' / 'firmware.bin').read_bytes() == _LOCAL_BYTES
    # Las demas si se actualizan: el ancla es por carpeta, no un interruptor
    # general que deje la maquina congelada sin que nadie se entere.
    assert (firmware_base / 'display_hmi' / 'firmware.bin').read_bytes() == _RELEASE_BYTES
    assert (firmware_base / 'sensorboard' / 'firmware.bin').read_bytes() == _RELEASE_BYTES


def test_se_pueden_anclar_varias_carpetas(firmware_base, fake_release):
    (firmware_base / 'motherboard' / PIN_MARKER).write_text('env=x\n', encoding='utf-8')
    (firmware_base / 'display_hmi' / PIN_MARKER).write_text('env=main\n', encoding='utf-8')

    assert download_latest(firmware_base) is True

    assert (firmware_base / 'motherboard' / 'firmware.bin').read_bytes() == _LOCAL_BYTES
    assert (firmware_base / 'display_hmi' / 'firmware.bin').read_bytes() == _LOCAL_BYTES
    assert (firmware_base / 'sensorboard' / 'firmware.bin').read_bytes() == _RELEASE_BYTES


# El manifiesto no puede afirmar un sha256 que no se ha descargado: de la
# carpeta anclada no se sabe nada, asi que su checksum se quita y se deja
# constancia del ancla. El tag si se guarda, o el flasher reintentaria la
# descarga en cada arranque.
def test_el_manifiesto_no_miente_sobre_la_carpeta_anclada(firmware_base, fake_release):
    (firmware_base / 'motherboard' / PIN_MARKER).write_text(
        'env=IncuNest_V18_factory\n', encoding='utf-8')

    download_latest(firmware_base)
    written = json.loads((firmware_base / 'manifest.json').read_text(encoding='utf-8'))

    assert written['release'] == 'v99.0'
    assert written['pinned'] == {'motherboard': 'IncuNest_V18_factory'}
    assert not any(name.startswith('motherboard_') for name in written['files'])
    assert 'display_hmi_firmware.bin' in written['files']
