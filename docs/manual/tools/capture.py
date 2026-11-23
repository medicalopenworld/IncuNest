#!/usr/bin/env python3
"""Semi-automatic screenshot capture for the IncuNest user manual.

The HMI (Display_HMI >= 4.10.0) serves the current screen as a 16-bit BMP at
GET /debug/screenshot. This script walks the list in captures.csv: for each
entry it tells the operator which screen state to set up on the touchscreen,
waits for Enter, downloads the screenshot, converts it to PNG and saves it as
docs/manual/<lang>/img/<name>_<lang>.png, the path the chapters reference.

Usage examples:
    python capture.py --mac 98-88-e0-11-cc-64            # whole list, English
    python capture.py --host 192.168.137.239 --missing   # only files not yet taken
    python capture.py --mac ... --only menu,settings_info
    python capture.py --mac ... --name menu              # one capture, no prompts
    python capture.py --list

Credentials: HTTP basic auth of the HMI web server. Read from the environment
variables INCUNEST_WEB_USER and INCUNEST_WEB_PASS, or asked interactively.
They are never written to disk.

Requires Pillow (pip install pillow).
"""

import argparse
import base64
import csv
import getpass
import io
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

try:
    from PIL import Image
except ImportError:  # pragma: no cover - environment check
    sys.exit("Pillow is required: pip install pillow")

TOOLS_DIR = Path(__file__).resolve().parent
MANUAL_DIR = TOOLS_DIR.parent
CAPTURES_CSV = TOOLS_DIR / "captures.csv"
LANGS = ("en", "es", "fr", "pt")
HTTP_TIMEOUT_S = 30  # one capture is ~768 KB and takes ~6-7 s over Wi-Fi


def load_captures():
    with open(CAPTURES_CSV, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def resolve_host_by_mac(mac):
    """Find the IP for a MAC address in the ARP table (Windows `arp -a` format)."""
    wanted = re.sub(r"[^0-9a-f]", "", mac.lower())
    out = subprocess.run(["arp", "-a"], capture_output=True, text=True).stdout
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 2 and re.sub(r"[^0-9a-f]", "", parts[1].lower()) == wanted:
            return parts[0]
    return None


class Hmi:
    def __init__(self, host, user, password):
        self.base = f"http://{host}"
        token = base64.b64encode(f"{user}:{password}".encode()).decode()
        self.auth = f"Basic {token}"

    def _request(self, method, path):
        req = urllib.request.Request(self.base + path, method=method)
        req.add_header("Authorization", self.auth)
        with urllib.request.urlopen(req, timeout=HTTP_TIMEOUT_S) as resp:
            return resp.read()

    def enable_debug_mode(self):
        self._request("POST", "/debug/mode?on=1")

    def screenshot_png(self):
        """Return the current screen as PNG bytes."""
        try:
            data = self._request("GET", "/debug/screenshot")
        except urllib.error.HTTPError as e:
            if e.code != 409:  # 409 = debug mode off (e.g. after a reboot)
                raise
            self.enable_debug_mode()
            data = self._request("GET", "/debug/screenshot")
        img = Image.open(io.BytesIO(data))
        buf = io.BytesIO()
        img.convert("RGB").save(buf, format="PNG", optimize=True)
        return buf.getvalue(), img.size


def out_path(name, lang):
    return MANUAL_DIR / lang / "img" / f"{name}_{lang}.png"


def take(hmi, name, lang):
    png, size = hmi.screenshot_png()
    path = out_path(name, lang)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)
    print(f"  saved {path.relative_to(MANUAL_DIR)} ({size[0]}x{size[1]}, {len(png) // 1024} KB)")


def interactive(hmi, entries, lang):
    print(
        "\nFor each entry: set up the screen, then press\n"
        "  Enter   capture now\n"
        "  w N     wait N seconds, then capture (to touch the screen meanwhile)\n"
        "  s       skip      b  back to the previous entry      q  quit\n"
    )
    i = 0
    while i < len(entries):
        e = entries[i]
        done = " (exists)" if out_path(e["name"], lang).exists() else ""
        print(f"[{i + 1}/{len(entries)}] {e['name']}{done}  — section {e['section']}")
        print(f"  {e['how']}")
        cmd = input("  > ").strip().lower()
        if cmd == "q":
            break
        if cmd == "s":
            i += 1
            continue
        if cmd == "b":
            i = max(0, i - 1)
            continue
        m = re.fullmatch(r"w\s*(\d+)", cmd)
        if m:
            secs = int(m.group(1))
            for left in range(secs, 0, -1):
                print(f"  capturing in {left} s ", end="\r", flush=True)
                time.sleep(1)
            print()
        elif cmd:
            print("  unknown command")
            continue
        try:
            take(hmi, e["name"], lang)
            i += 1
        except Exception as exc:  # keep the session alive on network errors
            print(f"  ERROR: {exc}. Try again (Enter) or skip (s).")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    where = ap.add_mutually_exclusive_group()
    where.add_argument("--host", help="IP address of the touchscreen")
    where.add_argument("--mac", help="MAC address of the touchscreen (looked up with arp -a)")
    ap.add_argument("--lang", default="en", choices=LANGS, help="language suffix and folder (default en); set the same language on the screen")
    ap.add_argument("--only", help="comma-separated capture names")
    ap.add_argument("--missing", action="store_true", help="only captures whose PNG does not exist yet")
    ap.add_argument("--name", help="take this one capture immediately, without prompts")
    ap.add_argument("--list", action="store_true", help="list the captures and whether they exist, then exit")
    args = ap.parse_args()

    entries = load_captures()
    names = {e["name"] for e in entries}

    if args.list:
        for e in entries:
            mark = "x" if out_path(e["name"], args.lang).exists() else " "
            print(f"[{mark}] {e['name']:<28} {e['section']:<9} {e['how']}")
        return

    if args.only:
        wanted = [n.strip() for n in args.only.split(",") if n.strip()]
        unknown = set(wanted) - names
        if unknown:
            sys.exit(f"unknown capture names: {', '.join(sorted(unknown))}")
        entries = [e for e in entries if e["name"] in wanted]
    if args.missing:
        entries = [e for e in entries if not out_path(e["name"], args.lang).exists()]

    host = args.host
    if args.mac:
        host = resolve_host_by_mac(args.mac)
        if not host:
            sys.exit(f"MAC {args.mac} not found in the ARP table (ping the network or use --host)")
    if not host:
        sys.exit("give --host or --mac")

    user = os.environ.get("INCUNEST_WEB_USER") or input("HMI web user: ")
    password = os.environ.get("INCUNEST_WEB_PASS") or getpass.getpass("HMI web password: ")
    hmi = Hmi(host, user, password)

    try:
        hmi.enable_debug_mode()
    except urllib.error.HTTPError as e:
        sys.exit("wrong user or password" if e.code == 401 else f"could not enable debug mode: HTTP {e.code}")
    except OSError as e:
        sys.exit(f"cannot reach the touchscreen at {host}: {e}")
    print(f"Touchscreen at {host}, debug mode on. Screen language must be '{args.lang}'.")

    if args.name:
        if args.name not in names:
            sys.exit(f"unknown capture name: {args.name}")
        take(hmi, args.name, args.lang)
        return

    interactive(hmi, entries, args.lang)


if __name__ == "__main__":
    main()
