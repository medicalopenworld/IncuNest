#!/usr/bin/env python3
"""Comprueba que los dos bloques de telemetria (GPRS y WiFi) no hayan derivado.

Invariante: una clave solo puede estar en un transporte y no en el otro si esta
dentro de un guard `#if TX_GROUP_<grupo>_<TRANSPORTE>` declarado en
config/transport_policy.h. Todo lo que este fuera de un guard es CORE y tiene
que existir en los dos.

Esto es lo que fallaba antes: las dos copias se mantienen a mano y habian
divergido en 18 claves sin que nadie lo decidiera.

Uso:  python tools/check_transport_matrix.py
Sale con codigo 1 si encuentra deriva no declarada.
"""
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = {
    "GPRS": os.path.join(ROOT, "src", "tasks", "GPRS.cpp"),
    "WIFI": os.path.join(ROOT, "src", "tasks", "Wifi_OTA.cpp"),
}
POLICY = os.path.join(ROOT, "include", "config", "transport_policy.h")

KEY_RE = re.compile(r"addVariableToTelemetry(GPRS|WIFI)JSON\[\s*([A-Za-z0-9_]+)\s*\]")
IF_RE = re.compile(r"^\s*#\s*if\s+(TX_GROUP_[A-Z]+_(?:GPRS|WIFI))\b")
ANY_IF_RE = re.compile(r"^\s*#\s*if")
ENDIF_RE = re.compile(r"^\s*#\s*endif")


def scan(path, transport):
    """Devuelve {clave: grupo o None} para un transporte, siguiendo los #if."""
    out = {}
    stack = []  # grupo activo por nivel de anidamiento
    for line in io.open(path, encoding="utf-8", errors="replace"):
        m = IF_RE.match(line)
        if m:
            stack.append(m.group(1))
            continue
        if ANY_IF_RE.match(line):
            stack.append(None)
            continue
        if ENDIF_RE.match(line):
            if stack:
                stack.pop()
            continue
        for tr, key in KEY_RE.findall(line):
            if tr != transport:
                continue
            group = next((g for g in reversed(stack) if g), None)
            out[key] = group
    return out


def policy_flags():
    txt = io.open(POLICY, encoding="utf-8", errors="replace").read()
    return dict(re.findall(r"#define\s+(TX_GROUP_[A-Z]+_(?:GPRS|WIFI))\s+([01])", txt))


def main():
    flags = policy_flags()
    g = scan(SRC["GPRS"], "GPRS")
    w = scan(SRC["WIFI"], "WIFI")

    problems = []

    # 1. CORE (sin guard) tiene que ser identico en los dos transportes.
    core_g = {k for k, grp in g.items() if grp is None}
    core_w = {k for k, grp in w.items() if grp is None}
    for k in sorted(core_g - core_w):
        problems.append(
            "%s se publica por GPRS sin guard, pero no por WiFi.\n"
            "    -> o lo anades al bloque WiFi, o lo metes en un grupo "
            "declarado en transport_policy.h" % k
        )
    for k in sorted(core_w - core_g):
        problems.append(
            "%s se publica por WiFi sin guard, pero no por GPRS.\n"
            "    -> o lo anades al bloque GPRS, o lo metes en un grupo "
            "declarado en transport_policy.h" % k
        )

    # 2. Todo grupo usado en el codigo tiene que existir en la tabla.
    for src, mapping in (("GPRS.cpp", g), ("Wifi_OTA.cpp", w)):
        for grp in {v for v in mapping.values() if v}:
            if grp not in flags:
                problems.append(
                    "%s usa %s, que no esta definido en transport_policy.h" % (src, grp)
                )

    # 3. Presupuesto de campos del StaticJsonDocument.
    main_h = io.open(
        os.path.join(ROOT, "include", "main.h"), encoding="utf-8", errors="replace"
    ).read()
    cap = re.search(r"#define\s+THINGSBOARD_FIELDS_AMOUNT\s+(\d+)", main_h)
    budget = int(cap.group(1)) if cap else None

    print("CORE (identico en ambos): %d claves" % len(core_g & core_w))
    for grp in sorted(flags):
        n = len([k for k, v in g.items() if v == grp] + [k for k, v in w.items() if v == grp])
        print("  %-30s = %s   (%d claves en su bloque)" % (grp, flags[grp], n))
    if budget:
        print(
            "\nPresupuesto JSON: %d campos | maximo posible GPRS %d, WiFi %d"
            % (budget, len(g), len(w))
        )
        if max(len(g), len(w)) > budget:
            print(
                "  AVISO: el maximo posible supera la capacidad. ArduinoJson\n"
                "  descarta campos EN SILENCIO al llenarse (no hay overflowed()).\n"
                "  Solo desborda si una sola publicacion supera %d campos." % budget
            )

    if problems:
        print("\nDERIVA NO DECLARADA (%d):" % len(problems))
        for p in problems:
            print("  - " + p)
        return 1
    print("\nOK: no hay deriva sin declarar.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
