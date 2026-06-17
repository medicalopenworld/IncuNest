# WiFi OTA Flasher — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Añadir una pestaña "WiFi" al GUI existente (`flasher_tool/flasher/main.py`) que descubre incubadoras IncuNest en la red y las flashea vía HTTP OTA con un solo clic.

**Architecture:** Nuevo módulo `wifi_flasher.py` con `discover_boards()` (mDNS → subnet scan fallback) y `flash_board_wifi()` (HTTP multipart POST con fallback de autenticación). `main.py` añade un `ttk.Notebook` con pestaña USB (contenido actual) y pestaña WiFi (`_WifiTab`).

**Tech Stack:** Python 3.12, tkinter, requests>=2.28, zeroconf>=0.128, pytest, PyInstaller.

## Global Constraints

- Working dir: `flasher_tool/flasher/`
- Tests run from `flasher_tool/flasher/` con `pytest tests/`
- Conftest añade el directorio padre al sys.path — imports directos por nombre de módulo
- Credenciales nuevas: `incunestadmin` / `savinglives`; fallback antiguo: `in3admin` / `savinglives`; fallback sin auth solo para `Board.MOTHERBOARD`
- Solo se flashea `firmware.bin` — bootloader/partitions/NVS fuera de scope
- `Board.MOTHERBOARD` → carpeta `motherboard/`; `Board.DISPLAY_HMI` → carpeta `display_hmi/`
- Nombre de red: `IncuNest_Display-*` → Display HMI; `IncuNest-*` → Motherboard
- Ventana final: `480×660`

---

### Task 1: Dependencias y spec PyInstaller

**Files:**
- Modify: `requirements.txt`
- Modify: `flasher.spec`

**Interfaces:**
- Produces: `requests` y `zeroconf` disponibles para importar en tareas posteriores

- [ ] **Step 1: Actualizar requirements.txt**

Reemplazar el contenido actual por:

```
esptool>=4.7.0
pyserial>=3.5
requests>=2.28
zeroconf>=0.128
```

- [ ] **Step 2: Actualizar flasher.spec — añadir hiddenimports**

En la lista `hiddenimports` de `flasher.spec`, añadir tras las entradas existentes:

```python
        'requests',
        'requests.adapters',
        'requests.auth',
        'urllib3',
        'urllib3.util',
        'zeroconf',
        'zeroconf._utils',
        'zeroconf._dns',
        'zeroconf._services',
        'zeroconf._services.browser',
        'zeroconf._handlers',
        'zeroconf._handlers.browsers',
        'ifaddr',
```

La sección `hiddenimports` completa queda:

```python
    hiddenimports=[
        'esptool',
        'esptool.cmds',
        'esptool.loader',
        'esptool.targets',
        'esptool.targets.esp32s3',
        'esptool.targets.esp32',
        'esptool.util',
        'esptool.bin_image',
        'esptool.elf',
        'esptool.reset',
        'esptool.config',
        'serial',
        'serial.tools',
        'serial.tools.list_ports',
        'serial.tools.list_ports_windows',
        'PIL',
        'PIL.Image',
        'PIL.ImageTk',
        'requests',
        'requests.adapters',
        'requests.auth',
        'urllib3',
        'urllib3.util',
        'zeroconf',
        'zeroconf._utils',
        'zeroconf._dns',
        'zeroconf._services',
        'zeroconf._services.browser',
        'zeroconf._handlers',
        'zeroconf._handlers.browsers',
        'ifaddr',
    ],
```

- [ ] **Step 3: Instalar las dependencias nuevas**

```
pip install requests zeroconf
```

- [ ] **Step 4: Verificar que los imports funcionan**

```
python -c "import requests; import zeroconf; print('OK')"
```

Resultado esperado: `OK`

- [ ] **Step 5: Commit**

```bash
git add flasher_tool/flasher/requirements.txt flasher_tool/flasher/flasher.spec
git commit -m "chore(flasher): add requests and zeroconf dependencies for WiFi OTA"
```

---

### Task 2: `wifi_flasher.py` — tipos y flash HTTP con fallback de auth

**Files:**
- Create: `flasher_tool/flasher/wifi_flasher.py`
- Create: `flasher_tool/flasher/tests/test_wifi_flasher.py`

**Interfaces:**
- Consumes: `Board` de `detector.py`
- Produces:
  - `WifiBoard(ip: str, board: Board, fw_version: str, hostname: str = "")` — dataclass
  - `_board_from_hostname(hostname: str) -> Optional[Board]`
  - `flash_board_wifi(ip: str, board: Board, firmware_base: Path, progress_cb: Callable[[str, Optional[int]], None], timeout_s: float = 120.0) -> None`

- [ ] **Step 1: Escribir los tests**

Crear `tests/test_wifi_flasher.py`:

```python
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
```

- [ ] **Step 2: Ejecutar los tests y verificar que fallan (módulo no existe aún)**

```
pytest tests/test_wifi_flasher.py -v
```

Resultado esperado: `ModuleNotFoundError: No module named 'wifi_flasher'`

- [ ] **Step 3: Crear `wifi_flasher.py` con los tipos y la función de flash**

Crear `wifi_flasher.py`:

```python
import socket
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from concurrent.futures import TimeoutError as FuturesTimeout
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Optional

import requests
from requests.auth import HTTPBasicAuth

from detector import Board

_BOARD_FOLDER: dict[Board, str] = {
    Board.MOTHERBOARD: 'motherboard',
    Board.DISPLAY_HMI: 'display_hmi',
}

_AUTH_SEQUENCES: dict[Board, list] = {
    Board.DISPLAY_HMI: [
        HTTPBasicAuth('incunestadmin', 'savinglives'),
        HTTPBasicAuth('in3admin', 'savinglives'),
    ],
    Board.MOTHERBOARD: [
        HTTPBasicAuth('incunestadmin', 'savinglives'),
        HTTPBasicAuth('in3admin', 'savinglives'),
        None,
    ],
}

_MDNS_SERVICE = '_http._tcp.local.'


@dataclass
class WifiBoard:
    ip: str
    board: Board
    fw_version: str
    hostname: str = ''


def _board_from_hostname(hostname: str) -> Optional[Board]:
    """Parse an mDNS hostname (with or without .local) into a Board type."""
    name = hostname.split('.')[0]
    if name.startswith('IncuNest_Display'):
        return Board.DISPLAY_HMI
    if name.startswith('IncuNest'):
        return Board.MOTHERBOARD
    return None


def flash_board_wifi(
    ip: str,
    board: Board,
    firmware_base: Path,
    progress_cb: Callable[[str, Optional[int]], None],
    timeout_s: float = 120.0,
) -> None:
    """Flash firmware.bin to an ESP32 via HTTP OTA POST /update.

    Tries auth credentials in order: incunestadmin, in3admin, None (MB only).
    Raises RuntimeError on auth failure, FAIL response, or unexpected response.
    """
    fw_path = firmware_base / _BOARD_FOLDER[board] / 'firmware.bin'
    if not fw_path.exists():
        raise FileNotFoundError(f'firmware.bin no encontrado: {fw_path}')

    progress_cb('Conectando…', None)

    for auth in _AUTH_SEQUENCES[board]:
        with open(fw_path, 'rb') as f:
            resp = requests.post(
                f'http://{ip}/update',
                files={'update': ('firmware.bin', f, 'application/octet-stream')},
                auth=auth,
                timeout=timeout_s,
            )
        if resp.status_code == 401:
            continue
        progress_cb('', 99)
        body = resp.text.strip()
        if body == 'FAIL':
            raise RuntimeError(f'El dispositivo reportó FAIL ({ip})')
        if body != 'OK':
            raise RuntimeError(f'Respuesta inesperada ({ip}): {body[:50]}')
        return

    raise RuntimeError(f'Autenticación fallida en {ip} — ninguna credencial funcionó')
```

- [ ] **Step 4: Ejecutar los tests y verificar que pasan**

```
pytest tests/test_wifi_flasher.py -v
```

Resultado esperado: todos los tests `PASSED`.

- [ ] **Step 5: Commit**

```bash
git add flasher_tool/flasher/wifi_flasher.py flasher_tool/flasher/tests/test_wifi_flasher.py
git commit -m "feat(flasher): add wifi_flasher module with flash_board_wifi and auth fallback"
```

---

### Task 3: `wifi_flasher.py` — descubrimiento de dispositivos

**Files:**
- Modify: `flasher_tool/flasher/wifi_flasher.py` (añadir funciones de descubrimiento)
- Modify: `flasher_tool/flasher/tests/test_wifi_flasher.py` (añadir tests de descubrimiento)

**Interfaces:**
- Consumes: `WifiBoard`, `_board_from_hostname`, `_MDNS_SERVICE` definidos en Task 2
- Produces:
  - `_get_fw_version(ip: str, timeout: float = 2.0) -> str`
  - `_identify_board_type(ip: str, timeout: float = 0.5) -> Board`
  - `_discover_mdns(timeout_s: float) -> list[WifiBoard]`
  - `_local_subnet() -> str`
  - `_probe_ip(ip: str) -> Optional[WifiBoard]`
  - `_discover_subnet(timeout_s: float) -> list[WifiBoard]`
  - `discover_boards(timeout_s: float = 5.0) -> list[WifiBoard]`

- [ ] **Step 1: Añadir tests de descubrimiento a `test_wifi_flasher.py`**

Añadir al final del fichero:

```python
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

    def test_motherboard_on_connection_error(self):
        with patch('wifi_flasher.requests.get', side_effect=Exception('refused')):
            assert _identify_board_type('192.168.1.7') == Board.MOTHERBOARD


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
```

- [ ] **Step 2: Ejecutar los nuevos tests — deben fallar por ImportError**

```
pytest tests/test_wifi_flasher.py::TestGetFwVersion tests/test_wifi_flasher.py::TestIdentifyBoardType tests/test_wifi_flasher.py::TestDiscoverBoards -v
```

Resultado esperado: `ImportError` (funciones no existen aún).

- [ ] **Step 3: Añadir funciones de descubrimiento a `wifi_flasher.py`**

Añadir después de la función `flash_board_wifi`:

```python
def _get_fw_version(ip: str, timeout: float = 2.0) -> str:
    """GET /get_fw_version and return the version string, or '?' on any failure."""
    try:
        resp = requests.get(f'http://{ip}/get_fw_version', timeout=timeout)
        if resp.status_code == 200:
            return resp.json().get('version', '?')
    except Exception:
        pass
    return '?'


def _identify_board_type(ip: str, timeout: float = 0.5) -> Board:
    """Determine board type by probing /get_freq (Display HMI only endpoint)."""
    try:
        resp = requests.get(f'http://{ip}/get_freq', timeout=timeout)
        if resp.status_code == 200:
            return Board.DISPLAY_HMI
    except Exception:
        pass
    return Board.MOTHERBOARD


def _discover_mdns(timeout_s: float) -> list[WifiBoard]:
    """Browse mDNS _http._tcp.local for IncuNest services."""
    from zeroconf import Zeroconf, ServiceBrowser, ServiceStateChange

    found: list[tuple[str, Board, str]] = []
    lock = threading.Lock()

    def on_change(zeroconf, service_type, name, state_change):
        if state_change is not ServiceStateChange.Added:
            return
        hostname = name.split('.')[0]
        board = _board_from_hostname(hostname)
        if board is None:
            return
        info = zeroconf.get_service_info(service_type, name)
        if info and info.addresses:
            ip = socket.inet_ntoa(info.addresses[0])
            with lock:
                if not any(e[0] == ip for e in found):
                    found.append((ip, board, hostname))

    zc = Zeroconf()
    try:
        browser = ServiceBrowser(zc, _MDNS_SERVICE, handlers=[on_change])
        time.sleep(timeout_s)
        browser.cancel()
    finally:
        zc.close()

    return [
        WifiBoard(ip=ip, board=board, fw_version=_get_fw_version(ip), hostname=hostname)
        for ip, board, hostname in found
    ]


def _local_subnet() -> str:
    """Return the first two octets + third of the local IP (e.g. '192.168.137').
    Falls back to Windows hotspot default '192.168.137' on failure."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
        s.close()
        if not ip.startswith('127.'):
            return ip.rsplit('.', 1)[0]
    except Exception:
        pass
    return '192.168.137'


def _probe_ip(ip: str) -> Optional[WifiBoard]:
    """Return a WifiBoard if ip hosts an IncuNest device, else None."""
    try:
        resp = requests.get(f'http://{ip}/get_fw_version', timeout=0.3)
        if resp.status_code != 200:
            return None
        fw_version = resp.json().get('version', '?')
        board = _identify_board_type(ip)
        return WifiBoard(ip=ip, board=board, fw_version=fw_version)
    except Exception:
        return None


def _discover_subnet(timeout_s: float) -> list[WifiBoard]:
    """Scan all 254 hosts in the local /24 subnet concurrently."""
    base = _local_subnet()
    ips = [f'{base}.{i}' for i in range(1, 255)]
    results: list[WifiBoard] = []

    with ThreadPoolExecutor(max_workers=50) as ex:
        futures = {ex.submit(_probe_ip, ip) for ip in ips}
        try:
            for future in as_completed(futures, timeout=timeout_s):
                try:
                    wb = future.result()
                    if wb is not None:
                        results.append(wb)
                except Exception:
                    pass
        except FuturesTimeout:
            pass

    return results


def discover_boards(timeout_s: float = 5.0) -> list[WifiBoard]:
    """Discover IncuNest boards: mDNS first, subnet scan as fallback."""
    try:
        mdns_results = _discover_mdns(timeout_s / 2)
    except Exception:
        mdns_results = []

    if mdns_results:
        return mdns_results

    return _discover_subnet(timeout_s / 2)
```

- [ ] **Step 4: Ejecutar todos los tests**

```
pytest tests/test_wifi_flasher.py -v
```

Resultado esperado: todos los tests `PASSED`.

- [ ] **Step 5: Commit**

```bash
git add flasher_tool/flasher/wifi_flasher.py flasher_tool/flasher/tests/test_wifi_flasher.py
git commit -m "feat(flasher): add discover_boards with mDNS + subnet scan fallback"
```

---

### Task 4: `main.py` — refactor a tabs (pestaña USB)

**Files:**
- Modify: `flasher_tool/flasher/main.py`

**Interfaces:**
- Consumes: nada nuevo (refactor puro)
- Produces: `FlasherApp` con `ttk.Notebook`, pestaña USB funcional, `_WifiTab` stub vacío

- [ ] **Step 1: Cambiar la geometría de la ventana**

En `FlasherApp.__init__`, cambiar:
```python
self.root.geometry("480x580")
```
por:
```python
self.root.geometry("480x660")
```

- [ ] **Step 2: Reestructurar `_build_ui` para usar Notebook**

Reemplazar el método `_build_ui` completo:

```python
def _build_ui(self) -> None:
    # --- Logo ---
    logo_path = get_logo_path()
    if logo_path.exists():
        img = Image.open(logo_path)
        target_h = 90
        target_w = int(img.width * target_h / img.height)
        img = img.resize((target_w, target_h), Image.LANCZOS)
        self._logo_img = ImageTk.PhotoImage(img)
        tk.Label(self.root, image=self._logo_img).pack(pady=(10, 4))
    else:
        tk.Label(self.root, text="IncuNest", font=('', 16, 'bold'),
                 fg='#1565C0').pack(pady=(10, 4))

    ttk.Separator(self.root, orient='horizontal').pack(fill='x', padx=12)

    # --- Status banner (shared) ---
    self._status_label = tk.Label(
        self.root, text="⏳  Conecta un dispositivo para comenzar…",
        anchor='w', font=('', 10), fg='#757575',
    )
    self._status_label.pack(fill='x', padx=12, pady=(6, 2))

    ttk.Separator(self.root, orient='horizontal').pack(fill='x', padx=12, pady=2)

    # --- Notebook ---
    style = ttk.Style()
    style.configure('Flash.Horizontal.TProgressbar', thickness=18)

    notebook = ttk.Notebook(self.root)
    notebook.pack(fill='x', padx=0, pady=0)

    # USB tab
    usb_frame = tk.Frame(notebook)
    notebook.add(usb_frame, text='  USB  ')
    for i in range(NUM_SLOTS):
        self._slots.append(_Slot(usb_frame, i, 'Flash.Horizontal.TProgressbar'))

    # WiFi tab (stub — logic added in Task 5)
    wifi_frame = tk.Frame(notebook)
    notebook.add(wifi_frame, text='  WiFi  ')
    self._wifi_tab = _WifiTab(wifi_frame, self.root, self._log_line, get_firmware_base())

    # --- Log area (shared, outside notebook) ---
    self._log = scrolledtext.ScrolledText(
        self.root, height=7, state='disabled', font=('Courier', 9),
    )
    self._log.pack(fill='both', expand=True, padx=12, pady=6)
    self._log.tag_config('success', foreground='#2E7D32')
    self._log.tag_config('error',   foreground='#C62828')
    self._log.tag_config('info',    foreground='#1565C0')
```

- [ ] **Step 3: Añadir la clase `_WifiTab` stub antes de `FlasherApp`**

Insertar antes de `class FlasherApp:`:

```python
class _WifiTab:
    """WiFi OTA tab — stub placeholder, logic added in Task 5."""

    def __init__(self, parent: tk.Widget, root: tk.Tk, log_cb, firmware_base: Path) -> None:
        self._root = root
        self._log_cb = log_cb
        self._firmware_base = firmware_base
        self._slots: list[_Slot] = []

        tk.Label(parent, text="WiFi OTA — próximamente",
                 fg='#9E9E9E', font=('', 10)).pack(pady=20)
```

- [ ] **Step 4: Verificar manualmente que la pestaña USB sigue funcionando**

```
python main.py
```

Comprobar:
- La ventana abre con dos pestañas: "USB" y "WiFi"
- La pestaña USB muestra los 3 slots igual que antes
- La pestaña WiFi muestra el texto placeholder
- El log compartido es visible
- Conectar un dispositivo USB → se detecta y flashea igual que antes

- [ ] **Step 5: Commit**

```bash
git add flasher_tool/flasher/main.py
git commit -m "refactor(flasher): add tab layout with USB and WiFi tabs"
```

---

### Task 5: `main.py` — lógica completa de `_WifiTab`

**Files:**
- Modify: `flasher_tool/flasher/main.py`

**Interfaces:**
- Consumes: `discover_boards`, `flash_board_wifi`, `WifiBoard` de `wifi_flasher`; `_Slot`, `NUM_SLOTS`, `SLOT_CLEAR_DELAY_S` de `main.py`
- Produces: `_WifiTab` completamente funcional

- [ ] **Step 1: Reemplazar `_WifiTab` stub por la implementación completa**

Reemplazar la clase `_WifiTab` completa:

```python
class _WifiTab:
    """WiFi OTA tab: discover IncuNest boards on the network and flash them."""

    def __init__(self, parent: tk.Widget, root: tk.Tk, log_cb, firmware_base: Path) -> None:
        self._root = root
        self._log_cb = log_cb
        self._firmware_base = firmware_base
        self._slots: list[_Slot] = []
        self._active_flashes = 0

        # Button + status row
        ctrl_frame = tk.Frame(parent)
        ctrl_frame.pack(fill='x', padx=12, pady=(8, 4))

        self._scan_btn = tk.Button(
            ctrl_frame, text="🔍  Buscar y flashear",
            font=('', 10, 'bold'), bg='#1565C0', fg='white',
            command=self._on_scan_clicked,
        )
        self._scan_btn.pack(side='left')

        self._scan_status = tk.Label(ctrl_frame, text="Listo", anchor='w', fg='#757575')
        self._scan_status.pack(side='left', padx=8)

        ttk.Separator(parent, orient='horizontal').pack(fill='x', padx=12, pady=4)

        # Slots (reuse _Slot with IP as the "port" display string)
        for i in range(NUM_SLOTS):
            self._slots.append(_Slot(parent, i, 'Flash.Horizontal.TProgressbar'))

    def _on_scan_clicked(self) -> None:
        self._scan_btn.configure(state='disabled')
        self._scan_status.configure(text="Escaneando red…", fg='#E65100')
        for slot in self._slots:
            slot.reset()
        self._log_cb("Iniciando búsqueda de dispositivos por WiFi…", 'info')
        threading.Thread(target=self._scan_thread, daemon=True).start()

    def _scan_thread(self) -> None:
        from wifi_flasher import discover_boards
        try:
            boards = discover_boards(timeout_s=5.0)
        except Exception as exc:
            boards = []
            self._root.after(0, self._log_cb, f"Error durante el escaneo: {exc}", 'error')
        self._root.after(0, self._on_scan_done, boards)

    def _on_scan_done(self, boards: list) -> None:
        if not boards:
            self._scan_status.configure(text="No se encontraron dispositivos", fg='#C62828')
            self._log_cb("No se encontraron dispositivos IncuNest en la red.", 'info')
            self._scan_btn.configure(state='normal')
            return

        n = len(boards)
        plural = 's' if n > 1 else ''
        self._scan_status.configure(
            text=f"{n} dispositivo{plural} encontrado{plural}", fg='#2E7D32',
        )
        self._log_cb(f"{n} dispositivo(s) encontrado(s) por WiFi.", 'success')
        self._active_flashes = 0

        for i, wb in enumerate(boards[:NUM_SLOTS]):
            self._slots[i].assign(wb.ip, wb.board)
            self._log_cb(
                f"{wb.board.value} en {wb.ip} (FW {wb.fw_version}) → slot {i + 1}", 'info',
            )
            self._tick_slot(i)
            self._active_flashes += 1
            threading.Thread(
                target=self._flash_thread, args=(wb, i), daemon=True,
            ).start()

    def _tick_slot(self, slot_idx: int) -> None:
        if self._slots[slot_idx].tick():
            self._root.after(500, self._tick_slot, slot_idx)

    def _flash_thread(self, wb, slot_idx: int) -> None:
        from wifi_flasher import flash_board_wifi
        progress_cb = self._make_progress_cb(slot_idx)
        try:
            flash_board_wifi(wb.ip, wb.board, self._firmware_base, progress_cb)
            self._root.after(0, self._on_flash_ok, wb, slot_idx)
        except Exception as exc:
            self._root.after(0, self._on_flash_err, str(exc), wb, slot_idx)

    def _make_progress_cb(self, slot_idx: int):
        def cb(msg: str, pct: Optional[int]) -> None:
            self._root.after(0, self._update_slot_progress, slot_idx, msg, pct)
        return cb

    def _update_slot_progress(self, slot_idx: int, msg: str, pct: Optional[int]) -> None:
        if msg.strip():
            self._log_cb(msg)
        self._slots[slot_idx].update_progress(pct)

    def _on_flash_ok(self, wb, slot_idx: int) -> None:
        self._log_cb(f"¡{wb.board.value} en {wb.ip} flasheado con éxito!", 'success')
        self._slots[slot_idx].set_done()
        self._active_flashes -= 1
        self._root.after(SLOT_CLEAR_DELAY_S * 1000, self._clear_slot, slot_idx)
        if self._active_flashes <= 0:
            self._scan_status.configure(text="Completado", fg='#2E7D32')
            self._scan_btn.configure(state='normal')

    def _on_flash_err(self, msg: str, wb, slot_idx: int) -> None:
        self._log_cb(f"Flash WiFi de {wb.board.value} ({wb.ip}) fallido.\n{msg}", 'error')
        self._slots[slot_idx].set_error()
        self._active_flashes -= 1
        self._root.after(SLOT_CLEAR_DELAY_S * 1000, self._clear_slot, slot_idx)
        if self._active_flashes <= 0:
            self._scan_btn.configure(state='normal')

    def _clear_slot(self, slot_idx: int) -> None:
        self._slots[slot_idx].reset()
```

- [ ] **Step 2: Verificar que los tests existentes siguen pasando**

```
pytest tests/ -v
```

Resultado esperado: todos los tests previos `PASSED`, sin regresiones.

- [ ] **Step 3: Verificar manualmente la pestaña WiFi**

```
python main.py
```

Pruebas con el hotspot activo:
1. Ir a la pestaña "WiFi"
2. Pulsar "Buscar y flashear"
3. El label cambia a "Escaneando red…" y el botón se deshabilita
4. Tras ~5s, el label muestra "N dispositivo(s) encontrado(s)"
5. Los slots se asignan con las IPs correspondientes
6. El flash arranca (barra animada), progresa y termina con ✅
7. El botón se re-habilita tras completar todos los flashes
8. El log muestra los mensajes del proceso

Prueba de error (dispositivo apagado a mitad):
- El slot muestra ❌ y el botón se re-habilita

- [ ] **Step 4: Commit**

```bash
git add flasher_tool/flasher/main.py
git commit -m "feat(flasher): add WiFi OTA tab with discover and flash flow"
```
