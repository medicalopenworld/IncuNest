import sys
import time
import threading
from pathlib import Path
from typing import Optional
import tkinter as tk
from tkinter import ttk, scrolledtext

import serial.tools.list_ports

from detector import detect_board, Board, BoardDetectionError
from flasher import flash_board

ESPRESSIF_VID = 0x303A
HOTPLUG_POLL_S = 0.5
# After a successful flash the ESP32 resets and briefly re-enumerates.
# Ignore new connections on the same port for this many seconds.
POST_FLASH_COOLDOWN_S = 8


def get_firmware_base() -> Path:
    if getattr(sys, 'frozen', False):
        return Path(sys.executable).parent / 'firmware'
    return Path(__file__).parent / 'firmware'


class FlasherApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("IncuNest Firmware Flasher")
        self.root.geometry("480x450")
        self.root.resizable(False, False)

        self._detected_board: Optional[Board] = None
        self._hotplug_triggered = False
        self._cooldown_until = 0.0
        self._known_esp_ports: set = set()

        self._build_ui()
        self._init_hotplug()

    # ------------------------------------------------------------------ #
    # UI construction
    # ------------------------------------------------------------------ #

    def _build_ui(self) -> None:
        pad = {'padx': 12, 'pady': 4}

        # --- Status banner ---
        self._status_label = tk.Label(
            self.root, text="⏳  Esperando dispositivo…",
            anchor='w', font=('', 10), fg='#757575',
        )
        self._status_label.pack(fill='x', padx=12, pady=(8, 2))

        ttk.Separator(self.root, orient='horizontal').pack(
            fill='x', padx=12, pady=2
        )

        # --- Port row (manual fallback) ---
        port_frame = tk.Frame(self.root)
        port_frame.pack(fill='x', **pad)
        tk.Label(port_frame, text="Puerto COM:").pack(side='left')
        self._port_var = tk.StringVar()
        self._port_combo = ttk.Combobox(
            port_frame, textvariable=self._port_var, width=12, state='readonly'
        )
        self._port_combo.pack(side='left', padx=6)
        tk.Button(port_frame, text="↺", width=3,
                  command=self._refresh_ports).pack(side='left')
        self._detect_btn = tk.Button(
            port_frame, text="Detectar",
            command=self._on_detect_manual,
        )
        self._detect_btn.pack(side='left', padx=6)

        # --- Board info ---
        self._board_label = tk.Label(self.root, text="Placa:  —", anchor='w')
        self._board_label.pack(fill='x', **pad)

        # --- Flash button ---
        self._flash_btn = tk.Button(
            self.root, text="FLASHEAR",
            command=self._on_flash_manual, width=22,
            state='disabled',
            bg='#1565C0', fg='white',
            font=('', 11, 'bold'),
            disabledforeground='#90A4AE',
        )
        self._flash_btn.pack(**pad)

        # --- Progress bar + percentage ---
        self._progress_var = tk.DoubleVar()
        ttk.Progressbar(
            self.root, variable=self._progress_var, maximum=100,
        ).pack(fill='x', padx=12, pady=2)
        self._pct_label = tk.Label(self.root, text="")
        self._pct_label.pack()

        # --- Log area ---
        self._log = scrolledtext.ScrolledText(
            self.root, height=9, state='disabled', font=('Courier', 9)
        )
        self._log.pack(fill='both', expand=True, padx=12, pady=6)
        self._log.tag_config('success', foreground='#2E7D32')
        self._log.tag_config('error',   foreground='#C62828')
        self._log.tag_config('info',    foreground='#1565C0')

        self._refresh_ports()

    # ------------------------------------------------------------------ #
    # Hotplug monitor
    # ------------------------------------------------------------------ #

    def _init_hotplug(self) -> None:
        self._known_esp_ports = {
            p.device for p in serial.tools.list_ports.comports()
            if p.vid == ESPRESSIF_VID
        }
        threading.Thread(target=self._hotplug_loop, daemon=True).start()

    def _hotplug_loop(self) -> None:
        while True:
            time.sleep(HOTPLUG_POLL_S)
            try:
                current = {
                    p.device for p in serial.tools.list_ports.comports()
                    if p.vid == ESPRESSIF_VID
                }
            except Exception:
                continue
            new_ports = current - self._known_esp_ports
            self._known_esp_ports = current
            for port in sorted(new_ports):
                self.root.after(0, self._on_hotplug_detected, port)

    def _on_hotplug_detected(self, port: str) -> None:
        if time.time() < self._cooldown_until:
            self._log_line(
                f"Nuevo ESP32 en {port} ignorado (enfriamiento post-flash).", 'info'
            )
            return
        if self._detect_btn['state'] == 'disabled':
            self._log_line(
                f"Nuevo ESP32 en {port} ignorado (operación en curso).", 'info'
            )
            return

        self._log_line(f"ESP32-S3 detectado automáticamente en {port}", 'info')
        existing = list(self._port_combo['values'])
        if port not in existing:
            existing.insert(0, port)
            self._port_combo['values'] = existing
        self._port_var.set(port)

        self._status_label.configure(
            text=f"🔌  Dispositivo en {port} — detectando…", fg='#1565C0'
        )
        self._hotplug_triggered = True
        self._run_detect(port)

    # ------------------------------------------------------------------ #
    # Manual controls
    # ------------------------------------------------------------------ #

    def _refresh_ports(self) -> None:
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self._port_combo['values'] = ports
        if ports and not self._port_var.get():
            self._port_combo.current(0)

    def _on_detect_manual(self) -> None:
        port = self._port_var.get()
        if not port:
            self._log_line("Selecciona un puerto COM.", 'error')
            return
        self._hotplug_triggered = False
        self._run_detect(port)

    def _on_flash_manual(self) -> None:
        port = self._port_var.get()
        if not port or self._detected_board is None:
            return
        self._hotplug_triggered = False
        self._run_flash(port, self._detected_board)

    # ------------------------------------------------------------------ #
    # Detection flow
    # ------------------------------------------------------------------ #

    def _run_detect(self, port: str) -> None:
        self._detect_btn.configure(state='disabled')
        self._flash_btn.configure(state='disabled')
        self._detected_board = None
        self._board_label.configure(text="Placa:  —")
        self._log_line(f"Conectando a {port}…")
        threading.Thread(
            target=self._detect_thread, args=(port,), daemon=True
        ).start()

    def _detect_thread(self, port: str) -> None:
        try:
            board = detect_board(port)
            self.root.after(0, self._on_detect_ok, board, port)
        except Exception as exc:
            self.root.after(0, self._on_detect_err, str(exc))

    def _on_detect_ok(self, board: Board, port: str) -> None:
        self._detected_board = board
        self._board_label.configure(text=f"Placa:  {board.value}")
        self._log_line(f"Placa identificada: {board.value}", 'success')
        self._detect_btn.configure(state='normal')
        self._flash_btn.configure(state='normal')
        self._status_label.configure(
            text=f"✅  {board.value} en {port}", fg='#2E7D32'
        )
        if self._hotplug_triggered:
            self._hotplug_triggered = False
            self._run_flash(port, board)

    def _on_detect_err(self, msg: str) -> None:
        self._board_label.configure(text="Error de detección")
        self._log_line(msg, 'error')
        self._detect_btn.configure(state='normal')
        self._status_label.configure(text="❌  Error de detección", fg='#C62828')
        self._hotplug_triggered = False

    # ------------------------------------------------------------------ #
    # Flashing flow
    # ------------------------------------------------------------------ #

    def _run_flash(self, port: str, board: Board) -> None:
        self._flash_btn.configure(state='disabled', text="Flasheando…")
        self._detect_btn.configure(state='disabled')
        self._progress_var.set(0)
        self._pct_label.configure(text="")
        self._status_label.configure(
            text=f"⚡  Flasheando {board.value}…", fg='#E65100'
        )
        self._log_line(f"Iniciando flasheo de {board.value} en {port}…")
        threading.Thread(
            target=self._flash_thread, args=(port, board), daemon=True
        ).start()

    def _flash_thread(self, port: str, board: Board) -> None:
        try:
            flash_board(port, board, get_firmware_base(), self._on_progress)
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
        self._status_label.configure(
            text="✅  ¡Completado! Desconecta la placa.", fg='#2E7D32'
        )
        self._cooldown_until = time.time() + POST_FLASH_COOLDOWN_S

    def _on_flash_err(self, msg: str) -> None:
        self._log_line(f"Flasheo fallido.\n{msg}", 'error')
        self._flash_btn.configure(state='normal', text="FLASHEAR")
        self._detect_btn.configure(state='normal')
        self._status_label.configure(text="❌  Flasheo fallido", fg='#C62828')
        self._hotplug_triggered = False

    # ------------------------------------------------------------------ #

    def _log_line(self, msg: str, tag: str = '') -> None:
        self._log.configure(state='normal')
        self._log.insert('end', '> ' + msg.strip() + '\n', tag)
        self._log.see('end')
        self._log.configure(state='disabled')


if __name__ == '__main__':
    root = tk.Tk()
    FlasherApp(root)
    root.mainloop()
