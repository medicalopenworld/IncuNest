"""
photo_sweep_monitor.py
Escucha los paquetes UDP del sweep de frecuencias de fototerapia (puerto 4210)
y guarda los datos en un CSV con timestamp en el nombre.

Uso:
    python photo_sweep_monitor.py

Formato de trama recibida:
    SWEEP,<millis>,<freq_hz>,<duty>,<current_A>,
          <led2>,<led1>,<aled2>,<aled1>,<led2_sub>,<led1_sub>,
          <spo2>,<hr1>,<hr2>,<hr3>,<rsqi>
    SWEEP,DONE
"""

import socket
import datetime
import csv
import os
import sys

UDP_PORT = 4210
COLUMNS = [
    "timestamp", "millis", "freq_hz", "duty", "current_A",
    "led2", "led1", "aled2", "aled1", "led2_sub", "led1_sub",
    "spo2", "hr1", "hr2", "hr3", "rsqi"
]

# Guardar junto a este script
script_dir = os.path.dirname(os.path.abspath(__file__))
filename = os.path.join(
    script_dir,
    f"SWEEP_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("", UDP_PORT))
sock.settimeout(30)

print(f"Escuchando UDP puerto {UDP_PORT}...")
print(f"Guardando en: {filename}")
print("Enciende la fototerapia en la incubadora para iniciar el sweep.\n")

samples = 0
current_freq = None

with open(filename, "w", newline="", encoding="utf-8") as f:
    writer = csv.writer(f)
    writer.writerow(COLUMNS)

    try:
        while True:
            try:
                data, _ = sock.recvfrom(256)
            except socket.timeout:
                print("Timeout (30s sin datos). Saliendo.")
                break

            line = data.decode(errors="replace").strip()

            if line == "SWEEP,DONE":
                print(f"\nSweep completado. {samples} muestras grabadas.")
                break

            if not line.startswith("SWEEP,"):
                continue

            parts = line.split(",")[1:]   # quitar "SWEEP"
            if len(parts) < 15:
                continue

            ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
            writer.writerow([ts] + parts)
            f.flush()
            samples += 1

            freq = parts[1]
            if freq != current_freq:
                current_freq = freq
                print(f"\n--- {freq} Hz ---")

            duty_pct = round(int(parts[2]) / 255 * 100, 1)
            print(f"  [{ts}] duty={duty_pct}%  I={parts[3]}A  "
                  f"led2={parts[4]}  led1={parts[5]}  "
                  f"spo2={parts[10]}%  hr1={parts[11]}bpm",
                  end="\r", flush=True)

    except KeyboardInterrupt:
        print(f"\nInterrumpido. {samples} muestras grabadas.")

sock.close()
print(f"CSV guardado: {filename}")
