#!/usr/bin/env python3
"""Prueba de la GUARDA DE PLACA en la OTA por WiFi.

El 2026-09-08 el flasher subio `motherboard/firmware.bin` a un Display HMI por
WiFi: pantalla violeta, sin tactil, sin enlace, y solo recuperable por USB. La
guarda que se anadio despues tiene DOS barreras, y esta prueba las ejercita por
separado porque protegen de cosas distintas:

  1. La CABECERA `X-IncuNest-Board` (o el argumento `board`): la herramienta
     declara a que placa cree que habla. Corta antes de abrir la flash. Es
     barata, pero solo protege de una herramienta honesta que se equivoca.
  2. La MARCA DENTRO DEL BINARIO: el firmware busca `IncuNestFW:<placa>` en el
     flujo segun lo recibe. Es la que protege de verdad, porque no depende de
     que nadie declare nada. Un binario SIN marca se acepta a proposito (para
     poder volver a una version anterior).

Y hay una tercera comprobacion que importa tanto como las otras dos: que una
OTA LEGITIMA siga entrando. Una guarda que rechaza todo no es una guarda.

    python board_guard_test.py --mb 192.168.137.35 --hmi 192.168.137.222
    python board_guard_test.py ... --skip-legit   # sin reiniciar las placas

Credenciales por INCUNEST_WEB_USER / INCUNEST_WEB_PASS (ver bench_tests.py).
"""

import argparse
import base64
import os
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

BOUNDARY = "----IncuNestBoardGuardTest"


def post_firmware(host, user, password, path, declared_board=None, timeout=240):
    """Sube un .bin a /update. Devuelve (status, cuerpo)."""
    payload = Path(path).read_bytes()
    head = (
        f"--{BOUNDARY}\r\n"
        f'Content-Disposition: form-data; name="update"; filename="{Path(path).name}"\r\n'
        f"Content-Type: application/octet-stream\r\n\r\n"
    ).encode()
    tail = f"\r\n--{BOUNDARY}--\r\n".encode()
    body = head + payload + tail

    req = urllib.request.Request(f"http://{host}/update", data=body, method="POST")
    token = base64.b64encode(f"{user}:{password}".encode()).decode()
    req.add_header("Authorization", f"Basic {token}")
    req.add_header("Content-Type", f"multipart/form-data; boundary={BOUNDARY}")
    req.add_header("Content-Length", str(len(body)))
    if declared_board is not None:
        req.add_header("X-IncuNest-Board", declared_board)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as exc:
        return exc.code, exc.read().decode("utf-8", "replace")


def wait_alive(host, user, password, timeout_s=180):
    token = base64.b64encode(f"{user}:{password}".encode()).decode()
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            req = urllib.request.Request(f"http://{host}/get_fw_version")
            req.add_header("Authorization", f"Basic {token}")
            with urllib.request.urlopen(req, timeout=5) as r:
                if r.status == 200:
                    return r.read().decode("utf-8", "replace")
        except Exception:  # noqa: BLE001
            pass
        time.sleep(3)
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mb", required=True)
    ap.add_argument("--hmi", required=True)
    ap.add_argument("--user", default=os.environ.get("INCUNEST_WEB_USER", "incunest"))
    ap.add_argument("--password", default=os.environ.get("INCUNEST_WEB_PASS", "changeme"))
    ap.add_argument("--root", default=str(Path(__file__).resolve().parents[2]),
                    help="raiz de Firmware/ (por defecto, deducida del script)")
    ap.add_argument("--skip-legit", action="store_true",
                    help="no probar la OTA legitima (esa reinicia la placa)")
    args = ap.parse_args()

    root = Path(args.root)
    mb_bin = root / "motherBoard" / "build" / "motherboard.bin"
    hmi_bin = root / "Display_HMI" / "build" / "display_hmi.bin"
    for b in (mb_bin, hmi_bin):
        if not b.is_file():
            ap.error(f"no existe {b}")

    passed = failed = 0

    def check(name, ok, detail):
        nonlocal passed, failed
        if ok:
            passed += 1
            print(f"[ OK ] {name}\n       {detail}")
        else:
            failed += 1
            print(f"[FALLA] {name}\n       {detail}")

    print(f"MB  = {args.mb}   ({mb_bin.stat().st_size} B)")
    print(f"HMI = {args.hmi}   ({hmi_bin.stat().st_size} B)\n")

    # --- Barrera 1: la cabecera ---------------------------------------------
    st, body = post_firmware(args.hmi, args.user, args.password, mb_bin,
                             declared_board="motherboard")
    check("barrera 1 (cabecera): el display rechaza un binario declarado de la placa",
          st == 400 and "display_hmi" in body, f"HTTP {st}: {body.strip()[:120]}")

    st, body = post_firmware(args.mb, args.user, args.password, hmi_bin,
                             declared_board="display_hmi")
    check("barrera 1 (cabecera): la placa rechaza un binario declarado del display",
          st == 400 and "motherboard" in body, f"HTTP {st}: {body.strip()[:120]}")

    # --- Barrera 2: la marca dentro del binario ------------------------------
    # SIN cabecera: es el caso que de verdad importa, porque reproduce a una
    # herramienta que no declara nada (o que declara mal por otro motivo).
    st, body = post_firmware(args.hmi, args.user, args.password, mb_bin,
                             declared_board=None)
    check("barrera 2 (marca): el display rechaza el binario de la placa sin cabecera",
          st == 400 and "otra placa" in body, f"HTTP {st}: {body.strip()[:120]}")

    st, body = post_firmware(args.mb, args.user, args.password, hmi_bin,
                             declared_board=None)
    check("barrera 2 (marca): la placa rechaza el binario del display sin cabecera",
          st == 400 and "otra placa" in body, f"HTTP {st}: {body.strip()[:120]}")

    # --- Y que lo legitimo SIGA ENTRANDO -------------------------------------
    if args.skip_legit:
        print("\n(se salta la OTA legitima: --skip-legit)")
    else:
        print("\nSubiendo el firmware CORRECTO al display (reinicia la placa)...")
        st, body = post_firmware(args.hmi, args.user, args.password, hmi_bin,
                                 declared_board="display_hmi")
        ok = st == 200
        detail = f"HTTP {st}: {body.strip()[:120]}"
        if ok:
            ver = wait_alive(args.hmi, args.user, args.password)
            ok = ver is not None
            detail += f" | tras reiniciar: {ver or 'NO VUELVE'}"
        check("la OTA legitima sigue entrando (una guarda que rechaza todo no vale)",
              ok, detail)

    print(f"\n{passed} OK, {failed} fallan")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
