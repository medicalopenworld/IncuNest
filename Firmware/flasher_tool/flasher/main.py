import json
import sys
import time
import threading
from pathlib import Path
from typing import Optional
import tkinter as tk
from tkinter import ttk, scrolledtext

import serial.tools.list_ports
from PIL import Image, ImageTk

from detector import Board, find_all_board_ports, board_from_vid_pid
from flasher import flash_board, has_firmware_flashed
from updater import check_update_available, download_latest

HOTPLUG_POLL_S = 0.5
POST_FLASH_COOLDOWN_S = 8
SLOT_CLEAR_DELAY_S = 5
NUM_SLOTS = 3


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
        boards_to_flash = boards[:NUM_SLOTS]
        self._active_flashes = len(boards_to_flash)

        for i, wb in enumerate(boards_to_flash):
            self._slots[i].assign(wb.ip, wb.board)
            self._log_cb(
                f"{wb.board.value} en {wb.ip} (FW {wb.fw_version}) → slot {i + 1}", 'info',
            )
            self._tick_slot(i)
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


class FlasherApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("IncuNest Firmware Flasher")
        self.root.geometry("480x660")
        self.root.resizable(False, False)

        self._slots: list[_Slot] = []
        self._port_to_slot: dict[str, int] = {}
        self._cooldown_until: dict[str, float] = {}
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

        # --- Log area (shared, outside notebook) ---
        self._log = scrolledtext.ScrolledText(
            self.root, height=7, state='disabled', font=('Courier', 9),
        )
        self._log.pack(fill='both', expand=True, padx=12, pady=6)
        self._log.tag_config('success', foreground='#2E7D32')
        self._log.tag_config('error',   foreground='#C62828')
        self._log.tag_config('info',    foreground='#1565C0')

    # ------------------------------------------------------------------ #
    # Hotplug monitor
    # ------------------------------------------------------------------ #

    def _init_hotplug(self) -> None:
        self._known_ports = {
            p.device for p in serial.tools.list_ports.comports()
            if board_from_vid_pid(p.vid, p.pid) is not None
        }
        threading.Thread(target=self._hotplug_loop, daemon=True).start()

    def _hotplug_loop(self) -> None:
        while True:
            time.sleep(HOTPLUG_POLL_S)
            try:
                current: dict[str, object] = {
                    p.device: p for p in serial.tools.list_ports.comports()
                    if board_from_vid_pid(p.vid, p.pid) is not None
                }
            except Exception:
                continue
            new_devices = {d: p for d, p in current.items() if d not in self._known_ports}
            self._known_ports = set(current.keys())
            for device, port_info in sorted(new_devices.items()):
                board = board_from_vid_pid(port_info.vid, port_info.pid)
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


if __name__ == '__main__':
    root = tk.Tk()
    FlasherApp(root)
    root.mainloop()
