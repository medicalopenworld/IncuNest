import json
import re
import shutil
import sys
import time
import threading
from pathlib import Path
from typing import Optional
import tkinter as tk
from tkinter import ttk, scrolledtext

import serial.tools.list_ports
from PIL import Image, ImageTk

from detector import (
    Board, find_all_board_ports, board_from_vid_pid,
    is_ambiguous_native_usb, is_sensorboard_app_port,
    detect_board_ambiguous, BoardDetectionError,
)
from flasher import flash_board, has_firmware_flashed
from updater import check_update_available, download_latest

HOTPLUG_POLL_S = 0.5
POST_FLASH_COOLDOWN_S = 8
SLOT_CLEAR_DELAY_S = 5
NUM_SLOTS = 3
WIFI_SLOTS = 8
WIFI_SCAN_INTERVAL_S = 5


def get_firmware_base() -> Path:
    if getattr(sys, 'frozen', False):
        return Path(sys.executable).parent / 'data' / 'firmware'
    return Path(__file__).parent.parent / 'data' / 'firmware'


def get_logo_path() -> Path:
    if getattr(sys, 'frozen', False):
        return Path(getattr(sys, '_MEIPASS', '')) / 'logo' / 'IncuNest_logo.png'
    return Path(__file__).parent / 'logo' / 'IncuNest_logo.png'


def load_config() -> dict:
    if getattr(sys, 'frozen', False):
        path = Path(sys.executable).parent / 'flasher_config.json'
    else:
        path = Path(__file__).parent.parent / 'flasher_config.json'
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except Exception:
        return {}


class _Slot:
    """One device flash slot: board label + progress bar + status line."""

    def __init__(self, parent: tk.Widget, index: int, style_name: str) -> None:
        self.port: Optional[str] = None
        self.board: Optional[Board] = None
        self._indeterminate = False
        self._start_time: Optional[float] = None
        self._status_override: Optional[str] = None  # overrides tick default text

        self._frame = tk.LabelFrame(
            parent, text=f"Slot {index + 1}",
            padx=8, pady=4, font=('', 9, 'bold'),
        )
        self._frame.pack(fill='x', padx=12, pady=4)

        self._title = tk.Label(
            self._frame, text="—  Esperando dispositivo…",
            anchor='w', fg='#9E9E9E',
        )
        self._title.pack(fill='x')

        self._progress_var = tk.DoubleVar()
        self._bar = ttk.Progressbar(
            self._frame, variable=self._progress_var, maximum=100,
            style=style_name,
        )
        self._bar.pack(fill='x', pady=(4, 2))

        self._status = tk.Label(self._frame, text="", anchor='w', fg='#9E9E9E')
        self._status.pack(fill='x')

    @property
    def is_free(self) -> bool:
        return self.port is None

    def assign(self, port: str, board: Board) -> None:
        self.port = port
        self.board = board
        self._start_time = time.time()
        self._title.configure(text=f"{board.value}  ·  {port}", fg='#000000')
        self._progress_var.set(0)
        self._bar.configure(mode='indeterminate')
        self._bar.start(12)
        self._indeterminate = True
        self._status.configure(text="⚡  Iniciando…  0s", fg='#E65100')

    def set_status(self, text: str) -> None:
        """Set a persistent status override shown by tick() instead of 'Iniciando…'."""
        self._status_override = text
        self._status.configure(text=text, fg='#E65100')

    def tick(self) -> bool:
        """Refresh elapsed time while in indeterminate phase. Returns True if still active."""
        if self._start_time is None or not self._indeterminate:
            return False
        if self._status_override:
            self._status.configure(text=self._status_override, fg='#E65100')
        else:
            self._status.configure(
                text=f"⚡  Iniciando…  {self._elapsed()}",
                fg='#E65100',
            )
        return True

    def update_progress(self, pct: Optional[int]) -> None:
        if pct is None:
            return
        if self._indeterminate:
            self._bar.stop()
            self._bar.configure(mode='determinate')
            self._indeterminate = False
        self._progress_var.set(pct)
        self._status.configure(text=f"⚡  Flasheando…  {pct}%  ({self._elapsed()})", fg='#E65100')

    def set_done(self) -> None:
        self._stop_indeterminate()
        self._progress_var.set(100)
        self._title.configure(fg='#2E7D32')
        self._status.configure(text=f"✅  Completado en {self._elapsed()}", fg='#2E7D32')

    def set_error(self) -> None:
        self._stop_indeterminate()
        self._title.configure(fg='#C62828')
        self._status.configure(text=f"❌  Error ({self._elapsed()}) — revisa el log", fg='#C62828')

    def reset(self) -> None:
        self._stop_indeterminate()
        self.port = None
        self.board = None
        self._start_time = None
        self._status_override = None
        self._title.configure(text="—  Esperando dispositivo…", fg='#9E9E9E')
        self._progress_var.set(0)
        self._status.configure(text="", fg='#9E9E9E')

    def _elapsed(self) -> str:
        if self._start_time is None:
            return '0s'
        s = int(time.time() - self._start_time)
        return f'{s}s' if s < 60 else f'{s // 60}m{s % 60:02d}s'

    def _stop_indeterminate(self) -> None:
        if self._indeterminate:
            self._bar.stop()
            self._bar.configure(mode='determinate')
            self._indeterminate = False


class _SerialNumberDialog:
    """Modal dialog that asks for a serial number (0-9999) before flashing a motherBoard."""

    def __init__(self, parent: tk.Tk, port: str) -> None:
        self.result: Optional[int] = None

        top = tk.Toplevel(parent)
        top.title("Número de serie")
        top.resizable(False, False)
        top.transient(parent)
        top.grab_set()

        tk.Label(top, text=f"motherBoard detectada en {port}.",
                 font=('', 10, 'bold')).pack(padx=24, pady=(18, 4))
        tk.Label(top, text="Introduce el número de serie:").pack(padx=24)

        vcmd = (top.register(self._validate), '%P')
        self._var = tk.StringVar(value='')
        entry = tk.Entry(top, textvariable=self._var, width=10,
                         validate='key', validatecommand=vcmd,
                         font=('', 16), justify='center')
        entry.pack(padx=24, pady=8)
        tk.Label(top, text="(0 – 9999)", fg='#757575').pack()

        btn_frame = tk.Frame(top)
        btn_frame.pack(padx=24, pady=(12, 18))
        tk.Button(btn_frame, text="Cancelar", width=10,
                  command=top.destroy).pack(side='left', padx=4)
        self._ok_btn = tk.Button(btn_frame, text="Aceptar", width=10,
                                 command=self._on_ok,
                                 bg='#1565C0', fg='white', font=('', 9, 'bold'))
        self._ok_btn.pack(side='left', padx=4)

        entry.focus_set()
        top.bind('<Return>', lambda _: self._on_ok())
        top.bind('<Escape>', lambda _: top.destroy())

        parent.update_idletasks()
        x = parent.winfo_rootx() + (parent.winfo_width()  - 300) // 2
        y = parent.winfo_rooty() + (parent.winfo_height() - 200) // 2
        top.geometry(f"300x195+{x}+{y}")

        self._top = top
        parent.wait_window(top)

    @staticmethod
    def _validate(value: str) -> bool:
        if value == '':
            return True
        if not value.isdigit():
            return False
        return len(value) <= 4 and int(value) <= 9999

    def _on_ok(self) -> None:
        val = self._var.get().strip()
        if val == '' or not val.isdigit():
            return
        n = int(val)
        if 0 <= n <= 9999:
            self.result = n
            self._top.destroy()


class _WifiDeviceSlot:
    """Compact two-line device card in the WiFi tab."""

    _STATE_EMPTY = 'empty'
    _STATE_AVAILABLE = 'available'
    _STATE_FLASHING = 'flashing'
    _STATE_DONE = 'done'
    _STATE_ERROR = 'error'

    def __init__(self, parent: tk.Widget, style_name: str, flash_cb) -> None:
        self._wb = None
        self._flash_cb = flash_cb
        self._state = self._STATE_EMPTY
        self._indeterminate = False

        row = tk.Frame(parent, relief='groove', bd=1)
        row.pack(fill='x', padx=8, pady=1)

        info = tk.Frame(row)
        info.pack(side='left', fill='both', expand=True, padx=6, pady=2)

        # Line 1: board + IP
        self._title = tk.Label(info, text="—  Sin dispositivo",
                               anchor='w', fg='#BDBDBD', font=('', 9))
        self._title.pack(fill='x')

        # Line 2: SN + FW + status (all in one compact label)
        self._detail = tk.Label(info, text='', anchor='w',
                                fg='#9E9E9E', font=('', 8))
        self._detail.pack(fill='x')

        # Progress bar — only packed when flashing
        self._bar_var = tk.DoubleVar()
        self._bar = ttk.Progressbar(info, variable=self._bar_var,
                                    maximum=100, style=style_name, length=1)
        self._bar_visible = False

        self._flash_btn = tk.Button(
            row, text="⚡", font=('', 9), width=3,
            bg='#1565C0', fg='white',
            command=self._on_flash_clicked,
            state='disabled',
        )
        self._flash_btn.pack(side='right', padx=4, pady=4)

    # ------------------------------------------------------------------ #

    @property
    def wb(self):
        return self._wb

    @property
    def is_busy(self) -> bool:
        return self._state == self._STATE_FLASHING

    def assign(self, wb) -> None:
        self._wb = wb
        self._state = self._STATE_AVAILABLE
        sn_str = f"SN:{wb.sn}  " if wb.sn is not None else ""
        self._title.configure(text=f"{wb.board.value}  ·  {wb.ip}", fg='#212121')
        self._detail.configure(text=f"{sn_str}FW {wb.fw_version}  |  Disponible",
                               fg='#2E7D32')
        self._flash_btn.configure(state='normal')
        self._hide_bar()

    def clear(self) -> None:
        self._stop_bar()
        self._wb = None
        self._state = self._STATE_EMPTY
        self._title.configure(text="—  Sin dispositivo", fg='#BDBDBD')
        self._detail.configure(text='', fg='#9E9E9E')
        self._flash_btn.configure(state='disabled')
        self._hide_bar()

    def set_flashing(self) -> None:
        self._state = self._STATE_FLASHING
        self._flash_btn.configure(state='disabled')
        self._show_bar()
        self._bar.configure(mode='indeterminate')
        self._bar.start(12)
        self._indeterminate = True
        self._detail.configure(text="⚡ Conectando…", fg='#E65100')

    def update_progress(self, pct: Optional[int]) -> None:
        if pct is None:
            return
        if self._indeterminate:
            self._bar.stop()
            self._bar.configure(mode='determinate')
            self._indeterminate = False
        self._bar_var.set(pct)
        self._detail.configure(text=f"⚡ Flasheando… {pct}%", fg='#E65100')

    def set_done(self) -> None:
        self._stop_bar()
        self._state = self._STATE_DONE
        self._bar_var.set(100)
        sn_str = f"SN:{self._wb.sn}  " if self._wb and self._wb.sn is not None else ""
        fw = self._wb.fw_version if self._wb else ''
        self._detail.configure(text=f"{sn_str}FW {fw}  |  ✅ Completado", fg='#2E7D32')

    def set_error(self) -> None:
        self._stop_bar()
        self._state = self._STATE_ERROR
        sn_str = f"SN:{self._wb.sn}  " if self._wb and self._wb.sn is not None else ""
        fw = self._wb.fw_version if self._wb else ''
        self._detail.configure(text=f"{sn_str}FW {fw}  |  ❌ Error", fg='#C62828')
        self._flash_btn.configure(state='normal')

    # ------------------------------------------------------------------ #

    def _on_flash_clicked(self) -> None:
        if self._wb:
            self._flash_cb(self._wb)

    def _show_bar(self) -> None:
        if not self._bar_visible:
            self._bar.pack(fill='x')
            self._bar_visible = True

    def _hide_bar(self) -> None:
        if self._bar_visible:
            self._bar.pack_forget()
            self._bar_visible = False

    def _stop_bar(self) -> None:
        if self._indeterminate:
            self._bar.stop()
            self._bar.configure(mode='determinate')
            self._indeterminate = False


class _WifiTab:
    """WiFi OTA tab: 8 device slots with auto-scan and per-device flash."""

    def __init__(self, parent: tk.Widget, root: tk.Tk, log_cb, firmware_base: Path) -> None:
        self._root = root
        self._log_cb = log_cb
        self._firmware_base = firmware_base
        self._slots: list[_WifiDeviceSlot] = []
        self._flash_queue: list = []
        self._flashing = False
        self._scanning = False
        self._auto_scan_id = None

        # ── Control row ──────────────────────────────────────────────── #
        ctrl = tk.Frame(parent)
        ctrl.pack(fill='x', padx=8, pady=(8, 4))

        if sys.platform == 'win32':
            from hotspot import is_supported, HOTSPOT_SSID
            self._hotspot_btn = tk.Button(
                ctrl, text=f"📡 {HOTSPOT_SSID}",
                font=('', 9), bg='#37474F', fg='white',
                command=self._on_hotspot_clicked,
                state='normal' if is_supported() else 'disabled',
            )
            self._hotspot_btn.pack(side='left', padx=(0, 4))
            self._hotspot_status = tk.Label(ctrl, text='', fg='#757575', font=('', 8))
            self._hotspot_status.pack(side='left', padx=(0, 6))
        else:
            self._hotspot_btn = None
            self._hotspot_status = None

        self._scan_btn = tk.Button(
            ctrl, text="🔍 Buscar",
            font=('', 9), bg='#455A64', fg='white',
            command=self._on_scan_clicked,
        )
        self._scan_btn.pack(side='left', padx=(0, 4))

        self._flash_all_btn = tk.Button(
            ctrl, text="⚡ Flash All",
            font=('', 9, 'bold'), bg='#1565C0', fg='white',
            command=self._on_flash_all_clicked,
            state='disabled',
        )
        self._flash_all_btn.pack(side='left')

        self._scan_status = tk.Label(ctrl, text="Auto-scan activo",
                                     anchor='w', fg='#757575', font=('', 8))
        self._scan_status.pack(side='left', padx=8)

        ttk.Separator(parent, orient='horizontal').pack(fill='x', padx=8, pady=(4, 2))

        # ── 8 device slots ────────────────────────────────────────────── #
        for _ in range(WIFI_SLOTS):
            self._slots.append(
                _WifiDeviceSlot(parent, 'Flash.Horizontal.TProgressbar',
                                self._on_flash_single)
            )

        # Kick off the first automatic scan after UI is ready
        self._root.after(500, self._do_scan)

    # ── Hotspot ──────────────────────────────────────────────────────── #

    def _on_hotspot_clicked(self) -> None:
        self._hotspot_btn.configure(state='disabled')
        self._hotspot_status.configure(text="Iniciando…", fg='#E65100')
        threading.Thread(target=self._hotspot_thread, daemon=True).start()

    def _hotspot_thread(self) -> None:
        from hotspot import start_hotspot, HOTSPOT_SSID, HOTSPOT_PASSWORD
        try:
            start_hotspot(HOTSPOT_SSID, HOTSPOT_PASSWORD)
            self._root.after(0, self._on_hotspot_ok)
        except Exception as exc:
            self._root.after(0, self._on_hotspot_err, str(exc))

    def _on_hotspot_ok(self) -> None:
        from hotspot import HOTSPOT_SSID
        self._hotspot_status.configure(text=f"✅ {HOTSPOT_SSID} activo", fg='#2E7D32')
        self._hotspot_btn.configure(state='normal')
        self._log_cb(f"Hotspot '{HOTSPOT_SSID}' iniciado.", 'success')

    def _on_hotspot_err(self, msg: str) -> None:
        self._hotspot_status.configure(text="❌ Error", fg='#C62828')
        self._hotspot_btn.configure(state='normal')
        self._log_cb(f"Error al crear hotspot:\n{msg}", 'error')

    # ── Scanning ─────────────────────────────────────────────────────── #

    def _on_scan_clicked(self) -> None:
        if self._scanning:
            return
        if self._auto_scan_id is not None:
            self._root.after_cancel(self._auto_scan_id)
            self._auto_scan_id = None
        self._do_scan()

    def _do_scan(self) -> None:
        self._scanning = True
        self._scan_btn.configure(state='disabled')
        self._scan_status.configure(text="Escaneando…", fg='#E65100')
        threading.Thread(target=self._scan_thread, daemon=True).start()

    def _scan_thread(self) -> None:
        from wifi_flasher import discover_boards
        try:
            boards = discover_boards(timeout_s=5.0)
        except Exception as exc:
            boards = []
            self._root.after(0, self._log_cb, f"Error escaneo WiFi: {exc}", 'error')
        self._root.after(0, self._on_scan_done, boards)

    def _on_scan_done(self, boards: list) -> None:
        self._scanning = False
        self._scan_btn.configure(state='normal')

        boards_by_ip = {wb.ip: wb for wb in boards}

        # Remove stale devices from non-busy slots
        for slot in self._slots:
            if not slot.is_busy and slot.wb is not None:
                if slot.wb.ip not in boards_by_ip:
                    slot.clear()

        # Assign newly found devices to free slots
        occupied_ips = {s.wb.ip for s in self._slots if s.wb is not None}
        free_slots = [s for s in self._slots if s.wb is None]
        for wb in boards:
            if wb.ip not in occupied_ips and free_slots:
                free_slots.pop(0).assign(wb)
                occupied_ips.add(wb.ip)

        n_found = sum(1 for s in self._slots if s.wb is not None)
        if n_found:
            self._scan_status.configure(
                text=f"{n_found} dispositivo(s) • auto-scan", fg='#2E7D32',
            )
            if not self._flashing:
                self._flash_all_btn.configure(state='normal')
        else:
            self._scan_status.configure(text="Sin dispositivos • auto-scan", fg='#C62828')
            self._flash_all_btn.configure(state='disabled')

        if not self._flashing:
            self._auto_scan_id = self._root.after(
                WIFI_SCAN_INTERVAL_S * 1000, self._do_scan,
            )

    # ── Flashing ─────────────────────────────────────────────────────── #

    def _on_flash_single(self, wb) -> None:
        self._start_flash_queue([wb])

    def _on_flash_all_clicked(self) -> None:
        from detector import Board as _Board
        wbs = [s.wb for s in self._slots if s.wb is not None and not s.is_busy]
        wbs_sorted = sorted(wbs, key=lambda wb: 0 if wb.board == _Board.DISPLAY_HMI else 1)
        if wbs_sorted:
            self._start_flash_queue(wbs_sorted)

    def _start_flash_queue(self, boards: list) -> None:
        self._flashing = True
        self._flash_all_btn.configure(state='disabled')
        self._flash_queue = list(boards)
        self._flash_next()

    def _flash_next(self) -> None:
        if not self._flash_queue:
            self._flashing = False
            n_found = sum(1 for s in self._slots if s.wb is not None)
            if n_found:
                self._flash_all_btn.configure(state='normal')
            # Resume auto-scan
            self._auto_scan_id = self._root.after(
                WIFI_SCAN_INTERVAL_S * 1000, self._do_scan,
            )
            return
        wb = self._flash_queue.pop(0)
        slot = next((s for s in self._slots if s.wb and s.wb.ip == wb.ip), None)
        if slot is None:
            self._flash_next()
            return
        slot.set_flashing()
        slot_idx = self._slots.index(slot)
        threading.Thread(
            target=self._flash_thread, args=(wb, slot_idx), daemon=True,
        ).start()

    def _flash_thread(self, wb, slot_idx: int) -> None:
        from wifi_flasher import flash_board_wifi

        def progress_cb(msg: str, pct: Optional[int]) -> None:
            if msg.strip():
                self._root.after(0, self._log_cb, msg, '')
            if pct is not None:
                self._root.after(0, self._slots[slot_idx].update_progress, pct)

        try:
            flash_board_wifi(wb.ip, wb.board, self._firmware_base, progress_cb)
            self._root.after(0, self._on_flash_ok, wb, slot_idx)
        except Exception as exc:
            self._root.after(0, self._on_flash_err, str(exc), wb, slot_idx)

    def _on_flash_ok(self, wb, slot_idx: int) -> None:
        self._log_cb(f"¡{wb.board.value} en {wb.ip} flasheado con éxito!", 'success')
        self._slots[slot_idx].set_done()
        self._flash_next()

    def _on_flash_err(self, msg: str, wb, slot_idx: int) -> None:
        self._log_cb(f"Flash WiFi de {wb.board.value} ({wb.ip}) fallido.\n{msg}", 'error')
        self._slots[slot_idx].set_error()
        self._flash_next()


class FlasherApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("IncuNest Firmware Flasher")
        self.root.geometry("480x820")
        self.root.resizable(False, False)

        self._slots: list[_Slot] = []
        self._port_to_slot: dict[str, int] = {}
        self._cooldown_until: dict[str, float] = {}
        self._sb_hint_mute_until: float = 0.0
        self._known_ports: set = set()

        cfg = load_config()
        self._force_serial_number_entry: bool = bool(cfg.get('force_serial_number', False))
        self._force_download: bool = bool(cfg.get('force_download_latest_firmware', False))
        self._downloading: bool = False

        self._build_ui()
        self._init_hotplug()
        if self._force_download:
            self.root.after(500, self._start_update_check)

    # ------------------------------------------------------------------ #
    # UI construction
    # ------------------------------------------------------------------ #

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

        # WiFi tab
        wifi_frame = tk.Frame(notebook)
        notebook.add(wifi_frame, text='  WiFi  ')
        self._wifi_tab = _WifiTab(wifi_frame, self.root, self._log_line, get_firmware_base())

        # --- "Actualizar binarios locales" — packed at bottom BEFORE log so it
        #     isn't swallowed by the log's expand=True fill. ---
        self._upd_btn: Optional[tk.Button] = None
        self._upd_status: Optional[tk.Label] = None
        if self._get_build_sources():
            ttk.Separator(self.root, orient='horizontal').pack(
                side='bottom', fill='x', padx=12,
            )
            upd_frame = tk.Frame(self.root)
            upd_frame.pack(side='bottom', fill='x', padx=12, pady=(6, 8))
            self._upd_btn = tk.Button(
                upd_frame, text="📂  Actualizar binarios locales",
                font=('', 10, 'bold'), bg='#37474F', fg='white',
                padx=12, pady=6,
                command=self._on_update_locals_clicked,
            )
            self._upd_btn.pack(side='left')
            self._upd_status = tk.Label(upd_frame, text='', anchor='w',
                                        fg='#757575', font=('', 10))
            self._upd_status.pack(side='left', padx=10)

        # --- Log area (shared, outside notebook) ---
        self._log = scrolledtext.ScrolledText(
            self.root, height=5, state='disabled', font=('Courier', 9),
        )
        self._log.pack(fill='both', expand=True, padx=12, pady=6)
        self._log.tag_config('success', foreground='#2E7D32')
        self._log.tag_config('error',   foreground='#C62828')
        self._log.tag_config('info',    foreground='#1565C0')

    # ------------------------------------------------------------------ #
    # Hotplug monitor
    # ------------------------------------------------------------------ #

    @staticmethod
    def _is_flasher_port(vid: Optional[int], pid: Optional[int]) -> bool:
        """True for any port this tool might care about, resolved or not.

        Includes the ambiguous native-USB PID (motherBoard in any state, or a
        virgin motherBoard/SensorBoard) — that still needs a flash-size probe
        to know which board it actually is, done in _hotplug_loop — and the
        running-SensorBoard CDC PID, which is only tracked to advise the user.
        """
        return (
            board_from_vid_pid(vid, pid) is not None
            or is_ambiguous_native_usb(vid, pid)
            or is_sensorboard_app_port(vid, pid)
        )

    def _init_hotplug(self) -> None:
        self._known_ports = {
            p.device for p in serial.tools.list_ports.comports()
            if self._is_flasher_port(p.vid, p.pid)
        }
        threading.Thread(target=self._hotplug_loop, daemon=True).start()

    def _hotplug_loop(self) -> None:
        while True:
            time.sleep(HOTPLUG_POLL_S)
            try:
                current: dict[str, object] = {
                    p.device: p for p in serial.tools.list_ports.comports()
                    if self._is_flasher_port(p.vid, p.pid)
                }
            except Exception:
                continue
            new_devices = {d: p for d, p in current.items() if d not in self._known_ports}
            self._known_ports = set(current.keys())
            for device, port_info in sorted(new_devices.items()):
                if is_sensorboard_app_port(port_info.vid, port_info.pid):
                    # SensorBoard firmware running (TinyUSB CDC): esptool can't
                    # reset it into download mode, so don't even try — tell the
                    # user how to get there. Muted right after a successful
                    # SensorBoard flash, when the freshly written app boots
                    # and enumerates on exactly this PID.
                    if time.time() >= self._sb_hint_mute_until:
                        self.root.after(
                            0, self._log_line,
                            f"SensorBoard con firmware en ejecución en {device} — no se "
                            "puede flashear así. Para reflashearla: desconecta, mantén "
                            "pulsado BOOT (IO0) mientras la conectas y no lo sueltes "
                            "hasta que el slot empiece a escribir.", 'info',
                        )
                    continue
                board = board_from_vid_pid(port_info.vid, port_info.pid)
                if board is None and is_ambiguous_native_usb(port_info.vid, port_info.pid):
                    # Blocking flash-size probe (~1-2s) — safe here, this loop
                    # already runs off the Tk main thread.
                    try:
                        board = detect_board_ambiguous(device)
                    except BoardDetectionError as exc:
                        self.root.after(
                            0, self._log_line,
                            f"No se pudo identificar el dispositivo en {device}: {exc}", 'error',
                        )
                        continue
                if board is None:
                    continue
                self.root.after(0, self._on_hotplug_detected, device, board)

    def _on_hotplug_detected(self, port: str, board: Board) -> None:
        if time.time() < self._cooldown_until.get(port, 0.0):
            self._log_line(
                f"{board.value} en {port} ignorado (enfriamiento post-flash).", 'info'
            )
            return
        if port in self._port_to_slot:
            return  # already being processed

        if self._downloading:
            self._log_line(
                f"{board.value} en {port} ignorado — descarga de firmware en curso.", 'info'
            )
            return

        slot_idx = next((i for i, s in enumerate(self._slots) if s.is_free), None)
        if slot_idx is None:
            self._log_line(
                f"{board.value} en {port} ignorado — todos los slots ocupados.", 'info'
            )
            return

        # Reserve slot and show immediately so the user sees activity
        self._port_to_slot[port] = slot_idx
        self._slots[slot_idx].assign(port, board)
        self._log_line(f"{board.value} detectado en {port} → slot {slot_idx + 1}", 'info')
        self._update_status_banner()
        self._tick_slot(slot_idx)

        if board == Board.MOTHERBOARD and self._force_serial_number_entry:
            # Always ask for serial number regardless of existing firmware
            self._slots[slot_idx].set_status('📋  Introduce el serial…')
            dlg = _SerialNumberDialog(self.root, port)
            self._slots[slot_idx].set_status('')
            if dlg.result is None:
                self._slots[slot_idx].reset()
                del self._port_to_slot[port]
                self._log_line(f"Flasheo de {board.value} cancelado.", 'info')
                self._update_status_banner()
                return
            self._run_flash(port, board, slot_idx, dlg.result)
        elif board == Board.MOTHERBOARD:
            # Auto-detect: check firmware presence, preserve NVS if already flashed
            threading.Thread(
                target=self._check_firmware_present,
                args=(port, board, slot_idx),
                daemon=True,
            ).start()
        else:
            self._run_flash(port, board, slot_idx, None)

    def _check_firmware_present(self, port: str, board: Board, slot_idx: int) -> None:
        self._slots[slot_idx].set_status('🔍  Leyendo dispositivo…')
        firmware_present = has_firmware_flashed(port)
        self.root.after(0, self._on_firmware_check_done, port, board, slot_idx, firmware_present)

    def _on_firmware_check_done(self, port: str, board: Board, slot_idx: int,
                                firmware_present: bool) -> None:
        self._slots[slot_idx].set_status('')
        if firmware_present:
            self._log_line("Firmware existente — serial conservado.", 'info')
            self._run_flash(port, board, slot_idx, None)
        else:
            # Virgin board — ask for serial number before flashing
            self._slots[slot_idx].set_status('📋  Introduce el serial…')
            dlg = _SerialNumberDialog(self.root, port)
            self._slots[slot_idx].set_status('')
            if dlg.result is None:
                self._slots[slot_idx].reset()
                del self._port_to_slot[port]
                self._log_line(f"Flasheo de {board.value} cancelado.", 'info')
                self._update_status_banner()
                return
            self._run_flash(port, board, slot_idx, dlg.result)

    def _tick_slot(self, slot_idx: int) -> None:
        if self._slots[slot_idx].tick():
            self.root.after(500, self._tick_slot, slot_idx)

    # ------------------------------------------------------------------ #
    # Flashing flow
    # ------------------------------------------------------------------ #

    def _run_flash(self, port: str, board: Board, slot_idx: int,
                   serial_number: Optional[int] = None) -> None:
        progress_cb = self._make_progress_cb(slot_idx)
        threading.Thread(
            target=self._flash_thread,
            args=(port, board, slot_idx, progress_cb, serial_number),
            daemon=True,
        ).start()

    def _make_progress_cb(self, slot_idx: int):
        def cb(msg: str, pct: Optional[int]) -> None:
            self.root.after(0, self._update_slot_progress, slot_idx, msg, pct)
        return cb

    def _flash_thread(self, port: str, board: Board, slot_idx: int,
                      progress_cb, serial_number: Optional[int] = None) -> None:
        try:
            flash_board(port, board, get_firmware_base(), progress_cb, serial_number)
            self.root.after(0, self._on_flash_ok, port, board, slot_idx)
        except Exception as exc:
            self.root.after(0, self._on_flash_err, str(exc), port, board, slot_idx)

    def _update_slot_progress(self, slot_idx: int, msg: str, pct: Optional[int]) -> None:
        if msg.strip():
            self._log_line(msg)
        self._slots[slot_idx].update_progress(pct)

    def _on_flash_ok(self, port: str, board: Board, slot_idx: int) -> None:
        self._log_line(f"¡{board.value} flasheado con éxito!", 'success')
        self._slots[slot_idx].set_done()
        self._cooldown_until[port] = time.time() + POST_FLASH_COOLDOWN_S
        if board == Board.SENSORBOARD:
            self._sb_hint_mute_until = time.time() + POST_FLASH_COOLDOWN_S
        self._update_status_banner()
        self.root.after(SLOT_CLEAR_DELAY_S * 1000, self._clear_slot, slot_idx, port)

    def _on_flash_err(self, msg: str, port: str, board: Board, slot_idx: int) -> None:
        self._log_line(f"Flasheo de {board.value} fallido.\n{msg}", 'error')
        self._slots[slot_idx].set_error()
        self._update_status_banner()
        self.root.after(SLOT_CLEAR_DELAY_S * 1000, self._clear_slot, slot_idx, port)

    def _clear_slot(self, slot_idx: int, port: str) -> None:
        self._slots[slot_idx].reset()
        self._port_to_slot.pop(port, None)
        self._update_status_banner()

    # ------------------------------------------------------------------ #
    # Firmware update check
    # ------------------------------------------------------------------ #

    def _start_update_check(self) -> None:
        self._log_line("Buscando actualizaciones de firmware…", 'info')
        threading.Thread(target=self._update_check_thread, daemon=True).start()

    def _update_check_thread(self) -> None:
        available, latest = check_update_available(get_firmware_base())
        self.root.after(0, self._on_update_check_done, available, latest)

    def _on_update_check_done(self, available: bool, latest: Optional[str]) -> None:
        if not available:
            msg = f"Firmware al día ({latest})." if latest else "Sin conexión — usando firmware local."
            self._log_line(msg, 'info')
            return
        self._log_line(f"Nueva versión disponible: {latest}. Descargando…", 'info')
        self._status_label.configure(text=f"⬇  Descargando firmware {latest}…", fg='#1565C0')
        self._downloading = True
        threading.Thread(target=self._download_thread, args=(latest,), daemon=True).start()

    def _download_thread(self, latest: str) -> None:
        def progress_cb(asset_name: str, downloaded: int, total: int) -> None:
            self.root.after(0, self._on_download_progress, asset_name, downloaded, total)

        success = download_latest(get_firmware_base(), progress_cb)
        self.root.after(0, self._on_download_done, success, latest)

    def _on_download_progress(self, asset_name: str, downloaded: int, total: int) -> None:
        if total > 0:
            pct = int(downloaded / total * 100)
            short = asset_name.replace('_', ' ').replace('.bin', '')
            self._status_label.configure(text=f"⬇  {short}… {pct}%", fg='#1565C0')

    def _on_download_done(self, success: bool, latest: str) -> None:
        self._downloading = False
        if success:
            self._log_line(f"Firmware {latest} descargado correctamente.", 'success')
        else:
            self._log_line("Error al descargar — usando binarios locales.", 'error')
        self._update_status_banner()

    # ------------------------------------------------------------------ #

    def _update_status_banner(self) -> None:
        active = sum(1 for s in self._slots if not s.is_free)
        if active == 0:
            self._status_label.configure(
                text="⏳  Conecta un dispositivo para comenzar…", fg='#757575',
            )
        else:
            n = str(active)
            self._status_label.configure(
                text=f"⚡  Flasheando {n} dispositivo{'s' if active > 1 else ''}…",
                fg='#E65100',
            )

    def _log_line(self, msg: str, tag: str = '') -> None:
        self._log.configure(state='normal')
        self._log.insert('end', '> ' + msg.strip() + '\n', tag)
        self._log.see('end')
        self._log.configure(state='disabled')

    # ------------------------------------------------------------------ #
    # Local firmware update
    # ------------------------------------------------------------------ #

    def _get_build_sources(self) -> dict[str, dict[str, Path]]:
        """Return {board_folder → {dest_filename → local build output}}.

        Covers both build layouts in this repo: PlatformIO (per-env dirs,
        already-generic filenames) for motherBoard/Display HMI, and plain
        ESP-IDF (single build/ dir, project-named app binary) for SensorBoard.
        """
        def _env_sort_key(p: Path) -> tuple:
            # Prefer higher version number (e.g. V17 > V16); mtime as tiebreaker.
            m = re.search(r'[Vv](\d+)', p.parent.name)
            return (int(m.group(1)) if m else -1, p.stat().st_mtime)

        try:
            firmware_base = get_firmware_base()
            repo_root = firmware_base.parents[2]
            sources: dict[str, dict[str, Path]] = {}

            for board_folder, pio_dir in [
                ('display_hmi', repo_root / 'Display_HMI' / '.pio' / 'build'),
                ('motherboard', repo_root / 'motherBoard' / '.pio' / 'build'),
            ]:
                if not pio_dir.is_dir():
                    continue
                candidates = sorted(
                    pio_dir.glob('*/firmware.bin'),
                    key=_env_sort_key,
                    reverse=True,
                )
                if candidates:
                    sources[board_folder] = {'firmware.bin': candidates[0]}

            sb_build = repo_root / 'SensorBoard_v2' / 'build'
            sb_files = {
                'firmware.bin': sb_build / 'SensorBoard.bin',
                'bootloader.bin': sb_build / 'bootloader' / 'bootloader.bin',
                'partitions.bin': sb_build / 'partition_table' / 'partition-table.bin',
            }
            sb_present = {name: p for name, p in sb_files.items() if p.is_file()}
            if 'firmware.bin' in sb_present:
                sources['sensorboard'] = sb_present

            return sources
        except Exception:
            return {}

    def _on_update_locals_clicked(self) -> None:
        if self._upd_btn:
            self._upd_btn.configure(state='disabled')
        sources = self._get_build_sources()
        if not sources:
            if self._upd_status:
                self._upd_status.configure(text='No se encontraron binarios', fg='#C62828')
            if self._upd_btn:
                self._upd_btn.configure(state='normal')
            return
        firmware_base = get_firmware_base()
        copied: list[str] = []
        for board_folder, files in sources.items():
            for dest_name, src in files.items():
                dst = firmware_base / board_folder / dest_name
                shutil.copy2(src, dst)
                self._log_line(f"Copiado: {src.name} → {dst}", 'info')
            copied.append(board_folder)
        if self._upd_status:
            self._upd_status.configure(text=f"✓ {', '.join(copied)}", fg='#2E7D32')
        if self._upd_btn:
            self._upd_btn.configure(state='normal')


if __name__ == '__main__':
    root = tk.Tk()
    FlasherApp(root)
    root.mainloop()
