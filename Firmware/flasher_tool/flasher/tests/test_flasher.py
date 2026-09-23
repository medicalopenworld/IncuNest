import pytest
from pathlib import Path
from unittest.mock import patch

from flasher import (
    flash_board, missing_files, write_initial_ota_data, _ProgressTracker,
)
from detector import Board


@pytest.fixture
def firmware_dir(tmp_path):
    for folder, files in [
        ('motherboard', ['bootloader.bin', 'partitions.bin', 'ota_data_initial.bin',
                         'firmware.bin']),
        ('display_hmi', ['bootloader.bin', 'partitions.bin', 'ota_data_initial.bin',
                         'firmware.bin']),
        ('sensorboard', ['bootloader.bin', 'partitions.bin', 'ota_data_initial.bin',
                         'firmware.bin']),
    ]:
        d = tmp_path / folder
        d.mkdir()
        for f in files:
            (d / f).write_bytes(b'\xff' * 16)
    return tmp_path


def test_motherboard_flash_includes_correct_addresses(firmware_dir):
    """La motherboard tambien repone otadata, en 0xE000 (ESP32S3_8MB.csv).

    Hasta 2026-09 no lo hacia y el fallo era silencioso: motherBoard/src/tasks/
    Wifi_OTA.cpp hace OTA de verdad, asi que una placa actualizada en campo
    tiene el puntero de arranque en app1. Al reflashear por USB, firmware.bin
    entra en app0 y la placa seguia arrancando la app vieja de app1 — con el
    flasheo dando verde.
    """
    captured = []
    with patch('flasher.esptool.main', lambda args: captured.extend(args)):
        flash_board('COM3', Board.MOTHERBOARD, firmware_dir, lambda m, p: None)

    assert '0x0000' in captured
    assert '0x8000' in captured
    assert '0xE000' in captured
    assert '0x10000' in captured


def test_display_hmi_flash_includes_ota_data_address(firmware_dir):
    captured = []
    with patch('flasher.esptool.main', lambda args: captured.extend(args)):
        flash_board('COM3', Board.DISPLAY_HMI, firmware_dir, lambda m, p: None)

    assert '0x0000' in captured
    assert '0x8000' in captured
    assert '0xE000' in captured
    assert '0x10000' in captured


def test_display_hmi_no_flashea_imagen_spiffs(firmware_dir):
    """El HMI NO escribe la particion del sistema de archivos.

    Se flasheaba una imagen de 6,16 MB en 0xA10000 para llevar
    /heartbeat.mp3, y en esta linea de firmware no la lee nadie: su unico
    consumidor, src/tasks/AudioManager.cpp, esta excluido del build
    (build_src_filter en Display_HMI/platformio.ini) y ni genera objeto.

    El test es al reves que el de antes a proposito. Volver a escribirla no es
    gratis: pisa esa particion en cada reflasheo y obliga a acordarse de
    `pio run -t buildfs`, que un build normal no hace, con un
    FileNotFoundError delante de la placa si se olvida.
    """
    captured = []
    with patch('flasher.esptool.main', lambda args: captured.extend(args)):
        flash_board('COM3', Board.DISPLAY_HMI, firmware_dir, lambda m, p: None)

    assert '0xA10000' not in captured
    assert not any(a.endswith('spiffs.bin') for a in captured)
    # Y lo que si tiene que escribir sigue ahi.
    assert '0x10000' in captured
    assert any(a.endswith('firmware.bin') for a in captured)


def test_flash_passes_port_to_esptool(firmware_dir):
    captured = []
    with patch('flasher.esptool.main', lambda args: captured.extend(args)):
        flash_board('COM7', Board.MOTHERBOARD, firmware_dir, lambda m, p: None)

    assert 'COM7' in captured


def test_progress_callback_receives_percentage(firmware_dir):
    progress = []

    def fake_main(args):
        print("Writing at 0x00010000... (50 %)")

    with patch('flasher.esptool.main', fake_main):
        flash_board('COM3', Board.MOTHERBOARD, firmware_dir,
                    lambda msg, pct: progress.append((msg, pct)))

    assert any(pct == 50 for _, pct in progress)


def test_raises_file_not_found_for_missing_binary(tmp_path):
    (tmp_path / 'motherboard').mkdir()  # empty — no bin files

    with pytest.raises(FileNotFoundError, match='bootloader.bin'):
        flash_board('COM3', Board.MOTHERBOARD, tmp_path, lambda m, p: None)


def test_sensorboard_flash_includes_correct_addresses(firmware_dir):
    """La SensorBoard usa el reparto estandar de IDF: otadata en 0xD000.

    No en 0xE000, que es donde cae en las dos placas Arduino. Escribir
    ota_data_initial.bin importa aqui mas que en el HMI: hasta 2026-09 la tabla
    de la SensorBoard no tenia otadata y ese rango era parte de la NVS, asi que
    en una placa reflasheada quedan bytes viejos justo debajo de la particion
    nueva.
    """
    captured = []
    with patch('flasher.esptool.main', lambda args: captured.extend(args)):
        flash_board('COM3', Board.SENSORBOARD, firmware_dir, lambda m, p: None)

    assert '0x0000' in captured
    assert '0x8000' in captured
    assert '0xD000' in captured
    assert '0x10000' in captured
    assert '0xE000' not in captured


def test_progress_tracker_parses_percent():
    tracker = _ProgressTracker.__new__(_ProgressTracker)
    tracker._segs, tracker._total, tracker._last = [], 0, 0
    assert tracker.parse("Writing at 0x00010000... (67 %)") == 67
    assert tracker.parse("Writing at 0x00010000... (100 %)") == 100


def test_progress_tracker_returns_none_for_non_progress_line():
    tracker = _ProgressTracker.__new__(_ProgressTracker)
    tracker._segs, tracker._total, tracker._last = [], 0, 0
    assert tracker.parse("Uploading stub...") is None
    assert tracker.parse("") is None


def test_write_initial_ota_data_creates_virgin_images(tmp_path):
    """8 KB de 0xFF por placa: "ninguna OTA todavia, arranca el primer slot".

    Se generan en local a proposito. PlatformIO no emite ota_data_initial.bin
    en ninguno de los dos proyectos, asi que el fichero del HMI se venia
    copiando a mano y no habia ninguno para la motherboard.
    """
    written = write_initial_ota_data(tmp_path)

    assert set(written) == {'motherboard', 'display_hmi', 'sensorboard'}
    for folder in written:
        data = (tmp_path / folder / 'ota_data_initial.bin').read_bytes()
        assert len(data) == 0x2000
        assert set(data) == {0xFF}


def test_missing_files_reports_gaps_per_board(tmp_path):
    write_initial_ota_data(tmp_path)
    gaps = missing_files(tmp_path)

    assert 'firmware.bin' in gaps['display_hmi']
    assert 'spiffs.bin' not in gaps['display_hmi']
    assert 'ota_data_initial.bin' not in gaps['display_hmi']
    assert 'firmware.bin' in gaps['motherboard']


def test_missing_files_empty_when_everything_is_there(firmware_dir):
    assert missing_files(firmware_dir) == {}
