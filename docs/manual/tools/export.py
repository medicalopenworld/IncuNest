#!/usr/bin/env python3
"""Export the IncuNest user manual to Word (.docx) and PDF.

Joins the chapters in docs/manual/<lang>/ (in file-name order) and writes to
docs/manual/build/ (not versioned):

    IncuNest_IFU_V19_<lang>.docx   A4, with cover, table of contents, running
                                   header and footer, and back cover
    IncuNest_IFU_V19_<lang>.pdf    the same document printed by LibreOffice,
                                   with the table of contents filled in

What goes where:
- Text: the chapters (NN-*.md).
- Data on cover, header, footer and back cover: <lang>/metadata.yaml.
- Look: build_reference.py (styles, page, header, footer), covers.py (cover
  and back cover), callouts.lua (warning/caution/note boxes, PENDING marks).

Usage:
    python export.py              # English, Word + PDF
    python export.py --lang es
    python export.py --no-pdf

Requires pandoc (winget install JohnMacFarlane.Pandoc), python-docx, PyYAML,
Pillow, segno (QR on the back cover) and, for the PDF, LibreOffice.
"""

import argparse
import copy
import datetime
import re
import subprocess
import sys
from pathlib import Path

import yaml
from docx import Document
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt

TOOLS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS_DIR))
import build_reference  # noqa: E402
import covers  # noqa: E402

MANUAL_DIR = TOOLS_DIR.parent
BUILD_DIR = MANUAL_DIR / "build"
LO_PYTHON = Path("C:/Program Files/LibreOffice/program/python.exe")

IMAGE_RE = re.compile(r"!\[([^\]]*)\]\(([^)\s]+)\)(\{[^}]*\})?")
PENDING_RE = re.compile(r"\[PENDING:([^\]]*)\]")


QUICK_GUIDE = "00-quick-guide.md"


def fill_placeholders(text, meta):
    """Replace {{key}} with values from metadata.yaml (manufacturer fields too)."""
    values = {k: v for k, v in meta.items() if isinstance(v, str)}
    values.update({k: v for k, v in meta.get("manufacturer", {}).items() if isinstance(v, str)})
    return re.sub(r"\{\{(\w+)\}\}", lambda m: values.get(m.group(1), m.group(0)), text)


def prepare_chapter(text, lang_dir, meta):
    """Fill placeholders, mark PENDING notes, size screenshots, flag missing ones."""
    text = fill_placeholders(text, meta)

    def image(m):
        alt, src, attrs = m.group(1), m.group(2), m.group(3)
        if not (lang_dir / src).exists():
            return f"[Screenshot pending: {Path(src).stem}]{{.pending}}"
        return f"![{alt}]({src}){attrs or '{width=10cm}'}"

    text = IMAGE_RE.sub(image, text)
    return PENDING_RE.sub(lambda m: f"[PENDING:{m.group(1)}]{{.pending}}", text)


def full_page_picture_paragraph(doc, image):
    """A paragraph holding one A4 picture, with no spacing around it."""
    p = doc.add_paragraph()
    pf = p.paragraph_format
    pf.space_before = pf.space_after = Pt(0)
    pf.line_spacing = 1.0
    p.add_run().add_picture(str(image), width=Cm(20.99), height=Cm(29.65))
    p._p.getparent().remove(p._p)  # caller places it
    return p._p


def section_break(sect_pr):
    """An empty paragraph that ends a section with the given properties."""
    p = OxmlElement("w:p")
    ppr = OxmlElement("w:pPr")
    ppr.append(sect_pr)
    p.append(ppr)
    return p


def zero_margins(sect_pr):
    mar = sect_pr.find(qn("w:pgMar"))
    for side in ("w:top", "w:bottom", "w:left", "w:right", "w:header", "w:footer", "w:gutter"):
        mar.set(qn(side), "0")


GUIDE_HEADER = "QUICK GUIDE — read before use"
WP_NS = "http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing"


def anchor_band(paragraph, band_png, page_width):
    """Add the coloured strip as a picture floating behind the text, at the
    right edge of the page (Word anchors it from the header, so it is on every
    page of the section)."""
    run = paragraph.add_run()
    run.add_picture(str(band_png), width=Cm(covers.BAND_W_CM), height=Cm(29.7))
    inline = run._r.find(".//{%s}inline" % WP_NS)
    anchor = OxmlElement("wp:anchor")
    for k, v in (("distT", "0"), ("distB", "0"), ("distL", "0"), ("distR", "0"),
                 ("simplePos", "0"), ("relativeHeight", "1"), ("behindDoc", "1"),
                 ("locked", "1"), ("layoutInCell", "1"), ("allowOverlap", "1")):
        anchor.set(k, v)
    simple = OxmlElement("wp:simplePos")
    simple.set("x", "0")
    simple.set("y", "0")
    anchor.append(simple)
    for tag, offset in (("wp:positionH", int(page_width - Cm(covers.BAND_W_CM))), ("wp:positionV", 0)):
        pos = OxmlElement(tag)
        pos.set("relativeFrom", "page")
        off = OxmlElement("wp:posOffset")
        off.text = str(offset)
        pos.append(off)
        anchor.append(pos)
    extent = inline.find("{%s}extent" % WP_NS)
    anchor.append(extent)
    eff = OxmlElement("wp:effectExtent")
    for k in ("l", "t", "r", "b"):
        eff.set(k, "0")
    anchor.append(eff)
    anchor.append(OxmlElement("wp:wrapNone"))
    for tag in ("docPr", "cNvGraphicFramePr"):
        el = inline.find("{%s}%s" % (WP_NS, tag))
        if el is not None:
            anchor.append(el)
    anchor.append(inline.find("{http://schemas.openxmlformats.org/drawingml/2006/main}graphic"))
    inline.getparent().replace(inline, anchor)


def guide_header(section, band_png):
    """Header of the quick guide pages: its own text in the guide colour, and the strip."""
    hp = section.header.paragraphs[0]
    ppr = hp._p.find(qn("w:pPr"))
    if ppr is not None:
        hp._p.remove(ppr)
    for r in list(hp._p.findall(qn("w:r"))):
        hp._p.remove(r)
    text_width = section.page_width - section.left_margin - section.right_margin
    p = build_reference.fill_header(section.header, GUIDE_HEADER, text_width,
                                    colour=build_reference.GUIDE, bold=True,
                                    border_hex=build_reference.GUIDE_HEX)
    anchor_band(p, band_png, section.page_width)


def mark_quick_guide(doc, band_png):
    """Give the quick guide chapter a section of its own, with its own header and strip."""
    body = doc.element.body
    main_sect = body.find(qn("w:sectPr"))
    heads = [p for p in body.findall(qn("w:p"))
             if (s := p.find(qn("w:pPr") + "/" + qn("w:pStyle"))) is not None and s.get(qn("w:val")) == "Heading1"]
    start = next((i for i, p in enumerate(heads) if "".join(t.text or "" for t in p.iter(qn("w:t"))).strip() == "Quick guide"), None)
    if start is None or start + 1 >= len(heads):
        return
    guide_head, next_head = heads[start], heads[start + 1]
    # End the section before the guide (contents) with the normal header ...
    body.insert(list(body).index(guide_head), section_break(copy.deepcopy(main_sect)))
    # ... and end the guide's own section just before the next chapter.
    guide_sect = copy.deepcopy(main_sect)
    for ref in guide_sect.findall(qn("w:headerReference")):
        guide_sect.remove(ref)
    body.insert(list(body).index(next_head), section_break(guide_sect))
    sec = doc.sections[1]
    sec.header.is_linked_to_previous = False
    guide_header(sec, band_png)


def add_covers(docx_path, cover_png, back_png, band_png=None):
    """Put the cover first and the back cover last, each on a page of its own
    without header, footer or margins; ask Word to refresh the contents table."""
    doc = Document(str(docx_path))
    if band_png:
        mark_quick_guide(doc, band_png)
    body = doc.element.body
    main_sect = body.find(qn("w:sectPr"))

    # Cover: first section, no header/footer references, no margins.
    cover_sect = copy.deepcopy(main_sect)
    for ref in cover_sect.findall(qn("w:headerReference")) + cover_sect.findall(qn("w:footerReference")):
        cover_sect.remove(ref)
    zero_margins(cover_sect)
    body.insert(0, section_break(cover_sect))
    body.insert(0, full_page_picture_paragraph(doc, cover_png))

    # Back cover: close the main section with its own header/footer, then a last
    # section with empty header/footer and no margins.
    body.insert(list(body).index(main_sect), section_break(copy.deepcopy(main_sect)))
    body.insert(list(body).index(main_sect), full_page_picture_paragraph(doc, back_png))
    zero_margins(main_sect)
    # The last section still points at the shared header/footer parts: drop
    # those references and give it new, empty ones.
    for ref in main_sect.findall(qn("w:headerReference")) + main_sect.findall(qn("w:footerReference")):
        main_sect.remove(ref)
    last = doc.sections[-1]
    last.header.is_linked_to_previous = False
    last.footer.is_linked_to_previous = False

    settings = doc.settings.element
    upd = OxmlElement("w:updateFields")
    upd.set(qn("w:val"), "true")
    settings.append(upd)
    doc.save(str(docx_path))


def run(cmd):
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        sys.exit("command failed:\n  " + " ".join(map(str, cmd)) + "\n" + res.stderr + res.stdout)
    return [line for line in res.stderr.splitlines() if line.strip()]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lang", default="en")
    ap.add_argument("--no-pdf", action="store_true")
    args = ap.parse_args()

    lang_dir = MANUAL_DIR / args.lang
    meta = yaml.safe_load((lang_dir / "metadata.yaml").read_text(encoding="utf-8"))
    chapters = sorted(p for p in lang_dir.glob("*.md") if p.name[0].isdigit())
    if not chapters:
        sys.exit(f"no chapters found in {lang_dir}")
    today = datetime.date.today().isoformat()

    BUILD_DIR.mkdir(exist_ok=True)
    base = BUILD_DIR / f"IncuNest_IFU_{meta['hardware']}_{args.lang}"
    src_md = base.with_suffix(".md")
    full_meta = dict(meta,
                     guide_rest="The rest of this manual gives more detail and is for reference.",
                     guide_end="The following chapters give more detail and are for reference.")
    src_md.write_text("\n\n".join(prepare_chapter(p.read_text(encoding="utf-8"), lang_dir, full_meta)
                                  for p in chapters), encoding="utf-8")
    print(f"chapters: {', '.join(p.name for p in chapters)}")

    reference = build_reference.build(meta, BUILD_DIR / "reference.docx", year=today[:4])
    cover_png = covers.cover(meta, BUILD_DIR / "cover.png", today)
    back_png = covers.back_cover(meta, BUILD_DIR / "back_cover.png", today)
    band_png = covers.guide_band(BUILD_DIR / "guide_band.png")

    docx = base.with_suffix(".docx")
    warn = run([build_reference.find_pandoc(), str(src_md), "--from", "markdown",
                "--resource-path", str(lang_dir), "--toc", "--toc-depth", "2",
                "--metadata", "toc-title=Contents",
                "--lua-filter", str(TOOLS_DIR / "callouts.lua"),
                "--reference-doc", str(reference), "-o", str(docx)])
    add_covers(docx, cover_png, back_png, band_png)
    print(f"wrote {docx.relative_to(MANUAL_DIR)}")
    outputs = [docx]

    # The quick guide is chapter 0 of the manual and is also exported on its own,
    # to print and keep next to the incubator. Same text, same reference and revision.
    guide_src = lang_dir / QUICK_GUIDE
    if guide_src.exists():
        guide_base = BUILD_DIR / f"IncuNest_QuickGuide_{meta['hardware']}_{args.lang}"
        guide_meta = dict(meta,
                          guide_rest="The Instructions for Use (IFU) give more detail.",
                          guide_end=f"For more detail, see the Instructions for Use ({meta['doc_ref']}).")
        text = prepare_chapter(guide_src.read_text(encoding="utf-8"), lang_dir, guide_meta)
        # Without the rest of the manual, chapter references point to the IFU.
        text = re.sub(r"\(see (chapter|section) ", r"(see IFU, \1 ", text)
        text = re.sub(r"\A#\s+[^\n]*\n", "", text.lstrip())   # the title replaces the heading
        guide_md = guide_base.with_suffix(".md")
        guide_md.write_text(text, encoding="utf-8")
        guide_docx = guide_base.with_suffix(".docx")
        warn += run([build_reference.find_pandoc(), str(guide_md), "--from", "markdown",
                     "--resource-path", str(lang_dir),
                     "--metadata", f"title={meta['product']} — Quick guide",
                     "--metadata", f"subtitle=IncuNest · hardware {meta['hardware']}",
                     "--lua-filter", str(TOOLS_DIR / "callouts.lua"),
                     "--reference-doc", str(reference), "-o", str(guide_docx)])
        gdoc = Document(str(guide_docx))
        guide_header(gdoc.sections[0], band_png)
        gdoc.save(str(guide_docx))
        print(f"wrote {guide_docx.relative_to(MANUAL_DIR)}")
        outputs.append(guide_docx)

    if not args.no_pdf:
        if not LO_PYTHON.exists():
            print("LibreOffice not found: PDF skipped")
        else:
            for out in outputs:
                pdf = out.with_suffix(".pdf")
                run([str(LO_PYTHON), str(TOOLS_DIR / "lo_pdf.py"), str(out), str(pdf)])
                print(f"wrote {pdf.relative_to(MANUAL_DIR)}" if pdf.exists() else "PDF failed")

    for w in sorted(set(warn)):
        print(f"pandoc: {w}")


if __name__ == "__main__":
    main()
