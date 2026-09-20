"""Comprueba que los binarios del flasher no llevan credenciales de flota dentro.

Sustituye al filtro por NOMBRE de entorno que habia antes (los *_factory de
platformio.ini, retirados el 2026-09-20). Ese filtro protegia de lo unico que
sabia mirar: como se llamaba la carpeta de build. Desde que la activacion de
SIM va encendida por defecto, cualquier binario compilado en una maquina con
el Credentials.h real lleva ONOMONDO_API_KEY dentro, se llame como se llame el
entorno, asi que lo que hay que mirar es el CONTENIDO.

Que busca: los valores de las macros de Credentials.h que no son credenciales
de UNA unidad sino de toda la organizacion. Hoy ONOMONDO_API_KEY, que abre
todas las SIM.

De donde saca los valores: del propio Credentials.h del repo, que no esta
versionado. En una maquina sin el (un clone limpio, el runner de CI) no hay
nada que comparar y el script lo dice con codigo 2 -- "no se ha podido
comprobar", que no es lo mismo que "esta limpio".

NUNCA imprime el valor de una clave, ni entera ni en trozos: solo dice en que
fichero aparece.

Codigos de salida:
    0  limpio
    1  se ha encontrado una credencial dentro de un .bin
    2  no se ha podido comprobar (sin Credentials.h, sin binarios)
"""
import re
import sys
from pathlib import Path

# Macros de Credentials.h cuyo valor NO puede viajar en un binario que salga de
# aqui. Son credenciales de organizacion: una sola filtracion las compromete
# todas. Las credenciales por unidad (el token que ThingsBoard entrega a cada
# placa al provisionarse) no estan aqui: esas no se compilan.
FLEET_SECRET_MACROS = ('ONOMONDO_API_KEY',)

# Longitud minima para buscar un valor dentro del binario. Un valor corto o
# dummy ("changeme", "") daria coincidencias por todas partes y convertiria la
# comprobacion en ruido.
_MIN_SECRET_LEN = 12


def find_credentials_header(start: Path) -> Path | None:
    for parent in [start] + list(start.parents):
        candidate = parent / 'Firmware' / 'motherBoard' / 'include' / 'Credentials.h'
        if candidate.is_file():
            return candidate
    return None


def read_fleet_secrets(header: Path) -> dict[str, bytes]:
    text = header.read_text(encoding='utf-8', errors='ignore')
    secrets: dict[str, bytes] = {}
    for macro in FLEET_SECRET_MACROS:
        m = re.search(r'#\s*define\s+' + macro + r'\s+"([^"]*)"', text)
        if m and len(m.group(1)) >= _MIN_SECRET_LEN:
            secrets[macro] = m.group(1).encode()
    return secrets


def scan(firmware_base: Path, secrets: dict[str, bytes]) -> list[tuple[Path, str]]:
    hits: list[tuple[Path, str]] = []
    for binary in sorted(firmware_base.rglob('*.bin')):
        data = binary.read_bytes()
        for macro, value in secrets.items():
            if value in data:
                hits.append((binary, macro))
    return hits


def main() -> int:
    here = Path(__file__).resolve().parent
    firmware_base = here.parent / 'data' / 'firmware'

    header = find_credentials_header(here)
    if header is None:
        print("check_no_secrets: no hay Credentials.h en este arbol — "
              "NO se ha podido comprobar si los binarios llevan claves dentro.")
        return 2

    secrets = read_fleet_secrets(header)
    if not secrets:
        print(f"check_no_secrets: {header.name} no define ningun valor real de "
              f"{', '.join(FLEET_SECRET_MACROS)} — nada que comprobar.")
        return 2

    binaries = list(firmware_base.rglob('*.bin'))
    if not binaries:
        print(f"check_no_secrets: no hay ningun .bin en {firmware_base}.")
        return 2

    hits = scan(firmware_base, secrets)
    if not hits:
        print(f"check_no_secrets: {len(binaries)} binarios revisados, ninguna "
              f"credencial de flota dentro.")
        return 0

    print("check_no_secrets: CREDENCIAL DE FLOTA DENTRO DE UN BINARIO")
    for binary, macro in hits:
        print(f"  {binary.relative_to(firmware_base)}  contiene  {macro}")
    print()
    print("Ese valor abre TODAS las SIM de la organizacion y se lee con un")
    print("`strings` del .bin. No empaquetes ni publiques esta carpeta.")
    print()
    print("Para obtener un binario publicable, compila con la activacion de SIM")
    print("apagada y vuelve a copiar los binarios (PowerShell):")
    print('  $env:PLATFORMIO_BUILD_FLAGS="-DFTEST_SIM_ACT_ENABLED=0"')
    print("  pio run -e IncuNest_V18")
    print("(o deja que lo compile CI, que no tiene Credentials.h)")
    return 1


if __name__ == '__main__':
    sys.exit(main())
