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

# Unambiguous USB fingerprints only. motherBoard is deliberately absent here
# — see NATIVE_USB_JTAG_PID above. board_from_vid_pid() returns None for
# that PID; callers must resolve it via is_ambiguous_native_usb() +
# detect_board_ambiguous().
BOARD_VID_PID: dict[Board, dict] = {
    # ESP-IDF esp_tinyusb, auto PID for a CDC-only device
    # (0x4000 | CDC bit, CONFIG_TINYUSB_DESC_USE_DEFAULT_PID) — only shows up
    # once SensorBoard's own firmware is running.
    Board.SENSORBOARD: {'vid': ESPRESSIF_VID, 'pid': 0x4001},
    Board.DISPLAY_HMI: {'vid': 0x1A86, 'pid': 0x7522},  # CH340K
}

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
            esptool.main(['--port', port, '--no-stub', 'flash-id'])
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
