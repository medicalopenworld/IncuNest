#!/usr/bin/env python3
"""Draw the cover and back cover of the user manual from metadata.yaml.

Same visual style as the previous manual (v6): MOW blue band, logos of Medical
Open World, Salesianos Pamplona and IncuNest. The pages are drawn as A4
images (200 dpi) so they look the same in Word and in the PDF; export.py places
them as the first and last page. Every text on them comes from metadata.yaml.
"""

import datetime
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

TOOLS_DIR = Path(__file__).resolve().parent
ASSETS = TOOLS_DIR / "assets"
FONTS = Path("C:/Windows/Fonts")

DPI = 200
W, H = round(21.0 / 2.54 * DPI), round(29.7 / 2.54 * DPI)   # A4
BLUE = (9, 80, 149)        # same blue as the v6 cover
RED = (198, 40, 40)
GREY = (90, 90, 90)
WHITE = (255, 255, 255)


def font(weight, size_pt):
    files = {"bold": "segoeuib.ttf", "semibold": "seguisb.ttf", "regular": "segoeui.ttf"}
    return ImageFont.truetype(str(FONTS / files[weight]), round(size_pt * DPI / 72))


def mm(v):
    return round(v / 25.4 * DPI)


def paste_logo(page, name, box_w, box_h, x, y, centre=False):
    logo = Image.open(ASSETS / name).convert("RGB")
    logo.thumbnail((box_w, box_h), Image.LANCZOS)
    if centre:
        x = x - logo.width // 2
    page.paste(logo, (x, y))
    return logo.size


def centred(draw, y, text, fnt, fill):
    w = draw.textlength(text, font=fnt)
    draw.text(((W - w) / 2, y), text, font=fnt, fill=fill)
    return fnt.size


def cover(meta, out, today=None):
    today = today or datetime.date.today().isoformat()
    page = Image.new("RGB", (W, H), WHITE)
    d = ImageDraw.Draw(page)

    band = round(H * 0.36)
    d.rectangle([0, 0, W, band], fill=BLUE)
    paste_logo(page, "mow_symbol.png", mm(28), mm(28), mm(18), mm(14))
    paste_logo(page, "sal_symbol.png", mm(26), mm(26), W - mm(18) - mm(26), mm(15))
    centred(d, mm(52), meta["manual_title"], font("bold", 46), WHITE)
    centred(d, mm(78), meta["manual_subtitle"], font("semibold", 24), WHITE)

    centred(d, band + mm(14), meta["product"], font("bold", 24), BLUE)
    lw, lh = paste_logo(page, "incunest.png", mm(95), mm(80), W // 2, band + mm(30), centre=True)

    y = band + mm(30) + lh + mm(12)
    centred(d, y, f"Hardware {meta['hardware']}", font("semibold", 16), BLUE)
    centred(d, y + mm(10), meta["firmware"], font("regular", 12), GREY)
    centred(d, y + mm(17), f"Ref: {meta['doc_ref']} · Rev. {meta['revision']} · {today}",
            font("regular", 12), GREY)

    if meta.get("draft"):
        txt = "DRAFT — not for clinical use"
        f = font("bold", 16)
        tw = d.textlength(txt, font=f)
        x0, y0 = (W - tw) / 2 - mm(6), max(y + mm(30), H - mm(34))
        d.rectangle([x0, y0, x0 + tw + mm(12), y0 + mm(13)], outline=RED, width=mm(0.8))
        d.text((x0 + mm(6), y0 + mm(2.5)), txt, font=f, fill=RED)

    page.save(out, dpi=(DPI, DPI))
    return out


def back_cover(meta, out, today=None):
    today = today or datetime.date.today().isoformat()
    year = today[:4]
    page = Image.new("RGB", (W, H), BLUE)
    d = ImageDraw.Draw(page)
    x = mm(28)

    def line(y, text, weight="regular", size=13, fill=WHITE):
        d.text((x, y), text, font=font(weight, size), fill=fill)
        return y + round(size * 1.55 / 72 * DPI)

    y = mm(40)
    y = line(y, "Legal manufacturer", "bold", 18)
    y = line(y, meta["manufacturer"]["name"], "semibold", 14)
    for a in meta["manufacturer"]["address"]:
        y = line(y, a)
    y = line(y + mm(4), meta["manufacturer"]["email"])
    y = line(y, meta["manufacturer"]["website"])
    y = line(y + mm(4), f"Assembled by {meta['assembled_by']}")

    y = line(y + mm(16), "Document control", "bold", 18)
    for label, value in (("Document", meta["doc_ref"]), ("Revision", meta["revision"]),
                         ("Date of issue", today), ("Hardware", meta["hardware"]),
                         ("Firmware", meta["firmware"])):
        y = line(y, f"{label}:  {value}")
    if meta.get("draft"):
        y = line(y + mm(4), "DRAFT — not for clinical use", "bold", 13, (255, 205, 210))

    y = mm(205)
    for text in (meta["copyright"].format(year=year),
                 "No part of this manual may be reproduced, copied, translated or transmitted",
                 "in any form without the prior written permission of Medical Open World Association."):
        y = line(y, text, size=9)

    lw, lh = paste_logo(page, "mow_full_white.png", mm(70), mm(40), x, H - mm(62))
    paste_logo(page, "sal_full_white.png", mm(80), mm(30), x + lw + mm(14), H - mm(58))
    page.save(out, dpi=(DPI, DPI))
    return out


if __name__ == "__main__":
    import yaml
    meta = yaml.safe_load((TOOLS_DIR.parent / "en" / "metadata.yaml").read_text(encoding="utf-8"))
    build = TOOLS_DIR.parent / "build"
    build.mkdir(exist_ok=True)
    print(cover(meta, build / "cover.png"), back_cover(meta, build / "back_cover.png"))
