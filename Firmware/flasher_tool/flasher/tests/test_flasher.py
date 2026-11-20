import pytest
from pathlib import Path
from unittest.mock import patch

from flasher import (
    flash_board, missing_files, write_initial_ota_data, _ProgressTracker,
)
from detector import Board, MB_HW_V18, MB_HW_V19


@pytest.fixture
def firmware_dir(tmp_path):
    for folder, files in [
        ('motherboard', ['bootloader.bin', 'partitions.bin', 'ota_data_initial.bin',
                         'firmware.bin']),
        ('motherboard_hw19', ['bootloader.bin', 'partitions.bin', 'ota_data_initial.bin',
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
        flash_board('COM3', Board.MOTHERBOARD, firmware_dir, lambda m, p: None, hw=MB_HW_V18)

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
        flash_board('COM7', Board.MOTHERBOARD, firmware_dir, lambda m, p: None, hw=MB_HW_V18)

    assert 'COM7' in captured


def test_progress_callback_receives_percentage(firmware_dir):
    progress = []

    def fake_main(args):
        print("Writing at 0x00010000... (50 %)")

    with patch('flasher.esptool.main', fake_main):
        flash_board('COM3', Board.MOTHERBOARD, firmware_dir,
                    lambda msg, pct: progress.append((msg, pct)), hw=MB_HW_V18)

    assert any(pct == 50 for _, pct in progress)


def test_raises_file_not_found_for_missing_binary(tmp_path):
    (tmp_path / 'motherboard').mkdir()  # empty — no bin files

    with pytest.raises(FileNotFoundError, match='bootloader.bin'):
        flash_board('COM3', Board.MOTHERBOARD, tmp_path, lambda m, p: None, hw=MB_HW_V18)


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

    assert set(written) == {'motherboard', 'motherboard_hw19', 'display_hmi', 'sensorboard'}
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
    assert 'firmware.bin' in gaps['motherboard_hw19']


def test_missing_files_empty_when_everything_is_there(firmware_dir):
    assert missing_files(firmware_dir) == {}


# --- Una carpeta de firmware por revision de motherBoard -------------------
# V18 sigue en motherboard/ (compatible con las releases y los datos que ya
# hay); V19 va en motherboard_hw19/. La revision la decide el hardware
# (detector.detect_motherboard_hw), no lo que haya compilado en el PC.

def _flash_capture(firmware_dir, hw):
    captured = []
    with patch('flasher.esptool.main', lambda args: captured.extend(args)):
        flash_board('COM3', Board.MOTHERBOARD, firmware_dir, lambda m, p: None, hw=hw)
    return captured


def test_v19_flashes_from_its_own_folder(firmware_dir):
    captured = _flash_capture(firmware_dir, MB_HW_V19)
    app = captured[captured.index('0x10000') + 1]
    assert Path(app).parent.name == 'motherboard_hw19'


def test_v18_flashes_from_motherboard_folder(firmware_dir):
    captured = _flash_capture(firmware_dir, MB_HW_V18)
    app = captured[captured.index('0x10000') + 1]
    assert Path(app).parent.name == 'motherboard'


def test_motherboard_without_revision_is_refused(firmware_dir):
    # Sin revision no hay forma de saber que binario toca: no se adivina.
    with patch('flasher.esptool.main', lambda args: None):
        with pytest.raises(ValueError):
            flash_board('COM3', Board.MOTHERBOARD, firmware_dir, lambda m, p: None)


def test_v19_missing_binaries_do_not_fall_back_to_v18(tmp_path):
    (tmp_path / 'motherboard').mkdir()
    for f in ('bootloader.bin', 'partitions.bin', 'ota_data_initial.bin', 'firmware.bin'):
        (tmp_path / 'motherboard' / f).write_bytes(bytes([0xFF]) * 16)
    with pytest.raises(FileNotFoundError, match='motherboard_hw19'):
        flash_board('COM3', Board.MOTHERBOARD, tmp_path, lambda m, p: None, hw=MB_HW_V19)


# --- Un error de esptool no puede acabar en "flasheado con exito" ----------
# En esptool 5 el teardown de cada comando (reset_chip) corre tambien cuando el
# comando falla, y siempre imprime "Hard resetting via RTS pin...". El flasher
# tomaba esa palabra como prueba de que la escritura habia terminado y se
# tragaba la excepcion: un fallo despues de conectar (stub, cambio de baudios,
# escritura) salia en verde con la placa sin flashear.

class _FatalError(Exception):
    pass


def _esptool_falla_tras_conectar(args):
    print("Connecting....")
    print("Connected to ESP32-S3 on COM3:")
    print("Hard resetting via RTS pin...")
    raise _FatalError("Failed to enter flash download mode")


def test_fallo_tras_conectar_no_es_exito(firmware_dir):
    with patch('flasher.esptool.main', _esptool_falla_tras_conectar):
        with pytest.raises(RuntimeError, match='flash download mode'):
            flash_board('COM3', Board.MOTHERBOARD, firmware_dir,
                        lambda m, p: None, hw=MB_HW_V19)


def test_fallo_a_mitad_de_escritura_no_es_exito(firmware_dir):
    def fake_main(args):
        print("Hash of data verified.")   # bootloader
        print("Hash of data verified.")   # particiones
        print("Hard resetting via RTS pin...")
        raise _FatalError("Serial data stream stopped")

    with patch('flasher.esptool.main', fake_main):
        with pytest.raises(RuntimeError, match='stream stopped'):
            flash_board('COM3', Board.MOTHERBOARD, firmware_dir,
                        lambda m, p: None, hw=MB_HW_V18)


def test_puerto_que_desaparece_en_el_reset_final_si_es_exito(firmware_dir):
    # Con USB nativo el puerto puede desaparecer en el reset final, cuando
    # todo ya esta escrito y verificado: eso si es un flasheo correcto.
    def fake_main(args):
        n = len([a for a in args if a.startswith('0x')])
        for _ in range(n):
            print("Hash of data verified.")
        print("Hard resetting via RTS pin...")
        raise OSError("could not open port")

    with patch('flasher.esptool.main', fake_main):
        flash_board('COM3', Board.MOTHERBOARD, firmware_dir,
                    lambda m, p: None, hw=MB_HW_V19)


# Si la placa deja de responder a mitad de escritura (Lost connection / Write
# timeout), el error dice que hacer. En la motherBoard 373 (2026-11-19) la
# causa era lo que tenia conectado ademas del USB: con todo desconectado
# flasheo bien. La placa queda colgada hasta volver a ponerla en boot.
def test_conexion_perdida_explica_que_hacer(firmware_dir):
    def fake_main(args):
        print("Compressed 14032 bytes to 9771...")
        print("\nLost connection, retrying...")
        print("Hard resetting via RTS pin...")
        raise _FatalError("Write timeout")

    with patch('flasher.esptool.main', fake_main):
        with pytest.raises(RuntimeError) as exc:
            flash_board('COM3', Board.MOTHERBOARD, firmware_dir,
                        lambda m, p: None, hw=MB_HW_V18)
    msg = str(exc.value)
    assert 'Write timeout' in msg
    assert 'desconecta' in msg.lower()
    assert 'boot' in msg.lower()


def test_otro_error_no_lleva_la_pista(firmware_dir):
    with patch('flasher.esptool.main', _esptool_falla_tras_conectar):
        with pytest.raises(RuntimeError) as exc:
            flash_board('COM3', Board.MOTHERBOARD, firmware_dir,
                        lambda m, p: None, hw=MB_HW_V19)
    assert 'desconecta' not in str(exc.value).lower()
