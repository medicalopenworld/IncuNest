"""Búsqueda en la librería de JLCPCB ordenada por stock.

python tools/jlcsearch.py "MMSZ5231B" [n]
"""
import json
import sys
import urllib.request

URL = 'https://jlcpcb.com/api/overseas-pcb-order/v1/shoppingCart/smtGood/selectSmtComponentList'


def search(kw, n=15):
    req = urllib.request.Request(URL, data=json.dumps({'keyword': kw, 'currentPage': 1, 'pageSize': 50}).encode(),
                                 headers={'User-Agent': 'Mozilla/5.0', 'Content-Type': 'application/json'})
    d = json.load(urllib.request.urlopen(req, timeout=30))
    lst = (d.get('data') or {}).get('componentPageInfo', {}).get('list') or []
    lst.sort(key=lambda c: -(c.get('stockCount') or 0))
    return lst[:n]


if __name__ == '__main__':
    for c in search(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 15):
        print(f"{c['componentCode']:10s} {c.get('stockCount', 0):>8} {c.get('componentLibraryType', ''):7s} "
              f"{c.get('componentModelEn', '')[:28]:28s} {c.get('componentSpecificationEn', '')[:10]:10s} {c.get('describe', '')[:90]}")
