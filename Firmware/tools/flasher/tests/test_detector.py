import pytest
from unittest.mock import patch

from detector import detect_board, Board, BoardDetectionError


def _fake_esptool(flash_size):
    def _inner(args):
        print(f"esptool.py v4.7.0\nSerial port {args[1]}\nDetected flash size: {flash_size}")
    return _inner


def test_detects_motherboard_for_8mb():
    with patch('detector.esptool.main', _fake_esptool('8MB')):
        board = detect_board('COM3')
    assert board == Board.MOTHERBOARD


def test_detects_display_hmi_for_16mb():
    with patch('detector.esptool.main', _fake_esptool('16MB')):
        board = detect_board('COM3')
    assert board == Board.DISPLAY_HMI


def test_raises_for_unrecognized_size():
    with patch('detector.esptool.main', _fake_esptool('4MB')):
        with pytest.raises(BoardDetectionError, match='4MB'):
            detect_board('COM3')


def test_raises_when_flash_size_line_absent():
    with patch('detector.esptool.main', lambda args: print("Error connecting")):
        with pytest.raises(BoardDetectionError):
            detect_board('COM3')


def test_raises_on_nonzero_exit():
    def bad_exit(args):
        raise SystemExit(2)
    with patch('detector.esptool.main', bad_exit):
        with pytest.raises(BoardDetectionError):
            detect_board('COM3')
