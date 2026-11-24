#!/usr/bin/env python3
"""Build tools/reference.docx, the Word template pandoc uses for the manual.

Starts from pandoc's default reference.docx and sets the manual's look:
fonts, heading colours, and the paragraph/character styles that export.py
maps the Markdown callouts to:

    ::: warning / ::: caution / ::: note   ->  "Warning Box" / "Caution Box" / "Note Box"
    [PENDING: ...]                          ->  "Pending" (character style)

Run it again after changing the look; commit the resulting reference.docx.
Requires pandoc on PATH (or in %LOCALAPPDATA%\\Pandoc) and python-docx.
"""

import subprocess
import sys
from pathlib import Path

from docx import Document
from docx.enum.style import WD_STYLE_TYPE
from docx.enum.text import WD_COLOR_INDEX
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Pt, RGBColor

sys.path.insert(0, str(Path(__file__).resolve().parent))
from export import find_pandoc  # noqa: E402

TOOLS_DIR = Path(__file__).resolve().parent
OUT = TOOLS_DIR / "reference.docx"

FONT = "Open Sans"          # same family as the previous manual (v6)
BLUE = RGBColor(0x1B, 0x3A, 0x6B)  # dark blue for headings

BOXES = {
    # style name: (border colour, fill colour)
    "Warning Box": ("C62828", "FDECEA"),
    "Caution Box": ("E08800", "FFF4E0"),
    "Note Box": ("1565C0", "E8F1FB"),
}


def set_box(style, border_hex, fill_hex):
    """Give a paragraph style a left border, a thin frame and a background."""
    ppr = style.element.get_or_add_pPr()
    borders = OxmlElement("w:pBdr")
    for side, size in (("top", 4), ("left", 24), ("bottom", 4), ("right", 4)):
        b = OxmlElement(f"w:{side}")
        b.set(qn("w:val"), "single")
        b.set(qn("w:sz"), str(size))
        b.set(qn("w:space"), "4")
        b.set(qn("w:color"), border_hex)
        borders.append(b)
    ppr.append(borders)
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), fill_hex)
    ppr.append(shd)


def set_font(style, name):
    """Set the font and drop pandoc's theme-font attributes, which would win over it."""
    style.font.name = name
    rpr = style.element.get_or_add_rPr()
    rfonts = rpr.find(qn("w:rFonts"))
    if rfonts is None:
        rfonts = OxmlElement("w:rFonts")
        rpr.append(rfonts)
    for attr in ("w:asciiTheme", "w:hAnsiTheme", "w:eastAsiaTheme", "w:cstheme"):
        if rfonts.get(qn(attr)) is not None:
            del rfonts.attrib[qn(attr)]
    for attr in ("w:ascii", "w:hAnsi", "w:cs"):
        rfonts.set(qn(attr), name)


def set_table_borders(style, colour="C8C8C8"):
    """Thin grey borders on every cell of tables using this style."""
    tblpr = style.element.find(qn("w:tblPr"))
    if tblpr is None:
        tblpr = OxmlElement("w:tblPr")
        style.element.append(tblpr)
    borders = OxmlElement("w:tblBorders")
    for side in ("top", "left", "bottom", "right", "insideH", "insideV"):
        b = OxmlElement(f"w:{side}")
        b.set(qn("w:val"), "single")
        b.set(qn("w:sz"), "4")
        b.set(qn("w:space"), "0")
        b.set(qn("w:color"), colour)
        borders.append(b)
    tblpr.append(borders)


def main():
    pandoc = find_pandoc()
    data = subprocess.run([pandoc, "--print-default-data-file", "reference.docx"],
                          capture_output=True, check=True).stdout
    OUT.write_bytes(data)

    doc = Document(str(OUT))
    styles = doc.styles

    names = [s.name for s in styles]
    for name in ("Normal", "Body Text", "First Paragraph", "Compact", "Date",
                 "Image Caption", "Table Caption", "Block Text", "TOC Heading"):
        if name in names:
            set_font(styles[name], FONT)
    for name in ("Normal", "Body Text", "First Paragraph", "Compact"):
        if name in names:
            styles[name].font.size = Pt(10.5)

    for level, size in ((1, 20), (2, 14), (3, 12)):
        st = styles[f"Heading {level}"]
        set_font(st, FONT)
        st.font.size = Pt(size)
        st.font.bold = True
        st.font.color.rgb = BLUE
    styles["Heading 1"].paragraph_format.page_break_before = True  # each chapter on a new page

    for name in ("Title", "Subtitle"):
        set_font(styles[name], FONT)
        styles[name].font.color.rgb = BLUE
    if "Date" in names:
        styles["Date"].font.color.rgb = RGBColor(0xC6, 0x28, 0x28)
        styles["Date"].font.bold = True

    if "Table" in names:
        set_table_borders(styles["Table"])

    for name, (border, fill) in BOXES.items():
        st = styles.add_style(name, WD_STYLE_TYPE.PARAGRAPH)
        st.base_style = styles["Normal"]
        set_font(st, FONT)
        st.font.size = Pt(10.5)
        st.paragraph_format.space_before = Pt(4)
        st.paragraph_format.space_after = Pt(4)
        set_box(st, border, fill)

    pending = styles.add_style("Pending", WD_STYLE_TYPE.CHARACTER)
    pending.font.highlight_color = WD_COLOR_INDEX.YELLOW
    pending.font.italic = True

    doc.save(str(OUT))
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
