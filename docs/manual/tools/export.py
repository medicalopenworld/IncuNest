#!/usr/bin/env python3
"""Export the IncuNest user manual to Word (.docx) and PDF.

Joins the chapters in docs/manual/<lang>/ (in file-name order), adds a cover
and a table of contents, and writes to docs/manual/build/:

    IncuNest_IFU_V19_<lang>.docx   via pandoc + tools/reference.docx
    IncuNest_IFU_V19_<lang>.html   via pandoc + tools/manual.css (self-contained)
    IncuNest_IFU_V19_<lang>.pdf    the HTML printed with Microsoft Edge (headless)

Callouts (::: warning / caution / note) become coloured boxes; [PENDING: ...]
markers are highlighted; screenshots that do not exist yet are replaced by a
highlighted "screenshot pending" line, so the draft shows what is missing.

Usage:
    python export.py                 # English, Word + PDF
    python export.py --lang es
    python export.py --no-pdf

Requires pandoc (winget install JohnMacFarlane.Pandoc) and, for the PDF,
Microsoft Edge.
"""

import argparse
import datetime
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
MANUAL_DIR = TOOLS_DIR.parent
BUILD_DIR = MANUAL_DIR / "build"

TITLE = "IncuNest — Instructions for Use"
SUBTITLE = "Neonatal incubator · hardware V19"
DRAFT_TEXT = ("DRAFT — not for clinical use. Sections marked PENDING are still "
              "waiting for a decision or a check.")

IMAGE_RE = re.compile(r"!\[([^\]]*)\]\(([^)\s]+)\)(\{[^}]*\})?")
PENDING_RE = re.compile(r"\[PENDING:([^\]]*)\]")


def find_pandoc():
    exe = shutil.which("pandoc")
    if exe:
        return exe
    local = Path(os.environ.get("LOCALAPPDATA", "")) / "Pandoc" / "pandoc.exe"
    if local.exists():
        return str(local)
    sys.exit("pandoc not found: winget install JohnMacFarlane.Pandoc")


def find_edge():
    for base in (os.environ.get("ProgramFiles(x86)"), os.environ.get("ProgramFiles")):
        if base:
            p = Path(base) / "Microsoft" / "Edge" / "Application" / "msedge.exe"
            if p.exists():
                return str(p)
    return shutil.which("msedge")


def prepare_chapter(text, lang_dir):
    """Mark PENDING notes, size screenshots, and flag missing ones."""
    def image(m):
        alt, src, attrs = m.group(1), m.group(2), m.group(3)
        if not (lang_dir / src).exists():
            name = Path(src).stem
            return f"[Screenshot pending: {name}]{{.pending}}"
        return f"![{alt}]({src}){attrs or '{width=12cm}'}"

    text = IMAGE_RE.sub(image, text)
    text = PENDING_RE.sub(lambda m: f"[PENDING:{m.group(1)}]{{.pending}}", text)
    return text


def build_source(lang, versions):
    lang_dir = MANUAL_DIR / lang
    chapters = sorted(p for p in lang_dir.glob("*.md") if p.name[0].isdigit())
    if not chapters:
        sys.exit(f"no chapters found in {lang_dir}")
    today = datetime.date.today().isoformat()
    front = (
        "---\n"
        f"title: \"{TITLE}\"\n"
        f"subtitle: \"{SUBTITLE}{' · ' + versions if versions else ''}\"\n"
        f"date: \"{DRAFT_TEXT} ({today})\"\n"
        f"lang: {lang}\n"
        "---\n\n"
    )
    body = "\n\n".join(prepare_chapter(p.read_text(encoding="utf-8"), lang_dir) for p in chapters)
    return front + body, lang_dir, [p.name for p in chapters]


def run(cmd):
    res = subprocess.run(cmd, capture_output=True, text=True)
    warnings = [l for l in res.stderr.splitlines() if l.strip()]
    if res.returncode != 0:
        sys.exit("command failed:\n  " + " ".join(cmd) + "\n" + res.stderr)
    return warnings


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lang", default="en")
    ap.add_argument("--versions", default="display firmware 4.10 · motherboard firmware 18.6x",
                    help="firmware versions shown on the cover")
    ap.add_argument("--no-pdf", action="store_true")
    args = ap.parse_args()

    pandoc = find_pandoc()
    reference = TOOLS_DIR / "reference.docx"
    if not reference.exists():
        sys.exit("tools/reference.docx missing: run build_reference.py first")

    source, lang_dir, chapters = build_source(args.lang, args.versions)
    BUILD_DIR.mkdir(exist_ok=True)
    base = BUILD_DIR / f"IncuNest_IFU_V19_{args.lang}"
    src_md = base.with_suffix(".md")
    src_md.write_text(source, encoding="utf-8")
    print(f"chapters: {', '.join(chapters)}")

    common = [pandoc, str(src_md), "--from", "markdown", "--resource-path", str(lang_dir),
              "--toc", "--toc-depth", "2", "--lua-filter", str(TOOLS_DIR / "callouts.lua")]

    warn = run(common + ["--reference-doc", str(reference), "-o", str(base.with_suffix(".docx"))])
    print(f"wrote {base.with_suffix('.docx').relative_to(MANUAL_DIR)}")

    warn += run(common + ["--standalone", "--embed-resources", "--css", str(TOOLS_DIR / "manual.css"),
                          "--metadata", f"pagetitle={TITLE}", "-o", str(base.with_suffix(".html"))])
    print(f"wrote {base.with_suffix('.html').relative_to(MANUAL_DIR)}")

    if not args.no_pdf:
        edge = find_edge()
        if not edge:
            print("Edge not found: PDF skipped")
        else:
            pdf = base.with_suffix(".pdf")
            if pdf.exists():
                pdf.unlink()
            # A separate profile keeps a running Edge window from taking over the job.
            profile = BUILD_DIR / ".edge-profile"
            subprocess.run([edge, "--headless=new", "--disable-gpu", "--no-pdf-header-footer",
                            f"--user-data-dir={profile}", f"--print-to-pdf={pdf}",
                            base.with_suffix(".html").as_uri()],
                           capture_output=True, timeout=180)
            # Edge can hand the job to a child process and return before the
            # file is written: wait for it to appear and stop growing.
            last = -1
            for _ in range(120):
                size = pdf.stat().st_size if pdf.exists() else -1
                if size > 0 and size == last:
                    break
                last = size
                time.sleep(1)
            print(f"wrote {pdf.relative_to(MANUAL_DIR)}" if pdf.exists() else "PDF failed")

    for w in sorted(set(warn)):
        print(f"pandoc: {w}")


if __name__ == "__main__":
    main()
