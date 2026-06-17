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


# ── Discovery helpers ─────────────────────────────────────────────────────

from wifi_flasher import (
    discover_boards, _discover_mdns, _discover_subnet,
    _identify_board_type, _get_fw_version,
)


class TestGetFwVersion:
    def test_returns_version_on_success(self):
        r = MagicMock(); r.status_code = 200; r.json.return_value = {'version': '2.2.0'}
        with patch('wifi_flasher.requests.get', return_value=r):
            assert _get_fw_version('192.168.1.1') == '2.2.0'

    def test_returns_question_mark_on_failure(self):
        with patch('wifi_flasher.requests.get', side_effect=Exception('timeout')):
            assert _get_fw_version('192.168.1.1') == '?'

    def test_returns_question_mark_on_non_200(self):
        r = MagicMock(); r.status_code = 404
        with patch('wifi_flasher.requests.get', return_value=r):
            assert _get_fw_version('192.168.1.1') == '?'


class TestIdentifyBoardType:
    def test_display_hmi_when_get_freq_returns_200(self):
        r = MagicMock(); r.status_code = 200
        with patch('wifi_flasher.requests.get', return_value=r):
            assert _identify_board_type('192.168.1.5') == Board.DISPLAY_HMI

    def test_motherboard_when_get_freq_returns_404(self):
        r = MagicMock(); r.status_code = 404
        with patch('wifi_flasher.requests.get', return_value=r):
            assert _identify_board_type('192.168.1.6') == Board.MOTHERBOARD

    def test_returns_none_on_connection_error(self):
        with patch('wifi_flasher.requests.get', side_effect=Exception('refused')):
            assert _identify_board_type('192.168.1.7') is None


class TestDiscoverBoards:
    def _make_wifi_board(self, ip, board):
        return WifiBoard(ip=ip, board=board, fw_version='2.2.0', hostname='')

    def test_mdns_results_returned_without_subnet_scan(self):
        mb = self._make_wifi_board('192.168.1.5', Board.MOTHERBOARD)
        with patch('wifi_flasher._discover_mdns', return_value=[mb]) as mock_mdns, \
             patch('wifi_flasher._discover_subnet') as mock_subnet:
            result = discover_boards(timeout_s=1.0)

        assert result == [mb]
        mock_subnet.assert_not_called()

    def test_empty_mdns_triggers_subnet_scan(self):
        hmi = self._make_wifi_board('192.168.137.10', Board.DISPLAY_HMI)
        with patch('wifi_flasher._discover_mdns', return_value=[]), \
             patch('wifi_flasher._discover_subnet', return_value=[hmi]):
            result = discover_boards(timeout_s=1.0)

        assert result == [hmi]

    def test_mdns_exception_falls_back_to_subnet_scan(self):
        mb = self._make_wifi_board('192.168.1.1', Board.MOTHERBOARD)
        with patch('wifi_flasher._discover_mdns', side_effect=Exception('zeroconf error')), \
             patch('wifi_flasher._discover_subnet', return_value=[mb]):
            result = discover_boards(timeout_s=1.0)

        assert result == [mb]

    def test_both_empty_returns_empty_list(self):
        with patch('wifi_flasher._discover_mdns', return_value=[]), \
             patch('wifi_flasher._discover_subnet', return_value=[]):
            result = discover_boards(timeout_s=1.0)

        assert result == []
