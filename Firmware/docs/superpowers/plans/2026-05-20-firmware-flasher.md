# IncuNest Firmware Flasher — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a standalone Windows GUI tool (`IncuNest_Flasher.exe`) that lets non-technical users flash firmware to the IncuNest motherBoard or Display HMI via USB, without any IDE.

**Architecture:** Python + Tkinter GUI packaged as a single `.exe` with PyInstaller. Board auto-detected by reading flash size via `esptool.main()`. Flashing runs in a background thread with real-time progress parsed from esptool stdout. Firmware `.bin` files live in a `firmware/` folder alongside the `.exe`.

**Tech Stack:** Python 3.11+, esptool 4.x, pyserial, Tkinter (stdlib), PyInstaller

**Spec:** `docs/superpowers/specs/2026-05-20-firmware-flasher-design.md`

---

## File map

| File | Responsibility |
|---|---|
| `tools/flasher/detector.py` | Reads flash size via esptool, maps to `Board` enum |
| `tools/flasher/flasher.py` | Builds esptool args, runs write_flash, routes stdout to progress callback |
| `tools/flasher/main.py` | Tkinter window, wires detector + flasher, handles threading |
| `tools/flasher/tests/conftest.py` | Adds `tools/flasher/` to `sys.path` for imports |
| `tools/flasher/tests/test_detector.py` | Unit tests for board detection logic |
| `tools/flasher/tests/test_flasher.py` | Unit tests for flash arg building and progress parsing |
| `tools/flasher/requirements.txt` | Runtime deps: esptool, pyserial |
| `tools/flasher/requirements-dev.txt` | Dev deps: pytest |
| `tools/flasher/flasher.spec` | PyInstaller build spec |
| `tools/flasher/build.bat` | One-click build script |
| `tools/flasher/firmware/motherboard/` | Placeholder dir for motherboard binaries |
| `tools/flasher/firmware/display_hmi/` | Placeholder dir for display HMI binaries |

---

## Task 1: Project scaffold

**Files:**
- Create: `tools/flasher/requirements.txt`
- Create: `tools/flasher/requirements-dev.txt`
- Create: `tools/flasher/tests/conftest.py`
- Create: `tools/flasher/firmware/motherboard/.gitkeep`
- Create: `tools/flasher/firmware/display_hmi/.gitkeep`

- [ ] **Step 1: Create directory structure**

Run from repo root (`Firmware/`):
```powershell
New-Item -ItemType Directory -Force tools\flasher\tests
New-Item -ItemType Directory -Force tools\flasher\firmware\motherboard
New-Item -ItemType Directory -Force tools\flasher\firmware\display_hmi
```

- [ ] **Step 2: Create requirements.txt**

Create `tools/flasher/requirements.txt`:
```
esptool>=4.7.0
pyserial>=3.5
```

- [ ] **Step 3: Create requirements-dev.txt**

Create `tools/flasher/requirements-dev.txt`:
```
pytest>=7.0.0
```

- [ ] **Step 4: Create conftest.py**

Create `tools/flasher/tests/conftest.py`:
```python
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))
```

- [ ] **Step 5: Create firmware placeholder files**

```powershell
New-Item -ItemType File tools\flasher\firmware\motherboard\.gitkeep
New-Item -ItemType File tools\flasher\firmware\display_hmi\.gitkeep
```

- [ ] **Step 6: Install dependencies**

```powershell
pip install -r tools\flasher\requirements.txt -r tools\flasher\requirements-dev.txt
```

Expected output: packages installed without errors.

- [ ] **Step 7: Commit**

```bash
git add tools/flasher/requirements.txt tools/flasher/requirements-dev.txt
git add tools/flasher/tests/conftest.py
git add tools/flasher/firmware/
git commit -m "feat(flasher): scaffold project structure and dependencies"
```

---

## Task 2: Board detector

**Files:**
- Create: `tools/flasher/tests/test_detector.py`
- Create: `tools/flasher/detector.py`

### Context

`detector.py` calls `esptool.main(['--port', port, 'flash_id'])` with stdout redirected to a buffer, then parses the line `Detected flash size: 8MB` from the output.

esptool calls `sys.exit(0)` on success — we catch `SystemExit` and ignore it. A non-zero exit code means a real error.

Flash size → board mapping:
- `8MB` → `Board.MOTHERBOARD`
- `16MB` → `Board.DISPLAY_HMI`
- anything else → `BoardDetectionError`

- [ ] **Step 1: Write failing tests**

Create `tools/flasher/tests/test_detector.py`:
```python
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
```

- [ ] **Step 2: Run tests — verify they fail**

```powershell
pytest tools\flasher\tests\test_detector.py -v
```

Expected: 5 failures with `ModuleNotFoundError: No module named 'detector'`.

- [ ] **Step 3: Implement detector.py**

Create `tools/flasher/detector.py`:
```python
import io
import sys
from enum import Enum

import esptool


class Board(Enum):
    MOTHERBOARD = "motherBoard"
    DISPLAY_HMI = "Display HMI"


class BoardDetectionError(Exception):
    pass


_FLASH_SIZE_MAP = {
    '8MB': Board.MOTHERBOARD,
    '16MB': Board.DISPLAY_HMI,
}


def detect_board(port: str) -> Board:
    flash_size = _read_flash_size(port)
    board = _FLASH_SIZE_MAP.get(flash_size)
    if board is None:
        raise BoardDetectionError(
            f"Placa no reconocida ({flash_size}). Solo se admiten motherBoard y Display HMI."
        )
    return board


def _read_flash_size(port: str) -> str:
    buf = io.StringIO()
    old_stdout, old_stderr = sys.stdout, sys.stderr
    sys.stdout = buf
    sys.stderr = buf
    try:
        esptool.main(['--port', port, 'flash_id'])
    except SystemExit as e:
        if e.code != 0:
            raise BoardDetectionError(
                f"No se pudo conectar al puerto {port}. "
                "Comprueba el cable y que no esté abierto en otro programa."
            )
    finally:
        sys.stdout = old_stdout
        sys.stderr = old_stderr

    output = buf.getvalue()
    for line in output.splitlines():
        if 'Detected flash size' in line:
            return line.split(':')[-1].strip()

    raise BoardDetectionError(
        "No se pudo detectar el tamaño de flash. "
        "Comprueba que la placa esté conectada correctamente."
    )
```

- [ ] **Step 4: Run tests — verify they pass**

```powershell
pytest tools\flasher\tests\test_detector.py -v
```

Expected:
```
test_detector.py::test_detects_motherboard_for_8mb PASSED
test_detector.py::test_detects_display_hmi_for_16mb PASSED
test_detector.py::test_raises_for_unrecognized_size PASSED
test_detector.py::test_raises_when_flash_size_line_absent PASSED
test_detector.py::test_raises_on_nonzero_exit PASSED
5 passed
```

- [ ] **Step 5: Commit**

```bash
git add tools/flasher/detector.py tools/flasher/tests/test_detector.py
git commit -m "feat(flasher): add board detector — identifies board by flash size"
```

---

## Task 3: Flasher logic

**Files:**
- Create: `tools/flasher/tests/test_flasher.py`
- Create: `tools/flasher/flasher.py`

### Context

`flasher.py` builds the esptool args list for `write_flash` and calls `esptool.main()` with stdout redirected. It routes each output line (and parsed percentage) to a `progress_callback(msg: str, pct: int | None)`.

Flash addresses from partition CSV files:

| Board | File | Address |
|---|---|---|
| motherboard | bootloader.bin | `0x0000` |
| motherboard | partitions.bin | `0x8000` |
| motherboard | firmware.bin | `0x10000` |
| display_hmi | bootloader.bin | `0x0000` |
| display_hmi | partitions.bin | `0x8000` |
| display_hmi | ota_data_initial.bin | `0xE000` |
| display_hmi | firmware.bin | `0x10000` |

esptool progress lines look like: `Writing at 0x00010000... (67 %)`

- [ ] **Step 1: Write failing tests**

Create `tools/flasher/tests/test_flasher.py`:
```python
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
```

- [ ] **Step 2: Run tests — verify they fail**

```powershell
pytest tools\flasher\tests\test_flasher.py -v
```

Expected: failures with `ModuleNotFoundError: No module named 'flasher'`.

- [ ] **Step 3: Implement flasher.py**

Create `tools/flasher/flasher.py`:
```python
import io
import re
import sys
from pathlib import Path
from typing import Callable, Optional

import esptool

from detector import Board


_BOARD_FOLDER = {
    Board.MOTHERBOARD: 'motherboard',
    Board.DISPLAY_HMI: 'display_hmi',
}

_BOARD_FILES = {
    Board.MOTHERBOARD: [
        ('0x0000', 'bootloader.bin'),
        ('0x8000', 'partitions.bin'),
        ('0x10000', 'firmware.bin'),
    ],
    Board.DISPLAY_HMI: [
        ('0x0000', 'bootloader.bin'),
        ('0x8000', 'partitions.bin'),
        ('0xE000', 'ota_data_initial.bin'),
        ('0x10000', 'firmware.bin'),
    ],
}


def flash_board(
    port: str,
    board: Board,
    firmware_base: Path,
    progress_callback: Callable[[str, Optional[int]], None],
) -> None:
    folder = firmware_base / _BOARD_FOLDER[board]
    file_pairs = _BOARD_FILES[board]

    args = [
        '--port', port,
        '--chip', 'esp32s3',
        '--baud', '460800',
        'write_flash',
        '--flash_mode', 'qio',
        '--flash_size', 'detect',
    ]

    for addr, filename in file_pairs:
        filepath = folder / filename
        if not filepath.exists():
            raise FileNotFoundError(f"Archivo no encontrado: {filepath}")
        args += [addr, str(filepath)]

    class _ProgressWriter(io.StringIO):
        def write(self, text: str) -> int:
            result = super().write(text)
            if text.strip():
                progress_callback(text.rstrip(), _parse_percent(text))
            return result

    writer = _ProgressWriter()
    old_stdout, old_stderr = sys.stdout, sys.stderr
    sys.stdout = writer
    sys.stderr = writer
    try:
        esptool.main(args)
    except SystemExit as e:
        if e.code != 0:
            raise RuntimeError(
                f"esptool terminó con error (código {e.code}). "
                "Flasheo fallido. Vuelve a intentarlo."
            )
    finally:
        sys.stdout = old_stdout
        sys.stderr = old_stderr


def _parse_percent(text: str) -> Optional[int]:
    m = re.search(r'\((\d+)\s*%\)', text)
    return int(m.group(1)) if m else None
```

- [ ] **Step 4: Run tests — verify they pass**

```powershell
pytest tools\flasher\tests\test_flasher.py -v
```

Expected:
```
test_flasher.py::test_motherboard_flash_includes_correct_addresses PASSED
test_flasher.py::test_display_hmi_flash_includes_ota_data_address PASSED
test_flasher.py::test_flash_passes_port_to_esptool PASSED
test_flasher.py::test_progress_callback_receives_percentage PASSED
test_flasher.py::test_raises_file_not_found_for_missing_binary PASSED
test_flasher.py::test_parse_percent_extracts_integer PASSED
test_flasher.py::test_parse_percent_returns_none_for_non_progress_line PASSED
7 passed
```

- [ ] **Step 5: Run all tests**

```powershell
pytest tools\flasher\tests\ -v
```

Expected: 12 passed.

- [ ] **Step 6: Commit**

```bash
git add tools/flasher/flasher.py tools/flasher/tests/test_flasher.py
git commit -m "feat(flasher): add flash logic with per-board file map and progress callback"
```

---

## Task 4: GUI (main.py)

**Files:**
- Create: `tools/flasher/main.py`

### Context

Tkinter window, 480×420 px, non-resizable. All GUI updates from background threads go through `root.after(0, fn)` — Tkinter is not thread-safe.

`get_firmware_base()` returns the path to `firmware/` relative to the `.exe` when frozen by PyInstaller (`sys.frozen == True`), or relative to `main.py` when running from source.

- [ ] **Step 1: Create main.py**

Create `tools/flasher/main.py`:
```python
import sys
import threading
from pathlib import Path
from typing import Optional
import tkinter as tk
from tkinter import ttk, scrolledtext

import serial.tools.list_ports

from detector import detect_board, Board, BoardDetectionError
from flasher import flash_board


def get_firmware_base() -> Path:
    if getattr(sys, 'frozen', False):
        return Path(sys.executable).parent / 'firmware'
    return Path(__file__).parent / 'firmware'


class FlasherApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("IncuNest Firmware Flasher")
        self.root.geometry("480x420")
        self.root.resizable(False, False)
        self._detected_board: Optional[Board] = None
        self._build_ui()
        self._refresh_ports()

    def _build_ui(self) -> None:
        pad = {'padx': 12, 'pady': 5}

        # --- Port row ---
        port_frame = tk.Frame(self.root)
        port_frame.pack(fill='x', **pad)
        tk.Label(port_frame, text="Puerto COM:").pack(side='left')
        self._port_var = tk.StringVar()
        self._port_combo = ttk.Combobox(
            port_frame, textvariable=self._port_var, width=12, state='readonly'
        )
        self._port_combo.pack(side='left', padx=6)
        tk.Button(port_frame, text="↺ Actualizar",
                  command=self._refresh_ports).pack(side='left')

        # --- Detect button ---
        self._detect_btn = tk.Button(
            self.root, text="Detectar placa",
            command=self._on_detect, width=22
        )
        self._detect_btn.pack(**pad)

        # --- Board info label ---
        self._board_label = tk.Label(
            self.root, text="Placa detectada:  —", anchor='w'
        )
        self._board_label.pack(fill='x', **pad)

        # --- Flash button ---
        self._flash_btn = tk.Button(
            self.root, text="FLASHEAR",
            command=self._on_flash, width=22,
            state='disabled',
            bg='#1565C0', fg='white',
            font=('', 11, 'bold'),
            disabledforeground='#90A4AE',
        )
        self._flash_btn.pack(**pad)

        # --- Progress bar + percentage ---
        self._progress_var = tk.DoubleVar()
        ttk.Progressbar(
            self.root, variable=self._progress_var, maximum=100
        ).pack(fill='x', padx=12, pady=2)
        self._pct_label = tk.Label(self.root, text="")
        self._pct_label.pack()

        # --- Log area ---
        self._log = scrolledtext.ScrolledText(
            self.root, height=9, state='disabled', font=('Courier', 9)
        )
        self._log.pack(fill='both', expand=True, padx=12, pady=6)
        self._log.tag_config('success', foreground='#2E7D32')
        self._log.tag_config('error', foreground='#C62828')

    # ------------------------------------------------------------------ #

    def _refresh_ports(self) -> None:
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self._port_combo['values'] = ports
        if ports:
            self._port_combo.current(0)
        msg = "Puertos disponibles: " + (', '.join(ports) if ports else "ninguno")
        self._log_line(msg)

    def _log_line(self, msg: str, tag: str = '') -> None:
        self._log.configure(state='normal')
        self._log.insert('end', '> ' + msg.strip() + '\n', tag)
        self._log.see('end')
        self._log.configure(state='disabled')

    # ------------------------------------------------------------------ #
    # Detection flow
    # ------------------------------------------------------------------ #

    def _on_detect(self) -> None:
        port = self._port_var.get()
        if not port:
            self._log_line("Selecciona un puerto COM.", 'error')
            return
        self._detect_btn.configure(state='disabled')
        self._flash_btn.configure(state='disabled')
        self._detected_board = None
        self._board_label.configure(text="Detectando…")
        self._log_line(f"Conectando a {port}…")
        threading.Thread(
            target=self._detect_thread, args=(port,), daemon=True
        ).start()

    def _detect_thread(self, port: str) -> None:
        try:
            board = detect_board(port)
            self.root.after(0, self._on_detect_ok, board)
        except BoardDetectionError as exc:
            self.root.after(0, self._on_detect_err, str(exc))

    def _on_detect_ok(self, board: Board) -> None:
        self._detected_board = board
        self._board_label.configure(
            text=f"Placa detectada:  {board.value}"
        )
        self._log_line(f"Placa identificada: {board.value}", 'success')
        self._detect_btn.configure(state='normal')
        self._flash_btn.configure(state='normal')

    def _on_detect_err(self, msg: str) -> None:
        self._board_label.configure(text="Error de detección")
        self._log_line(msg, 'error')
        self._detect_btn.configure(state='normal')

    # ------------------------------------------------------------------ #
    # Flashing flow
    # ------------------------------------------------------------------ #

    def _on_flash(self) -> None:
        port = self._port_var.get()
        if not port or self._detected_board is None:
            return
        self._flash_btn.configure(state='disabled', text="Flasheando…")
        self._detect_btn.configure(state='disabled')
        self._progress_var.set(0)
        self._pct_label.configure(text="")
        self._log_line(
            f"Iniciando flasheo de {self._detected_board.value} en {port}…"
        )
        threading.Thread(
            target=self._flash_thread,
            args=(port, self._detected_board),
            daemon=True,
        ).start()

    def _flash_thread(self, port: str, board: Board) -> None:
        try:
            flash_board(
                port, board, get_firmware_base(), self._on_progress
            )
            self.root.after(0, self._on_flash_ok)
        except Exception as exc:
            self.root.after(0, self._on_flash_err, str(exc))

    def _on_progress(self, msg: str, pct: Optional[int]) -> None:
        self.root.after(0, self._update_progress_ui, msg, pct)

    def _update_progress_ui(self, msg: str, pct: Optional[int]) -> None:
        if msg.strip():
            self._log_line(msg)
        if pct is not None:
            self._progress_var.set(pct)
            self._pct_label.configure(text=f"{pct}%")

    def _on_flash_ok(self) -> None:
        self._progress_var.set(100)
        self._pct_label.configure(text="100%")
        self._log_line("¡Flasheo completado con éxito!", 'success')
        self._flash_btn.configure(state='normal', text="FLASHEAR")
        self._detect_btn.configure(state='normal')

    def _on_flash_err(self, msg: str) -> None:
        self._log_line(f"Flasheo fallido. Vuelve a intentarlo.\n{msg}", 'error')
        self._flash_btn.configure(state='normal', text="FLASHEAR")
        self._detect_btn.configure(state='normal')


if __name__ == '__main__':
    root = tk.Tk()
    FlasherApp(root)
    root.mainloop()
```

- [ ] **Step 2: Smoke-test the GUI from source**

Copy a real `firmware.bin`, `bootloader.bin`, and `partitions.bin` from an existing PlatformIO build into `tools/flasher/firmware/motherboard/` (or `display_hmi/`).

```powershell
python tools\flasher\main.py
```

Expected: window opens, port dropdown populated. Connect a board, click "Detectar placa" — board name appears. Click "FLASHEAR" — progress bar fills and log shows esptool output. No freeze during flashing.

- [ ] **Step 3: Commit**

```bash
git add tools/flasher/main.py
git commit -m "feat(flasher): add Tkinter GUI with threaded detect and flash"
```

---

## Task 5: PyInstaller build

**Files:**
- Create: `tools/flasher/flasher.spec`
- Create: `tools/flasher/build.bat`

### Context

The `.exe` is built with `--onefile` mode (single file, no unpacking folder). The `firmware/` directory is **not** embedded — it sits next to the `.exe` in the distribution zip, so firmware files can be updated without rebuilding.

`console=False` hides the terminal window. esptool has many internal modules; the hidden imports list below covers all required ones.

- [ ] **Step 1: Create flasher.spec**

Create `tools/flasher/flasher.spec`:
```python
block_cipher = None

a = Analysis(
    ['main.py'],
    pathex=['.'],
    binaries=[],
    datas=[],
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
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='IncuNest_Flasher',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=None,
)
```

- [ ] **Step 2: Create build.bat**

Create `tools/flasher/build.bat`:
```bat
@echo off
cd /d "%~dp0"
echo Installing dependencies...
pip install -r requirements.txt pyinstaller

echo Building IncuNest_Flasher.exe...
pyinstaller flasher.spec --clean --noconfirm

echo.
echo Build complete.
echo Executable: dist\IncuNest_Flasher.exe
echo.
echo Distribution package:
echo   1. Copy dist\IncuNest_Flasher.exe
echo   2. Copy firmware\ folder (with populated bin files)
echo   3. Zip both together.
pause
```

- [ ] **Step 3: Run the build**

```powershell
cd tools\flasher
.\build.bat
```

Expected output ends with:
```
Building IncuNest_Flasher.exe...
...
Successfully built IncuNest_Flasher.exe
```

File `tools/flasher/dist/IncuNest_Flasher.exe` is created.

- [ ] **Step 4: Test the compiled exe**

Copy `tools/flasher/firmware/` next to the `.exe`:
```powershell
Copy-Item -Recurse tools\flasher\firmware tools\flasher\dist\firmware
```

Double-click `dist\IncuNest_Flasher.exe`. Expected: window opens with no console, same behavior as source run.

If any `ImportError` appears (PyInstaller missing a hidden import), add the missing module to the `hiddenimports` list in `flasher.spec` and rebuild.

- [ ] **Step 5: Add dist/ to .gitignore**

Add to the root `.gitignore` (or create one at `tools/flasher/.gitignore`):
```
dist/
build/
__pycache__/
*.spec.bak
```

- [ ] **Step 6: Commit**

```bash
git add tools/flasher/flasher.spec tools/flasher/build.bat tools/flasher/.gitignore
git commit -m "feat(flasher): add PyInstaller spec and build.bat for Windows exe"
```

---

## Done checklist

- [ ] `pytest tools/flasher/tests/ -v` → 12 tests pass
- [ ] `python tools/flasher/main.py` opens window from source
- [ ] `build.bat` produces `dist/IncuNest_Flasher.exe`
- [ ] Exe detects motherBoard (8MB) and Display HMI (16MB) correctly
- [ ] Progress bar fills during flash, log shows esptool output
- [ ] Flash button stays disabled until detect succeeds
- [ ] No console window when running the exe
