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

import datetime
import shutil
import os
import subprocess
from pathlib import Path

import yaml
from docx import Document
from docx.enum.style import WD_STYLE_TYPE
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_COLOR_INDEX, WD_TAB_ALIGNMENT
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor

TOOLS_DIR = Path(__file__).resolve().parent
MANUAL_DIR = TOOLS_DIR.parent
ASSETS = TOOLS_DIR / "assets"
OUT = TOOLS_DIR / "reference.docx"


def find_pandoc():
    exe = shutil.which("pandoc")
    if exe:
        return exe
    local = Path(os.environ.get("LOCALAPPDATA", "")) / "Pandoc" / "pandoc.exe"
    if local.exists():
        return str(local)
    raise SystemExit("pandoc not found: winget install JohnMacFarlane.Pandoc")

FONT = "Calibri"  # installed with Office; LibreOffice uses the metric-compatible Carlito
BLUE = RGBColor(0x1B, 0x3A, 0x6B)  # dark blue for headings

GUIDE_HEX = "E8505B"  # coral of the MOW logo: marks the quick guide pages
GUIDE = RGBColor.from_string(GUIDE_HEX)

BOXES = {
    # style name: (border colour, fill colour)
    "Warning Box": ("C62828", "FDECEA"),
    "Caution Box": ("E08800", "FFF4E0"),
    "Note Box": ("1565C0", "E8F1FB"),
    "Guide Intro": (GUIDE_HEX, "FDECEC"),
    "Guide End": (GUIDE_HEX, "FDECEC"),
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


def add_field(paragraph, instr):
    """Insert a Word field (PAGE, NUMPAGES...) at the end of a paragraph."""
    fld = OxmlElement("w:fldSimple")
    fld.set(qn("w:instr"), instr)
    run = OxmlElement("w:r")
    text = OxmlElement("w:t")
    text.text = "1"
    run.append(text)
    fld.append(run)
    paragraph._p.append(fld)


def set_border(paragraph, side, colour="1B3A6B"):
    ppr = paragraph._p.get_or_add_pPr()
    borders = OxmlElement("w:pBdr")
    b = OxmlElement(f"w:{side}")
    b.set(qn("w:val"), "single")
    b.set(qn("w:sz"), "6")
    b.set(qn("w:space"), "4")
    b.set(qn("w:color"), colour)
    borders.append(b)
    ppr.append(borders)


def fill_header(header, text, text_width, colour=BLUE, bold=False, border_hex="1B3A6B"):
    """MOW logo on the left, the given text on the right, a line underneath."""
    hp = header.paragraphs[0]
    hp.paragraph_format.tab_stops.add_tab_stop(text_width, WD_TAB_ALIGNMENT.RIGHT)
    hp.add_run().add_picture(str(ASSETS / "mow_logo_color.jpg"), height=Cm(0.9))
    r = hp.add_run("\t" + text)
    r.font.size, r.font.color.rgb, r.font.name, r.font.bold = Pt(9), colour, FONT, bold
    set_border(hp, "bottom", border_hex)
    return hp


def page_setup(doc, meta, year):
    """A4 page, margins, and the running header and footer of the manual."""
    sec = doc.sections[0]
    sec.page_width, sec.page_height = Cm(21.0), Cm(29.7)
    sec.left_margin = sec.right_margin = Cm(2.2)
    sec.top_margin, sec.bottom_margin = Cm(2.6), Cm(2.2)
    sec.header_distance = sec.footer_distance = Cm(1.0)
    text_width = sec.page_width - sec.left_margin - sec.right_margin

    fill_header(sec.header, meta["header_text"], text_width)

    fp = sec.footer.paragraphs[0]
    fp.paragraph_format.tab_stops.add_tab_stop(text_width, WD_TAB_ALIGNMENT.RIGHT)
    set_border(fp, "top")
    for piece in ("Page ", ("PAGE",), " of ", ("NUMPAGES",),
                  f"\tRef: {meta['doc_ref']} (Rev. {meta['revision']}) | {year}"):
        if isinstance(piece, tuple):
            add_field(fp, piece[0])
        else:
            r = fp.add_run(piece)
            r.font.size, r.font.name = Pt(9), FONT


def build(meta, out=OUT, year=None):
    """Write the reference docx for this manual's metadata."""
    year = year or datetime.date.today().year
    pandoc = find_pandoc()
    data = subprocess.run([pandoc, "--print-default-data-file", "reference.docx"],
                          capture_output=True, check=True).stdout
    out.write_bytes(data)

    doc = Document(str(out))
    page_setup(doc, meta, year)
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

    # Screenshots: centred, with room above, caption right under the picture.
    for name in ("Figure", "Captioned Figure"):
        if name in names:
            pf = styles[name].paragraph_format
            pf.alignment = WD_ALIGN_PARAGRAPH.CENTER
            pf.space_before, pf.space_after = Pt(18), Pt(2)
            pf.keep_with_next = True
    if "Image Caption" in names:
        st = styles["Image Caption"]
        st.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
        st.paragraph_format.space_before, st.paragraph_format.space_after = Pt(2), Pt(20)
        st.font.size, st.font.italic = Pt(9), True
        st.font.color.rgb = RGBColor(0x55, 0x55, 0x55)

    for name, (border, fill) in BOXES.items():
        st = styles.add_style(name, WD_STYLE_TYPE.PARAGRAPH)
        st.base_style = styles["Normal"]
        set_font(st, FONT)
        st.font.size = Pt(10.5)
        st.paragraph_format.space_before = Pt(4)
        st.paragraph_format.space_after = Pt(4)
        set_box(st, border, fill)
    for name in ("Guide Intro", "Guide End"):
        st = styles[name]
        st.font.size = Pt(11)
        st.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
        st.paragraph_format.space_before = Pt(10)
        st.paragraph_format.space_after = Pt(10)

    pending = styles.add_style("Pending", WD_STYLE_TYPE.CHARACTER)
    pending.font.highlight_color = WD_COLOR_INDEX.YELLOW
    pending.font.italic = True

    doc.save(str(out))
    return out


def main():
    meta = yaml.safe_load((MANUAL_DIR / "en" / "metadata.yaml").read_text(encoding="utf-8"))
    print(f"wrote {build(meta)}")


if __name__ == "__main__":
    main()
