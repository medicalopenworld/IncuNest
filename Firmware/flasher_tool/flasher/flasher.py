import io
import os
import re
import tempfile
from pathlib import Path
from typing import Callable, Optional

import esptool

import esptool_io
import nvs_gen
from detector import Board


_BOARD_FOLDER = {
    Board.MOTHERBOARD: 'motherboard',
    Board.DISPLAY_HMI: 'display_hmi',
    Board.SENSORBOARD: 'sensorboard',
}

_BOARD_BEFORE_RESET = {
    Board.MOTHERBOARD: 'default-reset',
    Board.DISPLAY_HMI: 'default-reset',
    Board.SENSORBOARD: 'default-reset',
}

# OJO: estos offsets estan duplicados a mano y tienen que seguir a las tablas
# de particiones de cada placa. Si se mueve una particion en el CSV y no aqui,
# el flasheo no falla — escribe en el sitio equivocado y la placa arranca mal.
# Fuentes de verdad:
#   motherboard   -> motherBoard/partitions/ESP32S3_8MB.csv
#   display_hmi   -> Display_HMI/partitions/hmi_16mb_ota.csv
#   sensorboard   -> SensorBoard_v2/partitions.csv
_BOARD_FILES = {
    # otadata en 0xE000, igual que el HMI (ESP32S3_8MB.csv). Reponerlo no es
    # cosmetico: motherBoard/src/tasks/Wifi_OTA.cpp hace OTA de verdad, asi que
    # una placa que haya recibido una actualizacion tiene el puntero de arranque
    # en app1. Si se reflashea por USB sin tocar otadata, firmware.bin cae en
    # app0 y la placa sigue arrancando la app vieja de app1 — parece un flasheo
    # correcto y no lo es. 8 KB de 0xFF significan "ninguna OTA todavia, arranca
    # el primer slot".
    Board.MOTHERBOARD: [
        ('0x0000', 'bootloader.bin'),
        ('0x8000', 'partitions.bin'),
        ('0xE000', 'ota_data_initial.bin'),
        ('0x10000', 'firmware.bin'),
    ],
    # spiffs.bin lleva /heartbeat.mp3, el unico fichero que el firmware lee del
    # filesystem (src/tasks/AudioManager.cpp). Hasta 2026-09 no se flasheaba
    # ninguna imagen SPIFFS: una unidad recien salida de fabrica arrancaba con
    # la particion vacia y sin sonido de latido, y el aviso quedaba en un
    # Serial.println que nadie lee en produccion ("NOT FOUND. Run 'Upload File
    # System Image'"). La imagen ocupa toda la particion pero va casi entera a
    # 0xFF, asi que con --compress el coste real de escribirla es despreciable.
    Board.DISPLAY_HMI: [
        ('0x0000', 'bootloader.bin'),
        ('0x8000', 'partitions.bin'),
        ('0xE000', 'ota_data_initial.bin'),
        ('0x10000', 'firmware.bin'),
        ('0xA10000', 'spiffs.bin'),
    ],
    # ESP-IDF native layout (bootloader offset 0x0, partition table at
    # 0x8000, primera app en 0x10000 — see SensorBoard_v2/partitions.csv).
    # otadata va en 0xD000, no en 0xE000 como en las placas Arduino: la tabla de
    # la SensorBoard usa el reparto estandar de IDF (nvs de 16K en 0x9000).
    Board.SENSORBOARD: [
        ('0x0000', 'bootloader.bin'),
        ('0x8000', 'partitions.bin'),
        ('0xD000', 'ota_data_initial.bin'),
        ('0x10000', 'firmware.bin'),
    ],
}


# Una imagen de otadata "virgen" son 8 KB de 0xFF: ninguna OTA todavia, el
# bootloader arranca el primer slot de app.
_OTA_DATA_SIZE = 0x2000
_OTA_DATA_NAME = 'ota_data_initial.bin'


def required_files(board: Board) -> list[str]:
    """Ficheros que flash_board espera en la carpeta de la placa."""
    return [fname for _, fname in _BOARD_FILES[board]]


def missing_files(firmware_base: Path) -> dict[str, list[str]]:
    """Devuelve {carpeta → ficheros que faltan} para las tres placas.

    Sirve para que un hueco salga en el registro al refrescar los binarios y no
    en un FileNotFoundError delante de la placa, en fabrica.
    """
    out: dict[str, list[str]] = {}
    for board, folder in _BOARD_FOLDER.items():
        absent = [
            fname for fname in required_files(board)
            if not (firmware_base / folder / fname).is_file()
        ]
        if absent:
            out[folder] = absent
    return out


def write_initial_ota_data(firmware_base: Path) -> list[str]:
    """Escribe los ota_data_initial.bin que la secuencia de flasheo espera.

    ESP-IDF genera el suyo en build/, pero PlatformIO no genera ninguno, asi que
    en el HMI el fichero se venia copiando a mano. Como el contenido es una
    constante, se escribe aqui en vez de depender de que alguien lo copie o de
    que venga en la release de GitHub.

    Devuelve las carpetas escritas.
    """
    written: list[str] = []
    for board, folder in _BOARD_FOLDER.items():
        if _OTA_DATA_NAME not in required_files(board):
            continue
        dest = firmware_base / folder / _OTA_DATA_NAME
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(b'\xff' * _OTA_DATA_SIZE)
        written.append(folder)
    return written


class _ProgressTracker:
    """Parse esptool output for progress percentage.

    Primary: match esptool's '(N %)' format.
    Fallback: estimate from 'Writing at 0x...' addresses when esptool
    suppresses percentage output (e.g. when rich detects non-TTY stdout).
    """

    def __init__(self, file_pairs: list[tuple[str, str]], folder: Path) -> None:
        segs: list[tuple[int, int]] = []
        for addr_str, fname in file_pairs:
            size = (folder / fname).stat().st_size
            start = int(addr_str, 16)
            segs.append((start, start + size))
        segs.sort()
        self._segs = segs
        self._total = sum(e - s for s, e in segs)
        self._last = 0

    def parse(self, text: str) -> Optional[int]:
        m = re.search(r'\((\d+)\s*%\)', text)
        if m:
            pct = int(m.group(1))
            self._last = pct
            return pct

        m = re.search(r'[Ww]riting at 0x([0-9a-fA-F]+)', text)
        if m and self._total > 0:
            addr = int(m.group(1), 16)
            done = 0
            for seg_start, seg_end in self._segs:
                if addr >= seg_end:
                    done += seg_end - seg_start
                elif addr > seg_start:
                    done += addr - seg_start
            pct = min(int(done * 100 / self._total), 99)
            if pct > self._last:
                self._last = pct
                return pct

        return None


def has_firmware_flashed(port: str) -> bool:
    """Return True if the app partition (0x10000) contains firmware."""
    fd, tmp = tempfile.mkstemp(suffix='.bin')
    os.close(fd)

    try:
        with esptool_io.capture():  # discard all esptool output
            esptool.main([
                '--port', port,
                '--chip', 'esp32s3',
                '--no-stub',
                '--before', 'no-reset',
                '--after', 'no-reset',
                'read_flash',
                '0x10000', '4', tmp,
            ])
        data = open(tmp, 'rb').read()
        return len(data) == 4 and data != b'\xff\xff\xff\xff'
    except Exception:
        return True  # conservative: assume firmware present, preserve NVS
    finally:
        try:
            os.unlink(tmp)
        except OSError:
            pass


def flash_board(
    port: str,
    board: Board,
    firmware_base: Path,
    progress_callback: Callable[[str, Optional[int]], None],
    serial_number: Optional[int] = None,
) -> None:
    folder = firmware_base / _BOARD_FOLDER[board]
    file_pairs = list(_BOARD_FILES[board])

    for _, filename in file_pairs:
        filepath = folder / filename
        if not filepath.exists():
            raise FileNotFoundError(f"Archivo no encontrado: {filepath}")

    nvs_tmp: Optional[str] = None

    if serial_number is not None and board == Board.MOTHERBOARD:
        nvs_offset, nvs_size = nvs_gen.find_nvs_partition(folder / 'partitions.bin')
        nvs_data = nvs_gen.generate_serial_nvs(serial_number, nvs_size)

        fd, nvs_tmp = tempfile.mkstemp(suffix='.bin')
        try:
            os.write(fd, nvs_data)
        finally:
            os.close(fd)
        file_pairs = file_pairs + [(hex(nvs_offset), nvs_tmp)]

    tracker = _ProgressTracker(file_pairs, folder)

    args = [
        '--port', port,
        '--chip', 'esp32s3',
        '--baud', '921600',
        '--before', _BOARD_BEFORE_RESET[board],
        'write-flash',
        '--flash-mode', 'keep',
        '--flash-freq', 'keep',
        '--flash-size', 'detect',
        '--compress',
    ]
    for addr, fname in file_pairs:
        args += [addr, fname if os.path.isabs(fname) else str(folder / fname)]

    reset_seen = False

    class _Writer(io.StringIO):
        def write(self, text: str) -> int:
            nonlocal reset_seen
            result = super().write(text)
            if 'resetting' in text.lower():
                reset_seen = True
            if text.strip():
                progress_callback(text.rstrip(), tracker.parse(text))
            return result

    esptool_io.set_thread_writer(_Writer())
    try:
        esptool.main(args)
    except SystemExit as e:
        if e.code != 0:
            raise RuntimeError(
                f"esptool terminó con error (código {e.code}). "
                "Flasheo fallido. Vuelve a intentarlo."
            )
    except Exception as e:
        if not reset_seen:
            raise RuntimeError(str(e))
    finally:
        esptool_io.set_thread_writer(None)
        if nvs_tmp:
            try:
                os.unlink(nvs_tmp)
            except OSError:
                pass
