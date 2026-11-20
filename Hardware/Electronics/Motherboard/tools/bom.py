"""Lector del BOM XLSX de Altium (sin openpyxl: el estilo de Altium no es estándar).

python tools/bom.py [exports/IncuNest.xlsx]  -> imprime filas
Uso como librería: rows = read_bom(path) -> lista de dicts por fila (cabecera = primera fila con 'Designator').
"""
import re
import sys
import zipfile
import xml.etree.ElementTree as ET

NS = {'m': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}


def _col(ref):
    s = re.match(r'[A-Z]+', ref).group(0)
    n = 0
    for ch in s:
        n = n * 26 + ord(ch) - 64
    return n - 1


def read_sheet(path):
    z = zipfile.ZipFile(path)
    ss = []
    if 'xl/sharedStrings.xml' in z.namelist():
        for si in ET.fromstring(z.read('xl/sharedStrings.xml')).findall('m:si', NS):
            ss.append(''.join(t.text or '' for t in si.iter('{%s}t' % NS['m'])))
    sheet = sorted(n for n in z.namelist() if n.startswith('xl/worksheets/sheet'))[0]
    rows = []
    for r in ET.fromstring(z.read(sheet)).iter('{%s}row' % NS['m']):
        row = {}
        for c in r.findall('m:c', NS):
            v = c.find('m:v', NS)
            t = c.get('t')
            if t == 'inlineStr':
                val = ''.join(x.text or '' for x in c.iter('{%s}t' % NS['m']))
            elif v is None:
                val = ''
            elif t == 's':
                val = ss[int(v.text)]
            else:
                val = v.text
            row[_col(c.get('r'))] = val
        if row:
            rows.append([row.get(i, '') for i in range(max(row) + 1)])
    return rows


def read_bom(path):
    rows = read_sheet(path)
    hi = next(i for i, r in enumerate(rows) if any('Designator' in str(x) for x in r))
    hdr = [str(h).strip() for h in rows[hi]]
    out = []
    for r in rows[hi + 1:]:
        d = {hdr[i]: (r[i] if i < len(r) else '') for i in range(len(hdr)) if hdr[i]}
        if any(str(v).strip() for v in d.values()):
            out.append(d)
    return out


if __name__ == '__main__':
    for r in read_bom(sys.argv[1] if len(sys.argv) > 1 else 'exports/IncuNest.xlsx'):
        print(r)
