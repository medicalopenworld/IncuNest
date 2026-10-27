"""Ejecuta toda la revisión y guarda la salida en exports/revision_<fecha>.txt.

python tools/run_all.py [exports]
Antes de una nueva exportación: mover el contenido actual de exports/ a exports/prev/ (al menos IncuNest.NET e IncuNest.xlsx).
"""
import contextlib
import datetime
import io
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import connectivity  # noqa: E402
import netcheck  # noqa: E402
import pcbcheck  # noqa: E402
import sync  # noqa: E402


def main(exp='exports'):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        print(f'# Revisión {datetime.datetime.now():%Y-%m-%d %H:%M}\n')
        netcheck.report(exp)
        print()
        sync.main(exp)
        print()
        connectivity.main(os.path.join(exp, 'odb'))
        print()
        pcbcheck.main(os.path.join(exp, 'odb'))
    out = os.path.join(exp, f'revision_{datetime.date.today():%Y-%m-%d}.txt')
    with open(out, 'w', encoding='utf-8') as fh:
        fh.write(buf.getvalue())
    print(buf.getvalue())
    print(f'-> {out}')


if __name__ == '__main__':
    main(*sys.argv[1:])
