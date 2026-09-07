#!/usr/bin/env python3
"""Genera las imagenes de pop-up de alarma con nano banana 2 (Gemini image API).

Los prompts NO viven aqui: se leen de docs/alarm_popup_image_prompts.md, que es
la unica fuente. Duplicarlos en un JSON paralelo seria un segundo sitio que
tocar cada vez que se afina una escena, y la copia que no se toca es la que
acaba generandose.

El fichero markdown tiene una estructura estable de la que depende el parser:
  - el PRIMER bloque cercado (```) es el bloque de estilo, comun a todas
  - cada bloque cercado posterior es una escena, y el nombre de fichero sale de
    la linea `**N - `ALARM_X`**` mas cercana por encima

Uso:
  python tools/gen_alarm_images.py --list          # las 20 escenas
  python tools/gen_alarm_images.py --only alm_17   # genera una iteracion
  python tools/gen_alarm_images.py --all           # las 20 (~1.4 $)
  python tools/gen_alarm_images.py --pick alm_17=1 # elige la v01
  python tools/gen_alarm_images.py --publish       # a Display_HMI/assets_src/, optimizadas

Sin --all ni --only no genera nada: pedir 20 imagenes de golpe cuesta dinero y
lo normal es iterar de una en una hasta clavar el estilo.

La clave sale de GEMINI_API_KEY o de tools/.gemini_key (gitignorado).
Las fotos de referencia del equipo van en docs/alarm_images_refs/ (ver su README).
"""

from __future__ import annotations

import argparse
import base64
import io
import json
import os
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PROMPTS_MD = REPO / "docs" / "alarm_popup_image_prompts.md"
# Destino de las imagenes elegidas. NO es Display_HMI/data/ (esa carpeta es el
# contenido de la imagen SPIFFS y solo lleva heartbeat.mp3): las imagenes de
# alarma se convierten a arrays C en Display_HMI/src/ui/assets/ y se compilan
# dentro de la app. Ver Display_HMI/assets_src/README.md.
ASSETS_DIR = REPO / "Display_HMI" / "assets_src"

# Taller. Una carpeta por alarma, y dentro el historial completo de intentos:
#
#   docs/alarm_images_wip/alm_17_heater_sensor_fault/
#       alm_17_heater_sensor_fault_v03.png  <- la elegida, version en el nombre
#       iteraciones/v01.png                 <- todos los intentos, en orden
#       iteraciones/v02.png
#       iteraciones/v03.png
#
# Nada de esto entra en assets_src/: el historial de intentos no tiene por que
# vivir en el arbol del firmware. Las imagenes buenas se copian alli, ya
# reescaladas, con --publish.
WIP_DIR = REPO / "docs" / "alarm_images_wip"

# Fichero de clave, gitignorado. Existe porque cada invocacion del script arranca
# un shell nuevo: un `$env:GEMINI_API_KEY = "..."` escrito a mano no sobrevive a
# la siguiente llamada, y `setx` solo lo veria un proceso arrancado despues. El
# fichero es el unico sitio donde la clave persiste sin tener que pegarla en una
# conversacion ni dejarla en el historial del shell.
KEY_FILE = REPO / "tools" / ".gemini_key"

# Referencias de ESTILO Y SILUETA. Estas dos imagenes base (2026-09-05) fijan a
# la vez el trazo, la paleta, la silueta de la incubadora y la cara del bebe. Son
# la referencia canonica: si una imagen generada no se parece a estas, esta mal.
#
# Son las variantes _FLAT: el panel crema inferior se ha repintado liso, sin el
# contorno del cajon ni su tirador. Esos trazos pasaban por DETRAS de la
# insignia y la volvian ilegible.
#
# Sustituyeron a Display_HMI/assets_src/Baby_phototherapy_eyes_covering.png y
# Baby_place_sensor.png, que se usaban antes. Dos motivos:
#  - dibujaban una incubadora generica distinta de la IncuNest, y la silueta se
#    desestabilizaba en cuanto una escena anadia elementos;
#  - la de fototerapia contagiaba las GAFAS del bebe, que llegaron a aparecer en
#    la alarma de corte de red.
#
# La del bebe va ESPEJADA (_MIRRORED). El original tiene la cabeza a la
# izquierda y la serie la quiere a la derecha; pedirlo por texto fallaba en 6 de
# cada 15 imagenes, porque la referencia adjunta pesa mas que la descripcion. Es
# la misma leccion que con la silueta: cuando texto y referencia discrepan, gana
# la referencia, asi que hay que arreglar la referencia y no el texto.
STYLE_REFS = [
    REPO / "docs" / "alarm_images_refs" / "_comunes" / "IncuNest_alarm_base_FLAT.jpg",
    REPO / "docs" / "alarm_images_refs" / "_comunes" / "IncuNest_alarm_base_with_baby_MIRRORED_FLAT.jpg",
]

# Referencias de FIDELIDAD: fotos de la incubadora real y de sus componentes.
# Responden a una pregunta distinta que las de estilo — aquellas dicen "dibuja
# asi", estas dicen "esto es lo que hay que dibujar". Sin ellas el modelo se
# inventa una incubadora generica, que es lo que ha estado haciendo.
#
#   docs/alarm_images_refs/_comunes/    -> se adjuntan SIEMPRE (el equipo entero)
#   docs/alarm_images_refs/<lo-que-sea> -> solo si una escena lo nombra con REFS:
REFS_DIR = REPO / "docs" / "alarm_images_refs"

# Tope de imagenes por peticion. Mas referencias no es mejor: compiten entre si
# y el modelo empieza a promediarlas. Style (2) + comunes + especificas.
MAX_REFS = 6
# Lado mayor al que se reescala cada referencia antes de mandarla. Una foto de
# movil son varios MB que en base64 crecen un tercio mas; a 1024 px se transmite
# igual de bien la forma y el color, que es para lo que sirven.
REF_MAX_PX = 1024

# "Nano banana 2" es este id (comprobado en ai.google.dev/gemini-api/docs/pricing,
# 2026-09-03). Existe ademas gemini-3-pro-image, que es OTRO modelo y no el que
# se pidio. Se puede sobreescribir con GEMINI_IMAGE_MODEL sin tocar el script.
#
# NO tiene nivel gratuito: los modelos de imagen estan fuera del free tier de AI
# Studio, y la suscripcion Google AI Pro tampoco cubre la API (ver
# docs/alarm_popup_image_prompts.md §5.3). A ~$0.067 por imagen de 1K, las 20
# salen por ~1.4 $, asi que el coste no es el problema — pero hay que tener
# facturacion activada en el proyecto o la primera llamada devuelve 429.
DEFAULT_MODEL = "gemini-3.1-flash-image"
API_BASE = "https://generativelanguage.googleapis.com/v1beta/models"

FENCE = re.compile(r"^```")
# `**7 - `ALARM_MAINS_INTERRUPTION`** ...` o `**`alm_display_link_lost.png`** ...`
HEADER = re.compile(r"^\*\*(?:(\d+)\s*[-—]\s*)?`([^`]+)`\*\*")


def parse_prompts(md_path: Path) -> tuple[str, list[dict]]:
    """Devuelve (bloque_de_estilo, [{'name', 'scene'}, ...])."""
    lines = md_path.read_text(encoding="utf-8").splitlines()

    blocks: list[tuple[int, str]] = []  # (linea_de_apertura, contenido)
    inside = False
    start = 0
    buf: list[str] = []
    for i, line in enumerate(lines):
        if FENCE.match(line):
            if inside:
                blocks.append((start, "\n".join(buf)))
                buf = []
                inside = False
            else:
                inside = True
                start = i
            continue
        if inside:
            buf.append(line)

    if not blocks:
        sys.exit(f"error: no hay bloques cercados en {md_path}")

    style = blocks[0][1].strip()

    scenes: list[dict] = []
    for open_line, body in blocks[1:]:
        name = None
        for j in range(open_line - 1, -1, -1):
            m = HEADER.match(lines[j])
            if m:
                num, token = m.group(1), m.group(2)
                if token.endswith(".png"):
                    name = token[: -len(".png")]
                elif token.startswith("ALARM_") and num:
                    name = f"alm_{int(num):02d}_{token[len('ALARM_'):].lower()}"
                break
        if not name:
            sys.exit(
                f"error: bloque en la linea {open_line + 1} de {md_path.name} sin "
                "cabecera reconocible por encima"
            )
        # Una escena puede empezar con `REFS: fan.jpg, bahia_*.jpg` para pedir
        # fotos concretas del equipo. La linea se saca del prompt: es
        # metainformacion para el script, no texto que deba leer el modelo.
        text = body.strip()
        named: list[str] = []
        if text.upper().startswith("REFS:"):
            head, _, rest = text.partition("\n")
            named = [t.strip() for t in head[len("REFS:"):].split(",") if t.strip()]
            text = rest.strip()

        scenes.append({"name": name, "scene": text, "refs": named})

    return style, scenes


def resolve_api_key() -> str | None:
    """Entorno primero, fichero despues. La clave NUNCA se imprime ni se mete en
    la linea de comandos: un argumento --api-key acabaria en el historial del
    shell y en la lista de procesos."""
    key = os.environ.get("GEMINI_API_KEY", "").strip()
    if key:
        return key
    if KEY_FILE.is_file():
        return KEY_FILE.read_text(encoding="utf-8").strip() or None
    return None


def build_prompt(style: str, scene: str) -> str:
    return f"{style}\n\n{scene}\n"


def inline_image(path: Path) -> dict:
    """Adjunta una imagen, reescalada a REF_MAX_PX si hace falta.

    Se manda siempre como JPEG salvo que ya fuera pequena y PNG: una foto de
    incubadora en PNG son 8-10 MB, y en base64 un tercio mas."""
    raw = path.read_bytes()
    mime = "image/png"
    try:
        from PIL import Image

        img = Image.open(io.BytesIO(raw))
        if max(img.size) > REF_MAX_PX:
            img = img.convert("RGB")
            img.thumbnail((REF_MAX_PX, REF_MAX_PX), Image.LANCZOS)
            buf = io.BytesIO()
            img.save(buf, format="JPEG", quality=88, optimize=True)
            raw, mime = buf.getvalue(), "image/jpeg"
        elif path.suffix.lower() in (".jpg", ".jpeg"):
            mime = "image/jpeg"
    except ImportError:
        if path.suffix.lower() in (".jpg", ".jpeg"):
            mime = "image/jpeg"

    return {
        "inline_data": {
            "mime_type": mime,
            "data": base64.b64encode(raw).decode("ascii"),
        }
    }


# Extensiones que se consideran imagen. El filtro tiene que aplicarse TAMBIEN a
# los globs de las escenas: `sonda_piel/*` casa con el QUE_FOTOGRAFIAR.md de la
# carpeta, y mandarselo a PIL aborta la generacion entera.
IMG_SUFFIXES = (".png", ".jpg", ".jpeg", ".webp")


def is_image(p: Path) -> bool:
    return p.is_file() and p.suffix.lower() in IMG_SUFFIXES


def collect_refs(named: list[str]) -> list[Path]:
    """Estilo + comunes + las que la escena pida por nombre, hasta MAX_REFS."""
    refs = list(STYLE_REFS)

    # _comunes/ NO se adjunta automaticamente. Son fotos del equipo real, y la
    # decision de producto (2026-09-04) es que la incubadora dibujada siga
    # siendo la generica de los assets Baby_*, para no dejar desparejados los
    # cuatro que ya estan en assets_src/. Adjuntarlas a todo tiraria en direccion
    # contraria al texto del bloque de estilo, y el resultado seria un hibrido
    # peor que cualquiera de los dos. Las fotos sirven para los COMPONENTES de
    # detalle, que cada escena pide por su nombre.
    for want in named:
        hits = sorted(REFS_DIR.glob(want)) if any(c in want for c in "*?") else [REFS_DIR / want]
        found = [h for h in hits if is_image(h)]
        if not found:
            print(f"    aviso: sin fotos en {want}, se genera sin esa referencia")
        refs += found

    if len(refs) > MAX_REFS:
        # Las de estilo van primero y son las que NO se pueden perder; el
        # recorte se come las ultimas especificas, asi que se avisa de cuales.
        dropped = [p.name for p in refs[MAX_REFS:]]
        print(f"    aviso: {len(refs)} referencias, se descartan: {', '.join(dropped)}")
        refs = refs[:MAX_REFS]
    return refs


def generate(prompt: str, refs: list[Path], model: str, api_key: str) -> bytes:
    parts: list[dict] = [{"text": prompt}]
    parts.extend(inline_image(p) for p in refs)

    payload = {
        "contents": [{"parts": parts}],
        "generationConfig": {"responseModalities": ["IMAGE"]},
    }
    req = urllib.request.Request(
        f"{API_BASE}/{model}:generateContent",
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Content-Type": "application/json",
            "x-goog-api-key": api_key,
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=300) as resp:
            body = json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        detail = e.read().decode("utf-8", "replace")[:800]
        raise SystemExit(f"error HTTP {e.code} de la API:\n{detail}") from None

    for cand in body.get("candidates", []):
        for part in cand.get("content", {}).get("parts", []):
            data = part.get("inlineData") or part.get("inline_data")
            if data and data.get("data"):
                return base64.b64decode(data["data"])

    raise SystemExit(
        "la respuesta no traia ninguna imagen. Respuesta cruda:\n"
        + json.dumps(body, indent=2)[:1200]
    )


def current_file(folder: Path, name: str) -> Path | None:
    """El elegido de esa alarma: `<nombre>_vNN.png` en la raiz de su carpeta.

    Lleva la version en el NOMBRE a proposito. Un `actual.png` no dice cual de
    los intentos es, asi que al mirar la carpeta no se sabe si lo que hay es el
    que se eligio o simplemente el ultimo que se genero."""
    found = sorted(folder.glob(f"{name}_v*.png"))
    return found[-1] if found else None


def set_current(folder: Path, name: str, n: int) -> Path:
    """Deja `<nombre>_vNN.png` como unico elegido, copiado de su iteracion."""
    for old in folder.glob(f"{name}_v*.png"):
        old.unlink()
    dest = folder / f"{name}_v{n:02d}.png"
    dest.write_bytes((folder / "iteraciones" / f"v{n:02d}.png").read_bytes())
    return dest


def save_iteration(root: Path, name: str, png: bytes) -> tuple[Path, Path, int]:
    """Guarda un intento nuevo y lo deja como elegido.

    Devuelve (iteracion, elegido, numero). Nunca sobreescribe una iteracion
    anterior: el historial es el motivo de que exista esta carpeta.
    """
    folder = root / name
    iters = folder / "iteraciones"
    iters.mkdir(parents=True, exist_ok=True)

    used = [
        int(m.group(1))
        for p in iters.glob("v*.png")
        if (m := re.fullmatch(r"v(\d+)", p.stem))
    ]
    n = max(used, default=0) + 1

    shot = iters / f"v{n:02d}.png"
    shot.write_bytes(png)
    return shot, set_current(folder, name, n), n


def pick(root: Path, spec: str) -> None:
    """--pick alm_17=1 : marca la v01 de esa alarma como la elegida."""
    if "=" not in spec:
        sys.exit("error: usa --pick PREFIJO=N, por ejemplo --pick alm_17=1")
    prefix, num = spec.split("=", 1)
    folders = [p for p in sorted(root.glob(f"{prefix.strip()}*")) if p.is_dir()]
    if len(folders) != 1:
        sys.exit(
            f"error: {prefix!r} casa con {len(folders)} carpetas; se necesita exactamente una"
        )
    folder = folders[0]
    try:
        n = int(num)
    except ValueError:
        sys.exit(f"error: {num!r} no es un numero de version")
    if not (folder / "iteraciones" / f"v{n:02d}.png").is_file():
        have = sorted(p.stem for p in (folder / "iteraciones").glob("v*.png"))
        sys.exit(f"error: {folder.name} no tiene v{n:02d}. Tiene: {', '.join(have) or 'ninguna'}")
    dest = set_current(folder, folder.name, n)
    print(f"elegida {dest.relative_to(REPO)}")


def publish(root: Path, dest: Path, size: int) -> None:
    """Copia el `actual.png` de cada alarma a dest/<nombre>.png y lo optimiza.

    assets_src/ queda PLANO: es una carpeta de fuentes para el conversor de
    LVGL, no un arbol que nadie recorra, y el historial de iteraciones no
    pinta ahi."""
    folders = sorted(p for p in root.iterdir() if p.is_dir()) if root.is_dir() else []
    if not folders:
        sys.exit(f"error: no hay nada que publicar en {root}")

    dest.mkdir(parents=True, exist_ok=True)
    published = 0
    for folder in folders:
        current = current_file(folder, folder.name)
        if current is None:
            print(f"  {folder.name}: sin elegida, se salta")
            continue
        # El nombre publicado va SIN version: da nombre al simbolo del array C
        # (ui_img_<nombre>_png) y un sufijo _vNN se arrastraria al firmware en
        # cada reiteracion de la imagen.
        out = dest / f"{folder.name}.png"
        out.write_bytes(current.read_bytes())
        published += 1
        print(f"  {current.name} -> {out.relative_to(REPO)}")

    print(f"\n{published} publicadas. Optimizando...")
    postprocess(dest, size)


def postprocess(directory: Path, size: int) -> None:
    """Reescala y optimiza in situ. Ver docs/alarm_popup_image_prompts.md §5.1.

    El presupuesto NO es SPIFFS: estas imagenes se compilan dentro de la app y
    cada una ocupa en app0 su tamano descomprimido en el formato LVGL, asi que
    lo que manda es el numero de pixeles. Reescalar es lo que de verdad ahorra
    flash; el peso del PNG solo importa para el repositorio."""
    try:
        from PIL import Image
    except ImportError:
        sys.exit("error: hace falta Pillow (pip install pillow)")

    total_before = total_after = 0
    for png in sorted(directory.glob("alm_*.png")):
        before = png.stat().st_size
        img = Image.open(png).convert("RGB")
        img.thumbnail((size, size), Image.LANCZOS)
        # PNG-8 con paleta adaptativa: estas ilustraciones son planas y con pocos
        # colores, asi que 256 entradas no se notan y el fichero cae ~10x.
        img.convert("P", palette=Image.ADAPTIVE, colors=256).save(
            png, optimize=True
        )
        after = png.stat().st_size
        total_before += before
        total_after += after
        print(f"  {png.name}: {before // 1024} KB -> {after // 1024} KB")
    if total_before:
        print(
            f"total: {total_before // 1024} KB -> {total_after // 1024} KB "
            f"({100 * total_after // total_before} %)"
        )


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--list", action="store_true", help="lista las escenas y sale")
    ap.add_argument("--only", metavar="PREFIJO", help="genera solo las que empiecen asi")
    ap.add_argument("--all", action="store_true", help="genera las 20")
    ap.add_argument("--out", type=Path, default=WIP_DIR, help="taller (default: docs/alarm_images_wip)")
    ap.add_argument("--publish", action="store_true",
                    help="copia las elegidas a Display_HMI/assets_src/ y las optimiza")
    ap.add_argument("--pick", metavar="PREFIJO=N",
                    help="marca esa version como elegida, p.ej. --pick alm_17=1")
    ap.add_argument("--model", default=os.environ.get("GEMINI_IMAGE_MODEL", DEFAULT_MODEL))
    ap.add_argument("--dry-run", action="store_true", help="imprime el prompt, no llama")
    ap.add_argument("--force", action="store_true", help="sobreescribe existentes")
    ap.add_argument("--postprocess", type=Path, metavar="DIR", help="reescala y optimiza")
    ap.add_argument("--size", type=int, default=360, help="lado mayor tras --postprocess")
    args = ap.parse_args()

    if args.postprocess:
        postprocess(args.postprocess, args.size)
        return

    if args.pick:
        pick(args.out, args.pick)
        return

    if args.publish:
        publish(args.out, ASSETS_DIR, args.size)
        return

    style, scenes = parse_prompts(PROMPTS_MD)

    if args.list:
        for s in scenes:
            print(f"{s['name']}.png")
        print(f"\n{len(scenes)} escenas en {PROMPTS_MD.relative_to(REPO)}")
        return

    if args.only:
        scenes = [s for s in scenes if s["name"].startswith(args.only)]
        if not scenes:
            sys.exit(f"error: ninguna escena empieza por {args.only!r}")
    elif not args.all:
        sys.exit("error: usa --all, --only PREFIJO, o --list")

    api_key = resolve_api_key()
    if not api_key and not args.dry_run:
        sys.exit(
            "error: no hay clave. Escribela en "
            f"{KEY_FILE.relative_to(REPO)} (una linea, sin comillas) "
            "o exporta GEMINI_API_KEY."
        )

    for ref in STYLE_REFS:
        if not ref.is_file():
            sys.exit(f"error: falta la referencia de estilo {ref}")

    args.out.mkdir(parents=True, exist_ok=True)

    for i, s in enumerate(scenes, 1):
        name = s["name"]
        if (not args.force and not args.dry_run
                and current_file(args.out / name, name) is not None):
            print(f"[{i}/{len(scenes)}] {name}: ya tiene elegida, se salta (--force)")
            continue

        prompt = build_prompt(style, s["scene"])
        if args.dry_run:
            print(f"\n===== {name} =====\n{prompt}")
            continue

        print(f"[{i}/{len(scenes)}] {name}: generando...", flush=True)
        refs = collect_refs(s["refs"])
        print(f"    referencias: {', '.join(r.name for r in refs)}")
        png = generate(prompt, refs, args.model, api_key)
        shot, chosen, n = save_iteration(args.out, name, png)
        print(f"    v{n:02d} ({len(png) // 1024} KB) -> {shot.relative_to(REPO)}")
        print(f"    elegida: {chosen.name}")

        if i < len(scenes):
            time.sleep(2)  # margen sobre el rate limit


if __name__ == "__main__":
    main()
