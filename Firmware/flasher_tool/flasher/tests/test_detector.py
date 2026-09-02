import pytest
from unittest.mock import patch

from detector import (
    detect_board_ambiguous, board_from_vid_pid, is_ambiguous_native_usb,
    Board, BoardDetectionError,
)


def test_board_from_vid_pid_sensorboard():
    assert board_from_vid_pid(0x303A, 0x4001) == Board.SENSORBOARD


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
