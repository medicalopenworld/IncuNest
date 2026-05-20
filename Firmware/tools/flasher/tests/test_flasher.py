import pytest
from pathlib import Path
from unittest.mock import patch

from flasher import flash_board, _parse_percent
from detector import Board


@pytest.fixture
def firmware_dir(tmp_path):
    for folder, files in [
        ('motherboard', ['bootloader.bin', 'partitions.bin', 'firmware.bin']),
        ('display_hmi', ['bootloader.bin', 'partitions.bin', 'ota_data_initial.bin', 'firmware.bin']),
    ]:
        d = tmp_path / folder
        d.mkdir()
        for f in files:
            (d / f).write_bytes(b'\xff' * 16)
    return tmp_path


def test_motherboard_flash_includes_correct_addresses(firmware_dir):
    captured = []
    with patch('flasher.esptool.main', lambda args: captured.extend(args)):
        flash_board('COM3', Board.MOTHERBOARD, firmware_dir, lambda m, p: None)

    assert '0x0000' in captured
    assert '0x8000' in captured
    assert '0x10000' in captured
    assert '0xE000' not in captured


def test_display_hmi_flash_includes_ota_data_address(firmware_dir):
    captured = []
    with patch('flasher.esptool.main', lambda args: captured.extend(args)):
        flash_board('COM3', Board.DISPLAY_HMI, firmware_dir, lambda m, p: None)

    assert '0x0000' in captured
    assert '0x8000' in captured
    assert '0xE000' in captured
    assert '0x10000' in captured


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


def test_parse_percent_extracts_integer():
    assert _parse_percent("Writing at 0x00010000... (67 %)") == 67
    assert _parse_percent("Writing at 0x00010000... (100 %)") == 100


def test_parse_percent_returns_none_for_non_progress_line():
    assert _parse_percent("Uploading stub...") is None
    assert _parse_percent("") is None
