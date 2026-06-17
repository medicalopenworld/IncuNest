import pytest
from pathlib import Path
from unittest.mock import patch, MagicMock, call

from detector import Board
from wifi_flasher import WifiBoard, flash_board_wifi, _board_from_hostname


@pytest.fixture
def firmware_base(tmp_path):
    for folder in ('motherboard', 'display_hmi'):
        d = tmp_path / folder
        d.mkdir()
        (d / 'firmware.bin').write_bytes(b'\xAA\xBB\xCC\xDD' * 256)
    return tmp_path


# ── _board_from_hostname ───────────────────────────────────────────────────

class TestBoardFromHostname:
    def test_motherboard_dash(self):
        assert _board_from_hostname('IncuNest-42') == Board.MOTHERBOARD

    def test_motherboard_with_local_suffix(self):
        assert _board_from_hostname('IncuNest-42.local') == Board.MOTHERBOARD

    def test_display_hmi(self):
        assert _board_from_hostname('IncuNest_Display-7') == Board.DISPLAY_HMI

    def test_display_hmi_with_local_suffix(self):
        assert _board_from_hostname('IncuNest_Display-7.local') == Board.DISPLAY_HMI

    def test_unknown_returns_none(self):
        assert _board_from_hostname('SomeOtherDevice') is None

    def test_empty_returns_none(self):
        assert _board_from_hostname('') is None


# ── flash_board_wifi ──────────────────────────────────────────────────────

class TestFlashBoardWifi:
    def _ok_response(self):
        r = MagicMock()
        r.status_code = 200
        r.text = 'OK'
        return r

    def _401_response(self):
        r = MagicMock()
        r.status_code = 401
        r.text = ''
        return r

    def test_success_with_new_credentials(self, firmware_base):
        progress = []
        with patch('wifi_flasher.requests.post', return_value=self._ok_response()) as mp:
            flash_board_wifi('192.168.1.1', Board.MOTHERBOARD, firmware_base,
                             lambda msg, pct: progress.append(pct))

        assert mp.call_count == 1
        auth = mp.call_args.kwargs['auth']
        assert auth.username == 'incunestadmin'
        assert 99 in progress

    def test_falls_back_to_old_credentials_on_401(self, firmware_base):
        with patch('wifi_flasher.requests.post',
                   side_effect=[self._401_response(), self._ok_response()]) as mp:
            flash_board_wifi('192.168.1.1', Board.MOTHERBOARD, firmware_base,
                             lambda m, p: None)

        assert mp.call_count == 2
        auth = mp.call_args_list[1].kwargs['auth']
        assert auth.username == 'in3admin'

    def test_motherboard_falls_back_to_no_auth(self, firmware_base):
        with patch('wifi_flasher.requests.post',
                   side_effect=[self._401_response(), self._401_response(),
                                 self._ok_response()]) as mp:
            flash_board_wifi('192.168.1.1', Board.MOTHERBOARD, firmware_base,
                             lambda m, p: None)

        assert mp.call_count == 3
        assert mp.call_args_list[2].kwargs['auth'] is None

    def test_display_hmi_does_not_fall_back_to_no_auth(self, firmware_base):
        with patch('wifi_flasher.requests.post',
                   side_effect=[self._401_response(), self._401_response()]):
            with pytest.raises(RuntimeError, match='utenticaci'):
                flash_board_wifi('192.168.1.1', Board.DISPLAY_HMI, firmware_base,
                                 lambda m, p: None)

    def test_fail_response_raises_runtime_error(self, firmware_base):
        r = MagicMock(); r.status_code = 200; r.text = 'FAIL'
        with patch('wifi_flasher.requests.post', return_value=r):
            with pytest.raises(RuntimeError, match='FAIL'):
                flash_board_wifi('192.168.1.1', Board.MOTHERBOARD, firmware_base,
                                 lambda m, p: None)

    def test_unexpected_response_raises_runtime_error(self, firmware_base):
        r = MagicMock(); r.status_code = 200; r.text = 'UNEXPECTED'
        with patch('wifi_flasher.requests.post', return_value=r):
            with pytest.raises(RuntimeError, match='nesperada'):
                flash_board_wifi('192.168.1.1', Board.MOTHERBOARD, firmware_base,
                                 lambda m, p: None)

    def test_missing_firmware_raises_file_not_found(self, tmp_path):
        # Only display_hmi exists, not motherboard
        (tmp_path / 'display_hmi').mkdir()
        (tmp_path / 'display_hmi' / 'firmware.bin').write_bytes(b'\x00')
        with pytest.raises(FileNotFoundError):
            flash_board_wifi('192.168.1.1', Board.MOTHERBOARD, tmp_path,
                             lambda m, p: None)

    def test_progress_callback_receives_99_on_success(self, firmware_base):
        progress = []
        with patch('wifi_flasher.requests.post', return_value=self._ok_response()):
            flash_board_wifi('192.168.1.1', Board.DISPLAY_HMI, firmware_base,
                             lambda msg, pct: progress.append(pct))
        assert 99 in progress
