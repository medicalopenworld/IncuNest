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
    python capture.py --check                            # check recipes.txt, no screen needed
    python capture.py --mac ... --auto --lang es         # recipes run by themselves
    python capture.py --mac ... --auto --auto-only --debug --only menu,settings_info

Automatic mode (--auto) needs the Display_HMI firmware variant hmi_screenshots
(>= 4.11.0), which adds /debug/touch and /debug/ui. The script sets the screen
language, turns auto-lock off for the session and plays the steps in
recipes.txt; captures without a recipe are still asked for.

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


RECIPES_TXT = TOOLS_DIR / "recipes.txt"
DEBUG_DIR = MANUAL_DIR / "build" / "recipe_debug"

# Touch points (x, y) on the 800x480 screen, read from the English screenshots.
POINTS = {
    # main screen
    "temp_on": (110, 88), "temp_off": (110, 88),
    "photo_on": (676, 88), "photo_off": (676, 88),
    "photo_start": (590, 184), "photo_stop": (751, 184),
    "hum_on": (676, 253), "hum_off": (676, 253),
    "bell": (680, 25), "menu": (756, 25), "lock": (400, 30),
    "eye_turn_on": (403, 332),                       # phototherapy eye-protection popup
    # menu and the full-screen panels opened from it
    "m_date": (108, 235), "m_babies": (300, 235), "m_settings": (497, 235), "m_help": (690, 235),
    "close": (745, 55),                              # red X of menu, help, training...
    # settings
    "s_back": (40, 35), "s_info": (200, 130), "s_wifi": (200, 185), "s_lang": (200, 240),
    "s_modes": (200, 295), "s_maint": (200, 350), "s_hwtest": (200, 405),
    "lang_dropdown": (510, 185),
    "open_reminder": (585, 360), "clean_service": (509, 430), "clean_close": (689, 430),
    "service_close": (689, 430),
    # help
    "h_guided": (143, 235), "h_video": (395, 235), "h_contact": (647, 235), "contact_back": (699, 430),
    # registered babies
    "bb_close": (698, 57), "bb_first": (300, 155), "curve_back": (140, 57), "curve_close": (698, 57),
    "bb_discharge": (589, 155),                      # DISCHARGE of the first active baby
    "reason_unknown": (400, 364),                    # "Reason for discharge" dialog
    # "Incubator idle" dialog (active baby, last function turned off)
    "idle_with_mother": (400, 218), "idle_discharge": (400, 318), "idle_close": (614, 84),
    # temperature wizard
    "wz_close_first": (675, 75), "wz_skip": (190, 402), "wz_new": (550, 402),
    "wz_close": (745, 55), "wz_back": (101, 169), "wz_continue": (679, 169),
    "sum_cancel": (120, 425), "sum_skip": (400, 425), "sum_apply": (679, 425),
    # alarm centre
    "al_test": (145, 57), "al_trend": (277, 57), "al_close": (698, 57),
    "trend_close": (746, 46),
    # lock screen: the wake touch and the padlock fall on empty areas of the
    # main screen, so "unlock" is harmless when the screen is already unlocked
    "lock_wake": (300, 400), "lock_hold": (400, 292),
}
# Language drop-down options (Settings > LANGUAGE): always in this order.
LANG_OPTION_Y = {"es": 236, "en": 268, "fr": 300, "pt": 332}
LANG_OPTION_X = 450

# Wizard keyboards.
_KEY_ROWS = (("QWERTYUIOP", (75, 146, 218, 289, 360, 432, 505, 576, 648, 720), 247),
             ("ASDFGHJKL", (78, 158, 238, 317, 397, 477, 556, 636, 716), 301),
             ("ZXCVBNM", (83, 173, 263, 352, 442, 532, 621), 355))
LETTERS = {ch: (x, y) for keys, xs, y in _KEY_ROWS for ch, x in zip(keys, xs)}
DIGITS = {str(d): ((268, 397, 526)[(d - 1) % 3], (247, 301, 355)[(d - 1) // 3]) for d in range(1, 10)}
DIGITS["0"] = (494, 410)

UNLOCK_HOLD_MS = 2500


def load_captures():
    with open(CAPTURES_CSV, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def load_recipes():
    """Return {capture name: [step, ...]} from recipes.txt."""
    recipes = {}
    for n, line in enumerate(RECIPES_TXT.read_text(encoding="utf-8").splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, sep, steps = line.partition(":")
        if not sep:
            sys.exit(f"recipes.txt line {n}: missing ':'")
        recipes[name.strip()] = [s.strip() for s in steps.split(";") if s.strip()]
    return recipes


def compile_recipe(steps, lang):
    """Turn recipe steps into actions: ("touch", x, y, ms, label), ("wait", s),
    ("ask", text) or ("shot",). Raises ValueError on a bad step."""
    actions = []

    def point(args):
        if len(args) == 2 and all(a.isdigit() for a in args):
            return int(args[0]), int(args[1])
        if len(args) == 1 and args[0] == "lang_current":
            return LANG_OPTION_X, LANG_OPTION_Y[lang]
        if len(args) == 1 and args[0] in POINTS:
            return POINTS[args[0]]
        raise ValueError(f"unknown point: {' '.join(args)}")

    for step in steps:
        verb, _, rest = step.partition(" ")
        args = rest.split()
        if verb == "tap":
            actions.append(("touch", *point(args), 80, step))
        elif verb == "hold":
            actions.append(("touch", *point(args[:-1]), int(args[-1]), step))
        elif verb == "type":
            for ch in rest.strip().upper():
                if ch not in LETTERS:
                    raise ValueError(f"no key for {ch!r}")
                actions.append(("touch", *LETTERS[ch], 80, f"key {ch}"))
        elif verb == "num":
            for ch in rest.strip():
                if ch not in DIGITS:
                    raise ValueError(f"no key for {ch!r}")
                actions.append(("touch", *DIGITS[ch], 80, f"key {ch}"))
        elif verb == "wait":
            actions.append(("wait", float(rest)))
        elif verb == "ask":
            actions.append(("ask", rest.strip()))
        elif verb == "unlock":
            actions.append(("touch", *POINTS["lock_wake"], 80, "unlock: wake"))
            actions.append(("wait", 0.5))
            actions.append(("touch", *POINTS["lock_hold"], UNLOCK_HOLD_MS, "unlock: hold padlock"))
        elif verb == "shot":
            actions.append(("shot",))
        else:
            raise ValueError(f"unknown step: {step}")
    if sum(a[0] == "shot" for a in actions) != 1:
        raise ValueError("a recipe needs exactly one 'shot'")
    return actions


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

    def _debug_request(self, method, path):
        """A /debug request; turns debug mode back on if the screen restarted (409)."""
        try:
            return self._request(method, path)
        except urllib.error.HTTPError as e:
            if e.code != 409:
                raise
            self.enable_debug_mode()
            return self._request(method, path)

    def enable_debug_mode(self):
        self._request("POST", "/debug/mode?on=1")

    def touch(self, x, y, ms=80):
        """Simulated touch (firmware variant hmi_screenshots). Returns after release."""
        self._debug_request("POST", f"/debug/touch?x={x}&y={y}&ms={ms}")

    def set_ui(self, lang=None, autolock=None):
        query = []
        if lang:
            query.append(f"lang={lang}")
        if autolock is not None:
            query.append(f"autolock={int(autolock)}")
        self._debug_request("POST", "/debug/ui?" + "&".join(query))

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


def run_recipe(hmi, name, actions, lang, pause, debug=False):
    """Play a recipe on the screen. Returns True if the screenshot was taken and
    the recipe finished (the screen should be back on the main screen)."""
    debug_dir = DEBUG_DIR / lang / name
    if debug:
        debug_dir.mkdir(parents=True, exist_ok=True)
        for old in debug_dir.glob("*.png"):
            old.unlink()
    for n, a in enumerate(actions, 1):
        try:
            if a[0] == "touch":
                _, x, y, ms, label = a
                print(f"    {label}")
                hmi.touch(x, y, ms)
                time.sleep(pause)
                if debug:
                    png, _ = hmi.screenshot_png()
                    tag = re.sub(r"[^a-z0-9]+", "_", label.lower()).strip("_")
                    (debug_dir / f"{n:02d}_{tag}.png").write_bytes(png)
            elif a[0] == "wait":
                time.sleep(a[1])
            elif a[0] == "ask":
                input(f"    {a[1]} — then press Enter ")
            elif a[0] == "shot":
                take(hmi, name, lang)
        except urllib.error.HTTPError as exc:
            if exc.code == 404:
                sys.exit("the screen has no /debug/touch: flash the firmware variant hmi_screenshots")
            print(f"  ERROR at step {n} ({a}): HTTP {exc.code}")
            return False
        except OSError as exc:
            print(f"  ERROR at step {n} ({a}): {exc}")
            return False
    return True


def interactive(hmi, entries, lang, recipes=None, pause=0.6, debug=False):
    recipes = recipes or {}
    print(
        "\nFor each entry: set up the screen, then press\n"
        "  Enter   capture now\n"
        "  w N     wait N seconds, then capture (to touch the screen meanwhile)\n"
        "  s       skip      b  back to the previous entry      q  quit\n"
    )
    if recipes:
        print("Entries with a recipe run by themselves, starting from the main screen\n"
              "(unlocked, all functions off).\n")
    i = 0
    while i < len(entries):
        e = entries[i]
        done = " (exists)" if out_path(e["name"], lang).exists() else ""
        if e["name"] in recipes:
            print(f"[{i + 1}/{len(entries)}] {e['name']}{done}  — automatic")
            if run_recipe(hmi, e["name"], recipes[e["name"]], lang, pause, debug):
                i += 1
                continue
            cmd = input("  Recipe stopped. Bring the screen back to the main screen, then\n"
                        "  Enter retry   s skip   q quit > ").strip().lower()
            if cmd == "q":
                break
            if cmd == "s":
                i += 1
            continue
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
    ap.add_argument("--auto", action="store_true",
                    help="play the recipes in recipes.txt (firmware variant hmi_screenshots); "
                         "entries without a recipe are still asked for")
    ap.add_argument("--auto-only", action="store_true", help="with --auto: skip the entries without a recipe")
    ap.add_argument("--debug", action="store_true",
                    help="with --auto: also save a picture after every touch in build/recipe_debug/")
    ap.add_argument("--pause", type=float, default=0.6, help="seconds to wait after each touch (default 0.6)")
    ap.add_argument("--check", action="store_true", help="check recipes.txt without a screen, then exit")
    args = ap.parse_args()

    entries = load_captures()
    names = {e["name"] for e in entries}

    if args.check or args.auto:
        recipes = {}
        bad = False
        for name, steps in load_recipes().items():
            if name not in names:
                print(f"recipe for unknown capture: {name}")
                bad = True
                continue
            try:
                recipes[name] = compile_recipe(steps, args.lang)
            except ValueError as exc:
                print(f"{name}: {exc}")
                bad = True
        if bad:
            sys.exit(1)
        if args.check:
            for e in entries:
                n = e["name"]
                kind = f"auto ({sum(a[0] == 'touch' for a in recipes[n])} touches)" if n in recipes else "manual"
                print(f"{n:<28} {kind}")
            print(f"{len(recipes)} automatic, {len(entries) - len(recipes)} manual")
            return
    else:
        recipes = {}

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
        by_name = {e["name"]: e for e in entries}
        entries = [by_name[n] for n in wanted]  # keep the operator's order
    if args.missing:
        entries = [e for e in entries if not out_path(e["name"], args.lang).exists()]
    if args.auto_only:
        entries = [e for e in entries if e["name"] in recipes]

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
    if args.name:
        if args.name not in names:
            sys.exit(f"unknown capture name: {args.name}")
        if args.name in recipes:
            entries = [e for e in load_captures() if e["name"] == args.name]
        else:
            print(f"Touchscreen at {host}, debug mode on. Screen language must be '{args.lang}'.")
            take(hmi, args.name, args.lang)
            return

    if not args.auto:
        print(f"Touchscreen at {host}, debug mode on. Screen language must be '{args.lang}'.")
        interactive(hmi, entries, args.lang)
        return

    try:
        hmi.set_ui(lang=args.lang, autolock=0)
    except urllib.error.HTTPError as e:
        sys.exit("the screen has no /debug/ui: flash the firmware variant hmi_screenshots"
                 if e.code == 404 else f"could not set the screen language: HTTP {e.code}")
    print(f"Touchscreen at {host}: debug mode on, language '{args.lang}', auto-lock off.")
    print("Before starting: all functions off, no popup open and NO ACTIVE BABY\n"
          "(discharge any baby in Menu > REGISTERED BABIES). The chamber must be empty:\n"
          "some recipes turn the heater and the phototherapy lamp on for a few seconds.")
    if input("Ready? Enter to start, q to quit > ").strip().lower() == "q":
        hmi.set_ui(autolock=1)
        return
    hmi.touch(*POINTS["lock_wake"])        # unlock in case the screen is locked
    time.sleep(0.5)
    hmi.touch(*POINTS["lock_hold"], UNLOCK_HOLD_MS)
    time.sleep(args.pause)
    try:
        interactive(hmi, entries, args.lang, recipes, args.pause, args.debug)
    finally:
        try:
            hmi.set_ui(autolock=1)
            print("Auto-lock back on.")
        except OSError:
            print("Could not turn auto-lock back on (it comes back after a restart).")


if __name__ == "__main__":
    main()
