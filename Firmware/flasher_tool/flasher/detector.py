import re
from enum import Enum
from typing import Optional

import esptool
import serial.tools.list_ports

import esptool_io


class Board(Enum):
    MOTHERBOARD = "motherBoard"
    DISPLAY_HMI = "Display HMI"
    SENSORBOARD = "SensorBoard"


class BoardDetectionError(Exception):
    pass


ESPRESSIF_VID = 0x303A

# ROM's built-in USB-Serial/JTAG peripheral (esptool.loader.ESPLoader.USB_JTAG_SERIAL_PID).
# ANY blank/virgin ESP32-S3 shows this — it's the ROM's own download-mode
# identity, used automatically when there's no valid app in flash to boot.
# motherBoard's OWN firmware ALSO shows this PID once running: its board
# (esp32-s3-devkitc-1) builds with ARDUINO_USB_MODE=1 ("Hardware CDC and
# JTAG"), which drives that same peripheral via Arduino's HWCDC class
# instead of a TinyUSB CDC stack with its own PID — HWCDC never touches the
# USB descriptor. So this single PID means "a motherBoard in any state, OR a
# virgin SensorBoard" — it cannot be resolved by VID/PID alone. Both boards
# use the same WROOM-1 module family but different flash size by design
# (motherBoard: N8 = 8MB, SensorBoard: N16R8 = 16MB), so detect_board_ambiguous()
# resolves it with an esptool flash-id probe instead.
NATIVE_USB_JTAG_PID = 0x1001

# Unambiguous, directly-flashable USB fingerprints only. motherBoard is
# deliberately absent here — see NATIVE_USB_JTAG_PID above. SensorBoard too
# — see SENSORBOARD_APP_CDC_PID below. board_from_vid_pid() returns None for
# that PID; callers must resolve it via is_ambiguous_native_usb() +
# detect_board_ambiguous().
BOARD_VID_PID: dict[Board, dict] = {
    Board.DISPLAY_HMI: {'vid': 0x1A86, 'pid': 0x7522},  # CH340K
}

# ESP-IDF esp_tinyusb, auto PID for a CDC-only device
# (0x4000 | CDC bit, CONFIG_TINYUSB_DESC_USE_DEFAULT_PID) — only shows up
# once SensorBoard's own firmware is running. Deliberately NOT a flashable
# fingerprint: the app drives the USB OTG controller via TinyUSB, so the
# ROM's USB-Serial/JTAG download path is gone and esptool's DTR/RTS reset
# dance (`--before default-reset`) is silently ignored by esp_tinyusb —
# every write attempt ends in "Failed to connect". A running SensorBoard
# must be re-entered into download mode by hand (IO0 low at reset), at
# which point it shows up as NATIVE_USB_JTAG_PID and takes the ambiguous
# path. is_sensorboard_app_port() exists so the UI can tell the user that.
SENSORBOARD_APP_CDC_PID = 0x4001

# Disambiguates NATIVE_USB_JTAG_PID by hardware flash size (a real BOM
# difference, not a firmware artifact): motherBoard's WROOM-1-N8 = 8MB,
# SensorBoard's WROOM-1-N16R8 = 16MB. Display HMI never reaches this path —
# its CH340 VID/PID is already unambiguous.
_FLASH_SIZE_MAP = {
    '8MB': Board.MOTHERBOARD,
    '16MB': Board.SENSORBOARD,
}


def find_all_board_ports() -> dict[Board, str]:
    """Scan COM ports and return {Board: device} for every unambiguously-identified board.

    Ports on NATIVE_USB_JTAG_PID (motherBoard, or a virgin motherBoard/
    SensorBoard) are not resolved here — that requires a serial probe, see
    detect_board_ambiguous().
    """
    result: dict[Board, str] = {}
    for port_info in serial.tools.list_ports.comports():
        board = board_from_vid_pid(port_info.vid, port_info.pid)
        if board is not None and board not in result:
            result[board] = port_info.device
    return result


def board_from_vid_pid(vid: Optional[int], pid: Optional[int]) -> Optional[Board]:
    """Return the Board type unambiguously matching vid/pid, or None if unknown/ambiguous."""
    for board, criteria in BOARD_VID_PID.items():
        if vid == criteria['vid'] and pid == criteria['pid']:
            return board
    return None


def is_ambiguous_native_usb(vid: Optional[int], pid: Optional[int]) -> bool:
    """True for the native-USB PID shared by motherBoard (any state) and a virgin SensorBoard."""
    return vid == ESPRESSIF_VID and pid == NATIVE_USB_JTAG_PID


def is_sensorboard_app_port(vid: Optional[int], pid: Optional[int]) -> bool:
    """True for a SensorBoard whose own firmware is running (TinyUSB CDC) — not flashable as-is."""
    return vid == ESPRESSIF_VID and pid == SENSORBOARD_APP_CDC_PID


def detect_board_ambiguous(port: str) -> Board:
    """Resolve a device on NATIVE_USB_JTAG_PID (motherBoard vs. virgin SensorBoard) by flash size."""
    flash_size = _read_flash_size(port)
    board = _FLASH_SIZE_MAP.get(flash_size)
    if board is None:
        raise BoardDetectionError(
            f"Placa no reconocida ({flash_size}). Solo se admiten motherBoard y SensorBoard."
        )
    return board


def _read_flash_size(port: str) -> str:
    with esptool_io.capture() as cap:
        try:
            # `--after no-reset` is essential: esptool's default post-command
            # hard reset would boot whatever app is in flash. On a SensorBoard
            # that already carries firmware that means TinyUSB takes over the
            # USB PHY, this USB-Serial/JTAG port vanishes and the follow-up
            # write-flash fails with "could not open port". A virgin chip
            # didn't care (no app → ROM falls back to download mode), which is
            # why the very first flash worked and every re-flash didn't.
            # Staying in download mode is harmless: write-flash's own
            # `--before default-reset` re-syncs via USB-Serial/JTAG anyway.
            esptool.main(['--port', port, '--no-stub', '--after', 'no-reset', 'flash-id'])
        except SystemExit as e:
            if e.code != 0:
                raise BoardDetectionError(
                    f"No se pudo conectar al puerto {port}. "
                    "Comprueba el cable y que no esté abierto en otro programa."
                )

    for line in cap.value.splitlines():
        if 'Detected flash size' in line:
            return line.split(':')[-1].strip()

    raise BoardDetectionError(
        "No se pudo detectar el tamaño de flash. "
        "Comprueba que la placa esté conectada correctamente."
    )


# --- Revision de la motherBoard: V18 o V19 ----------------------------------
#
# Las dos se ven igual por USB (NATIVE_USB_JTAG_PID) y las dos tienen 8 MB de
# flash, pero llevan modulos distintos POR BOM: V18 un ESP32-S3-WROOM-1-N8 (sin
# PSRAM) y V19 un N8R2 (chip ESP32-S3R2, 2 MB de PSRAM dentro del chip). Eso
# esta grabado en el eFuse PSRAM_CAP de fabrica de Espressif, asi que no depende
# del firmware que haya en la placa ni de que este vacia.
#
# Se lee el eFuse con `read-mem` y no del texto "Features: ... Embedded PSRAM"
# de flash-id: esptool 4.x no lo imprime para el S3 (get_chip_features devolvia
# solo WiFi/BLE), y "no sale PSRAM" no distingue "no hay" de "esta version no lo
# dice". Flashear el firmware de V18 en una V19 deja el display sin 5 V (IO40)
# y la de V19 en una V18 no arranca bien: por eso un valor desconocido es un
# error, no una suposicion.
#
# PSRAM_CAP: bits [4:3] de la palabra 4 del bloque 1 del eFuse y un bit alto en
# el bit 19 de la palabra 5 (mismo calculo que ESP32S3ROM.get_psram_cap() de
# esptool 5.x). 0 = sin PSRAM, 1 = 8 MB, 2 = 2 MB, 3 = 16 MB, 4 = 4 MB.
MB_HW_V18 = 18
MB_HW_V19 = 19

_EFUSE_BLK1_WORD4 = 0x60007054
_EFUSE_BLK1_WORD5 = 0x60007058

_MB_HW_BY_PSRAM_CAP = {
    0: MB_HW_V18,  # WROOM-1-N8
    2: MB_HW_V19,  # WROOM-1-N8R2 (ESP32-S3R2)
}


def decode_psram_cap(word4: int, word5: int) -> int:
    """PSRAM_CAP del ESP32-S3 a partir de las palabras 4 y 5 del bloque 1."""
    return (((word5 >> 19) & 0x01) << 2) | ((word4 >> 3) & 0x03)


def motherboard_hw_from_psram_cap(cap: int) -> int:
    """Revision de la motherBoard (MB_HW_V18/MB_HW_V19) para ese PSRAM_CAP."""
    hw = _MB_HW_BY_PSRAM_CAP.get(cap)
    if hw is None:
        raise BoardDetectionError(
            f"motherBoard con un modulo que no es de ninguna revision conocida "
            f"(PSRAM_CAP={cap}). No se flashea."
        )
    return hw


def detect_motherboard_hw(port: str) -> int:
    """Revision de la motherBoard conectada en `port`, leida del eFuse."""
    word4 = _read_mem(port, _EFUSE_BLK1_WORD4)
    word5 = _read_mem(port, _EFUSE_BLK1_WORD5)
    return motherboard_hw_from_psram_cap(decode_psram_cap(word4, word5))


def _read_mem(port: str, address: int) -> int:
    with esptool_io.capture() as cap:
        try:
            # --after no-reset por lo mismo que en _read_flash_size: la placa
            # tiene que quedarse en modo descarga para el write-flash.
            esptool.main(['--port', port, '--no-stub', '--after', 'no-reset',
                          'read-mem', hex(address)])
        except SystemExit as e:
            if e.code != 0:
                raise BoardDetectionError(
                    f"No se pudo leer la revision de la motherBoard en {port}. "
                    "Comprueba el cable y que no este abierto en otro programa."
                )
    m = re.search(rf'{address:#010x}\s*=\s*(0x[0-9a-fA-F]+)', cap.value)
    if m is None:
        raise BoardDetectionError(
            "No se pudo leer la revision de la motherBoard (eFuse). "
            "Comprueba que la placa este conectada correctamente."
        )
    return int(m.group(1), 16)


def identify_native_usb(port: str, solo_motherboard: bool = False) -> tuple[Board, Optional[int]]:
    """Placa (y revision, si es motherBoard) de un puerto USB nativo de Espressif.

    Con solo_motherboard (flasher_config.json) no se mira el tamano de flash:
    toda placa es motherBoard y solo se lee su revision del eFuse. Una
    SensorBoard conectada por error no pasa esa lectura (PSRAM_CAP=1, ninguna
    revision de motherBoard) y no se flashea.
    """
    if solo_motherboard:
        return Board.MOTHERBOARD, detect_motherboard_hw(port)
    board = detect_board_ambiguous(port)
    if board == Board.MOTHERBOARD:
        return board, detect_motherboard_hw(port)
    return board, None
