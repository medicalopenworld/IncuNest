"""Genera los iconos del heading de la pantalla de bloqueo (puerta abierta,
nivel de ruido y "en bateria") como PNG fuente y como array C de LVGL.

Dibujo vectorial a 8x y reducido con antialias: trazo blanco de 2 px con
extremos redondeados, el mismo lenguaje que los iconos de linea de la UI
(wind-vector, candado). Blancos sobre transparente: el color lo pone el
codigo con img_recolor (blanco informativo, amarillo de atencion).

Salida:
  assets_src/heading_<nombre>.png      fuente, a tamano real
  src/ui/assets/ui_img_<nombre>.c       LV_IMG_CF_TRUE_COLOR_ALPHA (RGB565 + A)

El blanco es 0xFFFF con o sin LV_COLOR_16_SWAP, asi que el array no depende
de esa opcion. Uso (desde Display_HMI/):  python tools/gen_heading_icons.py
"""

import math
import os

from PIL import Image, ImageDraw

SS = 8  # supersampling
STROKE = 2.0
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.img = Image.new("L", (w * SS, h * SS), 0)
        self.d = ImageDraw.Draw(self.img)

    def _p(self, x, y):
        return (x * SS, y * SS)

    def dot(self, x, y, r, fill=255):
        self.d.ellipse([(x - r) * SS, (y - r) * SS, (x + r) * SS, (y + r) * SS], fill=fill)

    def line(self, pts, width=STROKE, fill=255):
        sp = [self._p(x, y) for x, y in pts]
        self.d.line(sp, fill=fill, width=int(width * SS), joint="curve")
        for x, y in (pts[0], pts[-1]):
            self.dot(x, y, width / 2, fill)

    def poly(self, pts, fill=255):
        self.d.polygon([self._p(x, y) for x, y in pts], fill=fill)

    def rrect(self, x0, y0, x1, y1, r, width=None, fill=255):
        box = [x0 * SS, y0 * SS, x1 * SS, y1 * SS]
        if width is None:
            self.d.rounded_rectangle(box, radius=r * SS, fill=fill)
        else:
            self.d.rounded_rectangle(box, radius=r * SS, outline=fill, width=int(width * SS))

    def arc(self, cx, cy, r, a0, a1, width=STROKE):
        box = [(cx - r) * SS, (cy - r) * SS, (cx + r) * SS, (cy + r) * SS]
        self.d.arc(box, a0, a1, fill=255, width=int(width * SS))
        for a in (a0, a1):
            rm = r - width / 2
            self.dot(cx + rm * math.cos(math.radians(a)), cy + rm * math.sin(math.radians(a)), width / 2)

    def alpha(self):
        return self.img.resize((self.w, self.h), Image.LANCZOS)


def door():
    # Marco con suelo y la hoja abierta hacia quien mira: bisagra en el lado
    # izquierdo del marco y el canto libre fuera de el, mas alto por la
    # perspectiva. El hueco del marco queda vacio: se lee "abierta".
    c = Canvas(22, 22)
    c.line([(10, 21), (10, 2.5), (19.5, 2.5), (19.5, 21)])
    c.line([(8, 21), (21, 21)])
    c.poly([(10, 2.5), (3, 0.5), (3, 21.5), (10, 21)])
    c.dot(5.6, 11.5, 1.1, fill=0)
    return c.alpha()


def speaker(waves):
    # Altavoz macizo y 1-3 ondas: bajo / medio / alto.
    c = Canvas(26, 22)
    c.rrect(1.5, 7.5, 6.5, 14.5, 1)
    c.poly([(6, 7.5), (11.5, 3), (11.5, 19), (6, 14.5)])
    for i in range(waves):
        c.arc(11.5, 11, 4.5 + 4 * i, -48, 48)
    return c.alpha()


def battery():
    # Pila apaisada a medio gastar: el equipo tira de bateria.
    c = Canvas(32, 18)
    c.rrect(1, 1, 27, 17, 3, width=STROKE)
    c.rrect(27.5, 5.5, 31, 12.5, 1.5)
    c.rrect(4.5, 4.5, 9.5, 13.5, 1)
    c.rrect(11.5, 4.5, 16.5, 13.5, 1)
    return c.alpha()


ICONS = {
    "heading_door": door,
    "heading_noise_low": lambda: speaker(1),
    "heading_noise_mid": lambda: speaker(2),
    "heading_noise_high": lambda: speaker(3),
    "heading_battery": battery,
}


def write_c(name, a):
    w, h = a.size
    sym = "ui_img_" + name
    data = []
    for y in range(h):
        for x in range(w):
            data += [0xFF, 0xFF, a.getpixel((x, y))]
    rows = []
    for i in range(0, len(data), 24):
        rows.append("    " + ",".join("0x%02X" % b for b in data[i:i + 24]) + ",")
    lines = [
        "// Generado por tools/gen_heading_icons.py (fuente: assets_src/%s.png)." % name,
        "// Blanco + alfa: el color lo pone img_recolor.",
        "",
        '#include "ui.h"',
        "",
        "#ifndef LV_ATTRIBUTE_MEM_ALIGN",
        "    #define LV_ATTRIBUTE_MEM_ALIGN",
        "#endif",
        "",
        "const LV_ATTRIBUTE_MEM_ALIGN uint8_t %s_data[] = {" % sym,
    ] + rows + [
        "};",
        "const lv_img_dsc_t %s = {" % sym,
        "    .header.always_zero = 0,",
        "    .header.w = %d," % w,
        "    .header.h = %d," % h,
        "    .data_size = sizeof(%s_data)," % sym,
        "    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,",
        "    .data = %s_data" % sym,
        "};",
        "",
    ]
    path = os.path.join(ROOT, "src", "ui", "assets", sym + ".c")
    with open(path, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines))


def main():
    for name, fn in ICONS.items():
        a = fn()
        png = Image.new("RGBA", a.size, (255, 255, 255, 0))
        png.putalpha(a)
        png.save(os.path.join(ROOT, "assets_src", name + ".png"))
        write_c(name, a)
        print(name, a.size)


if __name__ == "__main__":
    main()
