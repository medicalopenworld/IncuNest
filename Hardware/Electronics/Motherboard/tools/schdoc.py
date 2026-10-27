"""Lectura (solo lectura) de .SchDoc de Altium: componentes, designadores y parámetros.

python tools/schdoc.py [Schematic] [REF ...]
"""
import glob
import os
import struct
import sys

import olefile


def records(path):
    data = olefile.OleFileIO(path).openstream('FileHeader').read()
    i, out = 0, []
    while i + 4 <= len(data):
        n = struct.unpack('<I', data[i:i + 4])[0] & 0x00FFFFFF
        raw = data[i + 4:i + 4 + n]
        i += 4 + n
        txt = raw.rstrip(b'\x00').decode('latin-1', 'ignore')
        d = {}
        for kv in txt.split('|'):
            if '=' in kv:
                k, v = kv.split('=', 1)
                d[k.upper()] = v
        out.append(d)
    return out


def components(path):
    recs = records(path)
    comps = {}
    for idx, r in enumerate(recs):
        if r.get('RECORD') == '1':
            comps[idx - 1] = {'lib': r.get('LIBREFERENCE', ''), 'params': {}, 'des': None, 'comment': None,
                              'sheet': os.path.basename(path)}
    for r in recs:
        own = r.get('OWNERINDEX')
        if own is None or int(own) not in comps:
            continue
        c = comps[int(own)]
        if r.get('RECORD') == '34':
            c['des'] = r.get('TEXT')
        elif r.get('RECORD') == '41':
            name = r.get('NAME', '')
            if name.upper() == 'COMMENT':
                c['comment'] = r.get('TEXT')
            else:
                c['params'][name] = r.get('TEXT', '')
    return [c for c in comps.values() if c['des']]


def all_components(folder='Schematic'):
    out = []
    for f in sorted(glob.glob(os.path.join(folder, '*.SchDoc'))):
        out += components(f)
    return out


if __name__ == '__main__':
    folder = sys.argv[1] if len(sys.argv) > 1 else 'Schematic'
    refs = set(sys.argv[2:])
    for c in all_components(folder):
        if not refs or c['des'] in refs:
            print(f"{c['des']:6s} {c['sheet']:24s} {c['comment']!s:28s} LCSC={c['params'].get('LCSC_PN')} "
                  f"MPN={c['params'].get('Manufacturer_Part_Number')}")
