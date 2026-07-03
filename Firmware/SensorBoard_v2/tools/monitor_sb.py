#!/usr/bin/env python3
"""Monitor/decodificador del protocolo USB del SensorBoard (IncuNest).

Frame: Magic(0xAB 0xCD) + Type(1B) + Length(4B LE) + Payload + CRC16(2B BE)
CRC16-CCITT FALSE (poly 0x1021, init 0xFFFF) sobre Type+Length+Payload.
TYPE 0x00 = JSON; TYPE 0x01 = JPEG (se guarda a disco).

Uso:
    python monitor_sb.py COM36                # escuchar y decodificar
    python monitor_sb.py COM36 --status       # enviar status al arrancar
    python monitor_sb.py COM36 --capture      # pedir una captura JPEG

Interactivo (escribe y Enter mientras escucha):
    status | capture | foo (cmd desconocido) | quit

Requiere: pip install pyserial
"""
import argparse
import datetime
import json
import struct
import sys
import threading

import serial

MAGIC = b"\xab\xcd"
TYPE_JSON = 0x00
TYPE_JPEG = 0x01


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1) & 0xFFFF
    return crc


def encode_frame(ftype: int, payload: bytes) -> bytes:
    body = bytes([ftype]) + struct.pack("<I", len(payload)) + payload
    return MAGIC + body + struct.pack(">H", crc16_ccitt(body))


def send_cmd(ser: serial.Serial, cmd: str, cmd_id: int) -> None:
    payload = json.dumps({"type": "cmd", "cmd": cmd, "id": cmd_id}).encode()
    ser.write(encode_frame(TYPE_JSON, payload))
    print(f">> cmd {cmd} (id={cmd_id})")


class Decoder:
    """Máquina de estados espejo de sensorBoard_frame.c."""

    def __init__(self):
        self.buf = bytearray()
        self.state = "M0"
        self.ftype = 0
        self.length = 0
        self.payload = bytearray()
        self.crc = bytearray()

    def feed(self, data: bytes):
        for b in data:
            frame = self._feed_byte(b)
            if frame is not None:
                yield frame

    def _feed_byte(self, b: int):
        s = self.state
        if s == "M0":
            self.state = "M1" if b == 0xAB else "M0"
        elif s == "M1":
            self.state = "TYPE" if b == 0xCD else ("M1" if b == 0xAB else "M0")
        elif s == "TYPE":
            self.ftype, self.length, self.lbytes = b, 0, 0
            self.payload, self.crc = bytearray(), bytearray()
            self.state = "LEN"
        elif s == "LEN":
            self.length |= b << (8 * self.lbytes)
            self.lbytes += 1
            if self.lbytes == 4:
                if self.length > 512 * 1024:  # implausible: resync
                    self.state = "M0"
                else:
                    self.state = "PAYLOAD" if self.length else "CRC"
        elif s == "PAYLOAD":
            self.payload.append(b)
            if len(self.payload) == self.length:
                self.state = "CRC"
        elif s == "CRC":
            self.crc.append(b)
            if len(self.crc) == 2:
                self.state = "M0"
                body = bytes([self.ftype]) + struct.pack("<I", self.length) + bytes(self.payload)
                if struct.unpack(">H", bytes(self.crc))[0] == crc16_ccitt(body):
                    return (self.ftype, bytes(self.payload))
                print("!! frame con CRC inválido descartado")
        return None


def handle_frame(ftype: int, payload: bytes) -> None:
    ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
    if ftype == TYPE_JPEG:
        name = f"capture_{datetime.datetime.now():%Y%m%d_%H%M%S}.jpg"
        with open(name, "wb") as f:
            f.write(payload)
        print(f"[{ts}] << JPEG {len(payload)} B -> {name}")
        return
    try:
        msg = json.loads(payload)
    except json.JSONDecodeError:
        print(f"[{ts}] << JSON inválido: {payload[:80]!r}")
        return
    kind = msg.get("type", "?")
    if kind == "log":
        print(f"[{ts}] LOG   {msg.get('msg', '')}")
    elif kind == "event":
        print(f"[{ts}] EVENT {msg.get('cmd', '?')}: "
              f"{json.dumps({k: v for k, v in msg.items() if k not in ('type', 'cmd')})}")
    elif kind == "resp":
        print(f"[{ts}] RESP  {json.dumps(msg)}")
    else:
        print(f"[{ts}] <<    {json.dumps(msg)}")


def stdin_loop(ser: serial.Serial, next_id):
    for line in sys.stdin:
        cmd = line.strip()
        if not cmd:
            continue
        if cmd in ("quit", "exit", "q"):
            print("cerrando...")
            ser.close()
            return
        send_cmd(ser, cmd, next_id())


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("port", help="puerto serie (p. ej. COM36)")
    ap.add_argument("--status", action="store_true", help="enviar status al arrancar")
    ap.add_argument("--capture", action="store_true", help="pedir una captura JPEG al arrancar")
    args = ap.parse_args()

    ser = serial.Serial(args.port, baudrate=115200, timeout=0.2)  # baudrate irrelevante en CDC
    print(f"escuchando {args.port} — comandos: status | capture | quit")

    counter = {"id": 0}

    def next_id():
        counter["id"] += 1
        return counter["id"]

    threading.Thread(target=stdin_loop, args=(ser, next_id), daemon=True).start()

    if args.status:
        send_cmd(ser, "status", next_id())
    if args.capture:
        send_cmd(ser, "capture", next_id())

    dec = Decoder()
    try:
        while ser.is_open:
            data = ser.read(4096)
            if data:
                for ftype, payload in dec.feed(data):
                    handle_frame(ftype, payload)
    except (serial.SerialException, OSError):
        pass
    except KeyboardInterrupt:
        print("\ninterrumpido")


if __name__ == "__main__":
    main()
