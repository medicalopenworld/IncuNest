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
