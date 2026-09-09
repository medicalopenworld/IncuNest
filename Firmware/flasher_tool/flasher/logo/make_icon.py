"""Genera el icono de la aplicacion a partir de IncuNest_logo.png.

Salidas (versionadas junto a este script; solo hay que regenerarlas si cambia
el logo de origen, no para compilar):

  IncuNest_icon.png   512x512, fondo transparente  -> iconphoto de Tk y
                                                      icono del .app de macOS
  IncuNest_icon.ico   16..256 px                   -> icono del .exe y
                                                      iconbitmap en Windows

Dos decisiones que no son obvias:

- Del logo se recorta **solo la marca de la incubadora**. El wordmark
  "incunest" queda ilegible a 16 px (el tamano al que Windows pinta la barra
  de titulo) y solo ensucia el resto de tamanos.
- Los tamanos pequenos (<= 32 px) llevan un refuerzo de contraste. Los trazos
  del logo son finos: al reducir de 512 a 16 px se promedian con el blanco del
  interior y el icono sale lavado, casi rosa palido (comprobado en la barra de
  titulo). A partir de 48 px se usa el color de marca tal cual.

Requiere Pillow >= 9.3 (append_images en el guardado de .ico).

Uso:  python logo/make_icon.py
"""

from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).parent
SOURCE = HERE / 'IncuNest_logo.png'

# Caja de la marca de la incubadora dentro del logo de 225x225, medida sobre el
# perfil de filas del PNG (el wordmark empieza en y=140, tras una banda vacia).
MARK_BOX = (46, 43, 177, 131)
PAD = 6            # margen alrededor de la marca, en pixeles del original
MASTER = 512       # lado del PNG maestro
ICO_SIZES = (16, 24, 32, 48, 64, 128, 256)

# Cuanto se aleja del blanco cada pixel, por tamano. Cuanto mas pequeno, mas
# promedia el reescalado y mas refuerzo hace falta. Valores mas altos que estos
# ennegrecen el marco azul en vez de solo recuperarlo. Los tamanos que no estan
# aqui (48 y arriba) se dejan con el color de marca exacto.
PUNCH_COLOR = {16: 2.5, 24: 1.9, 32: 1.6}
PUNCH_ALPHA = 1.15


def build_master() -> Image.Image:
    """Marca recortada, centrada en un cuadrado y con el fondo transparente."""
    logo = Image.open(SOURCE).convert('RGBA')
    mark = logo.crop(MARK_BOX)

    side = max(mark.size) + 2 * PAD
    square = Image.new('RGBA', (side, side), (255, 255, 255, 255))
    square.paste(mark, ((side - mark.width) // 2, (side - mark.height) // 2))

    master = square.resize((MASTER, MASTER), Image.LANCZOS)

    # El fondo blanco exterior se vacia con un relleno desde las cuatro
    # esquinas. El contorno azul de la incubadora esta cerrado, asi que el
    # blanco de dentro (sobre el que va el bebe) se mantiene.
    edge = MASTER - 1
    for corner in ((0, 0), (edge, 0), (0, edge), (edge, edge)):
        ImageDraw.floodfill(master, corner, (0, 0, 0, 0), thresh=40)
    return master


def punch(img: Image.Image, factor: float) -> Image.Image:
    """Aleja cada pixel del blanco y refuerza el alfa, para los tamanos pequenos."""
    color_lut = [max(0, min(255, round(255 - (255 - v) * factor)))
                 for v in range(256)]
    alpha_lut = [min(255, round(v * PUNCH_ALPHA)) for v in range(256)]
    r, g, b, a = img.split()
    return Image.merge('RGBA', (r.point(color_lut), g.point(color_lut),
                                b.point(color_lut), a.point(alpha_lut)))


def main() -> None:
    master = build_master()
    master.save(HERE / 'IncuNest_icon.png')

    frames = {}
    for size in ICO_SIZES:
        frame = master.resize((size, size), Image.LANCZOS)
        factor = PUNCH_COLOR.get(size)
        frames[size] = punch(frame, factor) if factor else frame

    # El primero es la base; el resto van en append_images y Pillow los empareja
    # por tamano exacto, asi que ningun frame se re-escala.
    base = frames.pop(max(ICO_SIZES))
    base.save(HERE / 'IncuNest_icon.ico',
              sizes=[(s, s) for s in ICO_SIZES],
              append_images=list(frames.values()))
    print(f"Escritos {HERE / 'IncuNest_icon.png'} y {HERE / 'IncuNest_icon.ico'}")


if __name__ == '__main__':
    main()
