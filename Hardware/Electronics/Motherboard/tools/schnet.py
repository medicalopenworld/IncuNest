"""Conectividad de un .SchDoc (solo lectura): pines, cables, etiquetas de net, puertos de alimentación y uniones.

Sirve para revisar un esquema antes de hacer el ECO. Solo resuelve nets dentro de la hoja (las etiquetas,
puertos de alimentación y puertos de hoja dan el nombre); la conectividad entre hojas se valida con el ODB++.

python tools/schnet.py Schematic/PowerInput.SchDoc [REF ...]
"""
import collections
import struct
import sys

import olefile


def _raw_records(path):
    data = olefile.OleFileIO(path).openstream('FileHeader').read()
    i = 0
    while i + 4 <= len(data):
        hdr = struct.unpack('<I', data[i:i + 4])[0]
        n, kind = hdr & 0x00FFFFFF, hdr >> 24
        yield kind, data[i + 4:i + 4 + n]
        i += 4 + n


def _txt(raw):
    d = {}
    for kv in raw.rstrip(b'\x00').decode('latin-1', 'ignore').split('|'):
        if '=' in kv:
            k, v = kv.split('=', 1)
            d[k.upper()] = v
    return d


def _pstr(b, i):
    n = b[i]
    return b[i + 1:i + 1 + n].decode('latin-1', 'ignore'), i + 1 + n


def _pin(b):
    i = 4 + 1 + 2 + 1 + 4  # RECORD, ?, OwnerPartId, DisplayMode, 4 símbolos
    _, i = _pstr(b, i)     # descripción
    i += 2                 # FormalType, Electrical
    cong = b[i]; i += 1
    length, x, y = struct.unpack('<hhh', b[i:i + 6]); i += 6
    i += 4                 # color
    name, i = _pstr(b, i)
    des, i = _pstr(b, i)
    return dict(name=name, des=des, x=x, y=y, length=length, rot=cong & 3)


def _xy(r, k=''):
    return (int(r.get(f'LOCATION.X{k}', r.get(f'X{k}', 0))) + int(r.get(f'LOCATION.X{k}_FRAC', r.get(f'X{k}_FRAC', 0))) / 1e5,
            int(r.get(f'LOCATION.Y{k}', r.get(f'Y{k}', 0))) + int(r.get(f'LOCATION.Y{k}_FRAC', r.get(f'Y{k}_FRAC', 0))) / 1e5)


def parse(path):
    comps, pins, wires, labels = {}, [], [], []
    idx = -1
    cur = None
    for kind, raw in _raw_records(path):
        idx += 1
        if kind == 1:
            if struct.unpack('<i', raw[:4])[0] == 2 and cur is not None:
                p = _pin(raw)
                p['owner'] = cur
                pins.append(p)
            continue
        r = _txt(raw)
        rec = r.get('RECORD')
        if rec == '1':
            cur = idx - 1
            comps[cur] = {'des': None, 'part': r.get('CURRENTPARTID', '1'), 'disp': r.get('DISPLAYMODE', '0')}
        elif rec == '2' and r.get('OWNERINDEX') is not None and int(r['OWNERINDEX']) in comps:
            c = comps[int(r['OWNERINDEX'])]
            if r.get('OWNERPARTID', c['part']) not in (c['part'], '-1', '0') or \
                    r.get('OWNERPARTDISPLAYMODE', c['disp']) != c['disp']:
                continue
            x, y = _xy(r)
            pins.append(dict(name=r.get('NAME', ''), des=r.get('DESIGNATOR', ''), x=x, y=y,
                             length=int(r.get('PINLENGTH', 0)) + int(r.get('PINLENGTH_FRAC', 0)) / 1e5,
                             rot=int(r.get('PINCONGLOMERATE', 0)) & 3, owner=int(r['OWNERINDEX'])))
        elif rec == '34' and r.get('OWNERINDEX') is not None and int(r['OWNERINDEX']) in comps:
            comps[int(r['OWNERINDEX'])]['des'] = r.get('TEXT')
        elif rec == '27':
            pts = [_xy(r, str(k)) for k in range(1, int(r.get('LOCATIONCOUNT', 0)) + 1)]
            wires += list(zip(pts, pts[1:]))
        elif rec in ('25', '17', '18'):  # etiqueta de net, puerto de alimentación, puerto de hoja
            labels.append((r.get('TEXT') or r.get('NAME'), _xy(r), rec))
        elif rec == '29':
            labels.append((None, _xy(r), rec))
    return comps, pins, wires, labels


def nets(path):
    comps, pins, wires, labels = parse(path)
    D = [(1, 0), (0, 1), (-1, 0), (0, -1)]
    parent = {}

    def f(a):
        parent.setdefault(a, a)
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def u(a, b):
        parent[f(a)] = f(b)

    def key(p):
        return (round(p[0], 2), round(p[1], 2))

    def on_seg(p, a, b):
        (x, y), (x1, y1), (x2, y2) = p, a, b
        if abs((x2 - x1) * (y - y1) - (y2 - y1) * (x - x1)) > 1e-3:
            return False
        return min(x1, x2) - 1e-3 <= x <= max(x1, x2) + 1e-3 and min(y1, y2) - 1e-3 <= y <= max(y1, y2) + 1e-3

    pts = []
    for p in pins:
        c = comps.get(p['owner'])
        if not c or not c['des']:
            continue
        dx, dy = D[p['rot']]
        e = key((p['x'] + dx * p['length'], p['y'] + dy * p['length']))
        p['end'] = e
        p['ref'] = c['des']
        pts.append(e)
    for _, xy, _ in labels:
        pts.append(key(xy))
    for a, b in wires:
        u(key(a), key(b))
        pts += [key(a), key(b)]
    for p in set(pts):
        f(p)
        for a, b in wires:
            if on_seg(p, a, b):
                u(p, key(a))
    groups = collections.defaultdict(lambda: {'pins': set(), 'names': set()})
    for p in pins:
        if 'end' in p:
            groups[f(p['end'])]['pins'].add(f"{p['ref']}-{p['des']}")
    for name, xy, rec in labels:
        if name:
            groups[f(key(xy))]['names'].add(name)
    return [g for g in groups.values() if g['pins']]


if __name__ == '__main__':
    path, refs = sys.argv[1], set(sys.argv[2:])
    for g in sorted(nets(path), key=lambda g: sorted(g['pins'])):
        if not refs or any(p.split('-')[0] in refs for p in g['pins']):
            print(f"{'/'.join(sorted(g['names'])) or '(sin nombre)':22s} {sorted(g['pins'])}")
