"""Convert a .docx to PDF with LibreOffice, updating the table of contents first.

Run it with LibreOffice's own Python, which has the `uno` module:
    "C:\\Program Files\\LibreOffice\\program\\python.exe" lo_pdf.py in.docx out.pdf

export.py calls it for you.
"""

import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import uno
from com.sun.star.beans import PropertyValue

PIPE = "incunest_manual_export"


def prop(name, value):
    p = PropertyValue()
    p.Name, p.Value = name, value
    return p


def main(src, dst):
    soffice = Path(sys.executable).with_name("soffice.exe")
    profile = Path(tempfile.gettempdir()) / "lo_incunest_manual_profile"
    proc = subprocess.Popen([str(soffice), f"-env:UserInstallation={profile.as_uri()}",
                             "--headless", "--invisible", "--norestore", "--nologo",
                             f"--accept=pipe,name={PIPE};urp;"])
    try:
        local = uno.getComponentContext()
        resolver = local.ServiceManager.createInstanceWithContext(
            "com.sun.star.bridge.UnoUrlResolver", local)
        ctx = None
        for _ in range(60):
            try:
                ctx = resolver.resolve(f"uno:pipe,name={PIPE};urp;StarOffice.ComponentContext")
                break
            except Exception:
                time.sleep(0.5)
        if ctx is None:
            sys.exit("could not connect to LibreOffice")
        desktop = ctx.ServiceManager.createInstanceWithContext("com.sun.star.frame.Desktop", ctx)
        doc = desktop.loadComponentFromURL(Path(src).resolve().as_uri(), "_blank", 0,
                                           (prop("Hidden", True),))
        if doc is None:
            sys.exit(f"LibreOffice could not open {src}")
        indexes = doc.getDocumentIndexes()
        for i in range(indexes.getCount()):
            indexes.getByIndex(i).update()
        doc.getTextFields().refresh()
        doc.storeToURL(Path(dst).resolve().as_uri(), (prop("FilterName", "writer_pdf_Export"),))
        doc.close(True)
        try:
            desktop.terminate()
        except Exception:
            pass  # the bridge drops when LibreOffice exits
    finally:
        try:
            proc.wait(timeout=30)
        except subprocess.TimeoutExpired:
            proc.kill()


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
