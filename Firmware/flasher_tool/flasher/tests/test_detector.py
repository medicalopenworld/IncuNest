import pytest
from unittest.mock import patch

from detector import (
    detect_board_ambiguous, board_from_vid_pid, is_ambiguous_native_usb,
    is_sensorboard_app_port,
    Board, BoardDetectionError,
)


def test_board_from_vid_pid_running_sensorboard_is_not_flashable():
    # PID 0x4001 = SensorBoard app running on TinyUSB CDC. esptool cannot
    # reset a TinyUSB device into download mode, so it must NOT be offered as
    # a flashable board; is_sensorboard_app_port() flags it for a UI hint.
    assert board_from_vid_pid(0x303A, 0x4001) is None


def test_is_sensorboard_app_port():
    assert is_sensorboard_app_port(0x303A, 0x4001) is True
    assert is_sensorboard_app_port(0x303A, 0x1001) is False
    assert is_sensorboard_app_port(0x1A86, 0x4001) is False


def test_board_from_vid_pid_display_hmi():
    assert board_from_vid_pid(0x1A86, 0x7522) == Board.DISPLAY_HMI


def test_board_from_vid_pid_unknown_vid():
    assert board_from_vid_pid(0x1234, 0x5678) is None


def test_board_from_vid_pid_native_usb_jtag_pid_is_ambiguous_not_motherboard():
    # 0x303A:0x1001 (ROM USB-Serial/JTAG) is shared by motherBoard (any
    # state, via HWCDC) and a virgin SensorBoard — board_from_vid_pid() must
    # NOT silently resolve it to motherBoard; callers resolve it via
    # detect_board_ambiguous() instead.
    assert board_from_vid_pid(0x303A, 0x1001) is None


def test_board_from_vid_pid_native_usb_unrecognized_pid():
    assert board_from_vid_pid(0x303A, 0x9999) is None


def test_is_ambiguous_native_usb_true_for_jtag_pid():
    assert is_ambiguous_native_usb(0x303A, 0x1001) is True


def test_is_ambiguous_native_usb_false_for_other_pids():
    assert is_ambiguous_native_usb(0x303A, 0x4001) is False
    assert is_ambiguous_native_usb(0x1A86, 0x1001) is False


def _fake_esptool(flash_size):
    def _inner(args):
        print(f"esptool.py v4.7.0\nSerial port {args[1]}\nDetected flash size: {flash_size}")
    return _inner


def test_detects_motherboard_for_8mb():
    with patch('detector.esptool.main', _fake_esptool('8MB')):
        board = detect_board_ambiguous('COM3')
    assert board == Board.MOTHERBOARD


def test_detects_sensorboard_for_16mb():
    with patch('detector.esptool.main', _fake_esptool('16MB')):
        board = detect_board_ambiguous('COM3')
    assert board == Board.SENSORBOARD


def test_raises_for_unrecognized_size():
    with patch('detector.esptool.main', _fake_esptool('4MB')):
        with pytest.raises(BoardDetectionError, match='4MB'):
            detect_board_ambiguous('COM3')


def test_raises_when_flash_size_line_absent():
    with patch('detector.esptool.main', lambda args: print("Error connecting")):
        with pytest.raises(BoardDetectionError):
            detect_board_ambiguous('COM3')


def test_raises_on_nonzero_exit():
    def bad_exit(args):
        raise SystemExit(2)
    with patch('detector.esptool.main', bad_exit):
        with pytest.raises(BoardDetectionError):
            detect_board_ambiguous('COM3')


def test_probe_leaves_board_in_download_mode():
    # The flash-id probe must not hard-reset afterwards: on a SensorBoard that
    # already has firmware that boots TinyUSB, the USB-Serial/JTAG port
    # disappears and the follow-up write-flash fails.
    seen = {}

    def spy(args):
        seen['args'] = args
        print("Detected flash size: 16MB")

    with patch('detector.esptool.main', spy):
        detect_board_ambiguous('COM3')
    args = seen['args']
    assert args[args.index('--after') + 1] == 'no-reset'
    assert 'flash-id' in args


# --- Revision de la motherBoard (V18 / V19) por el eFuse PSRAM_CAP ---------
# V18 lleva un ESP32-S3-WROOM-1-N8 (sin PSRAM); V19, un N8R2 (ESP32-S3R2, 2 MB
# de PSRAM dentro del chip). esptool lo lee del eFuse PSRAM_CAP: bits [4:3] de
# la palabra 4 del bloque 1 (0x60007054) y un bit alto en el bit 19 de la
# palabra 5 (0x60007058). Mismo calculo que ESP32S3ROM.get_psram_cap() de
# esptool 5.x.

from detector import (  # noqa: E402
    decode_psram_cap, motherboard_hw_from_psram_cap, detect_motherboard_hw,
    MB_HW_V18, MB_HW_V19,
)


def test_decode_psram_cap_none():
    assert decode_psram_cap(0x00000000, 0x00000000) == 0


def test_decode_psram_cap_2mb():
    assert decode_psram_cap(0x2 << 3, 0) == 2


def test_decode_psram_cap_ignores_neighbour_bits():
    # Bits ajenos (vendor de PSRAM en [8:7], otros) no cuentan.
    assert decode_psram_cap(0xFFFFFFE7, 0x00000000) == 0


def test_decode_psram_cap_high_bit():
    assert decode_psram_cap(0, 1 << 19) == 4


def test_psram_cap_maps_to_revision():
    assert motherboard_hw_from_psram_cap(0) == MB_HW_V18
    assert motherboard_hw_from_psram_cap(2) == MB_HW_V19


def test_unknown_psram_cap_is_an_error_not_a_guess():
    # 8 MB de PSRAM (cap 1) no es ninguna motherBoard conocida: mejor no
    # flashear que flashear la revision equivocada.
    for cap in (1, 3, 4):
        with pytest.raises(BoardDetectionError):
            motherboard_hw_from_psram_cap(cap)


def _fake_read_mem(word4, word5, seen=None):
    def _inner(args):
        if seen is not None:
            seen.append(args)
        addr = int(args[args.index('read-mem') + 1], 16)
        val = {0x60007054: word4, 0x60007058: word5}[addr]
        print(f"esptool v5.6.0\nConnected to ESP32-S3\n{addr:#010x} = {val:#010x}")
    return _inner


def test_detect_v18_board():
    with patch('detector.esptool.main', _fake_read_mem(0, 0)):
        assert detect_motherboard_hw('COM3') == MB_HW_V18


def test_detect_v19_board():
    with patch('detector.esptool.main', _fake_read_mem(0x2 << 3, 0)):
        assert detect_motherboard_hw('COM3') == MB_HW_V19


def test_detect_hw_leaves_board_in_download_mode():
    seen = []
    with patch('detector.esptool.main', _fake_read_mem(0, 0, seen)):
        detect_motherboard_hw('COM3')
    for args in seen:
        assert args[args.index('--after') + 1] == 'no-reset'


def test_detect_hw_raises_when_value_line_absent():
    with patch('detector.esptool.main', lambda args: print("Error connecting")):
        with pytest.raises(BoardDetectionError):
            detect_motherboard_hw('COM3')


def test_detect_hw_raises_on_nonzero_exit():
    def bad_exit(args):
        raise SystemExit(2)
    with patch('detector.esptool.main', bad_exit):
        with pytest.raises(BoardDetectionError):
            detect_motherboard_hw('COM3')


# --- Modo "solo motherboard" (flasher_config.json: "solo_motherboard") -----
# En una tanda solo de motherBoards se salta la prueba del tamano de flash que
# la distingue de una SensorBoard virgen. La revision V18/V19 se sigue leyendo
# del eFuse, y eso tambien hace de red: una SensorBoard (N16R8, PSRAM 8 MB) da
# PSRAM_CAP=1, que no es ninguna motherBoard, y no se flashea.

from detector import identify_native_usb  # noqa: E402


def _fake_esptool_all(flash_size, word4, word5, seen):
    def _inner(args):
        seen.append(args)
        if 'flash-id' in args:
            print(f"Detected flash size: {flash_size}")
        else:
            addr = int(args[args.index('read-mem') + 1], 16)
            print(f"{addr:#010x} = {({0x60007054: word4, 0x60007058: word5}[addr]):#010x}")
    return _inner


def test_solo_motherboard_no_mira_el_tamano_de_flash():
    seen = []
    with patch('detector.esptool.main', _fake_esptool_all('8MB', 0x2 << 3, 0, seen)):
        board, hw = identify_native_usb('COM3', solo_motherboard=True)
    assert (board, hw) == (Board.MOTHERBOARD, MB_HW_V19)
    assert not any('flash-id' in a for a in seen)


def test_solo_motherboard_v18():
    with patch('detector.esptool.main', _fake_esptool_all('8MB', 0, 0, [])):
        assert identify_native_usb('COM3', solo_motherboard=True) == (Board.MOTHERBOARD, MB_HW_V18)


def test_solo_motherboard_con_una_sensorboard_no_flashea():
    # N16R8: PSRAM_CAP=1 (8 MB) -> no es V18 ni V19.
    with patch('detector.esptool.main', _fake_esptool_all('16MB', 0x1 << 3, 0, [])):
        with pytest.raises(BoardDetectionError):
            identify_native_usb('COM3', solo_motherboard=True)


def test_modo_normal_sigue_distinguiendo_la_sensorboard():
    seen = []
    with patch('detector.esptool.main', _fake_esptool_all('16MB', 0x1 << 3, 0, seen)):
        assert identify_native_usb('COM3', solo_motherboard=False) == (Board.SENSORBOARD, None)
    assert any('flash-id' in a for a in seen)
    assert not any('read-mem' in a for a in seen)


def test_modo_normal_motherboard_lee_la_revision():
    with patch('detector.esptool.main', _fake_esptool_all('8MB', 0x2 << 3, 0, [])):
        assert identify_native_usb('COM3', solo_motherboard=False) == (Board.MOTHERBOARD, MB_HW_V19)
