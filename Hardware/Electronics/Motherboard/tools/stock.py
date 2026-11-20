"""Stock en JLCPCB (montaje) y LCSC de cada línea del BOM para N placas.

python tools/stock.py [N=50] [exports/IncuNest.xlsx]
Necesario = cantidad por placa × N + merma (5 % o mínimo 5, aproximación a la de JLC).
"""
import json
import os
import sys
import time
import urllib.request

sys.path.insert(0, os.path.dirname(__file__))
from bom import read_bom  # noqa: E402

JLC = 'https://jlcpcb.com/api/overseas-pcb-order/v1/shoppingCart/smtGood/selectSmtComponentList'
LCSC = 'https://wmsc.lcsc.com/ftps/wm/product/detail?productCode='
HDR = {'User-Agent': 'Mozilla/5.0', 'Content-Type': 'application/json'}


def jlc(code):
    req = urllib.request.Request(JLC, data=json.dumps({'keyword': code, 'currentPage': 1, 'pageSize': 10}).encode(),
                                 headers=HDR)
    d = json.load(urllib.request.urlopen(req, timeout=30))
    for c in (d.get('data') or {}).get('componentPageInfo', {}).get('list') or []:
        if c.get('componentCode') == code:
            return c
    return None


def lcsc(code):
    req = urllib.request.Request(LCSC + code, headers=HDR)
    return (json.load(urllib.request.urlopen(req, timeout=30)) or {}).get('result')


def main(n=50, path='exports/IncuNest.xlsx'):
    n = int(n)
    rows = []
    for r in read_bom(path):
        code = r['LCSC_PN'].strip()
        q = int(r['Quantity'])
        need = q * n + max(5, round(q * n * 0.05))
        j = jlc(code)
        time.sleep(0.3)
        try:
            l = lcsc(code)
        except Exception:  # noqa: BLE001
            l = None
        js = j['stockCount'] if j else None
        lib = j['componentLibraryType'] if j else '—'
        ls = l['stockNumber'] if l else None
        model = (l or {}).get('productModel') or (j or {}).get('componentModelEn', '')
        ok = js is not None and js >= need
        rows.append((ok, code, r['Designator'], q, need, js, lib, ls, model, r['Comment']))
        print(f"{'OK ' if ok else '!! '}{code:10s} x{q:<3d} necesita {need:5d} | JLC {js!s:>8} ({lib}) | LCSC {ls!s:>8} | {model} | {r['Designator'][:40]}",
              flush=True)
    return rows


if __name__ == '__main__':
    main(*sys.argv[1:])
