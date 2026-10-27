"""Comprobación de nets: fusiones/divisiones respecto a la exportación anterior y sincronía esquema <-> PCB.

python tools/netcheck.py [exports] [exports/prev]
  - exports/IncuNest.NET       netlist del esquema (actual)
  - exports/prev/IncuNest.NET  netlist del esquema (anterior)
  - exports/odb                ODB++ (nets de la PCB)
"""
import os
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(__file__))
from odb import Board, read_protel_net, pin_to_net  # noqa: E402

GND = 'DGND'
# pines que nunca deben estar en GND
MUST_NOT_GND = ['J11-1', 'J8-1', 'J9-1']


def partition(nets):
    return {n: frozenset(p) for n, p in nets.items()}


def compare(old, new, label_old='prev', label_new='actual'):
    """Compara dos particiones de pines {net: set(pins)} ignorando nombres."""
    o_p2n, n_p2n = pin_to_net(old), pin_to_net(new)
    common = set(o_p2n) & set(n_p2n)
    merges, splits = [], []
    # fusiones: una net nueva contiene pines de >1 nets antiguas
    for nn, pins in new.items():
        olds = defaultdict(set)
        for p in pins:
            if p in o_p2n:
                olds[o_p2n[p]].add(p)
        if len(olds) > 1:
            merges.append((nn, {k: sorted(v) for k, v in olds.items()}))
    for on, pins in old.items():
        news = defaultdict(set)
        for p in pins:
            if p in n_p2n:
                news[n_p2n[p]].add(p)
        if len(news) > 1:
            splits.append((on, {k: sorted(v) for k, v in news.items()}))
    to_gnd = sorted(p for p in common if n_p2n[p] == GND and o_p2n[p] != GND)
    from_gnd = sorted(p for p in common if o_p2n[p] == GND and n_p2n[p] != GND)
    added = sorted(set(n_p2n) - set(o_p2n))
    removed = sorted(set(o_p2n) - set(n_p2n))
    return merges, splits, to_gnd, from_gnd, added, removed


def fmt_groups(d, maxn=8):
    out = []
    for k, v in d.items():
        s = ', '.join(v[:maxn]) + (' …' if len(v) > maxn else '')
        out.append(f'      {k}: {s}')
    return '\n'.join(out)


def report(exp='exports', prev=None):
    prev = prev or os.path.join(exp, 'prev')
    _, sch = read_protel_net(os.path.join(exp, 'IncuNest.NET'))
    sch_p2n = pin_to_net(sch)
    b = Board(os.path.join(exp, 'odb'))
    pcb = defaultdict(set)
    for c in b.comps.values():
        for p in c.pins:
            if p.net and p.net != '$NONE$':
                pcb[p.net].add(f'{c.ref}-{p.name}')
    pcb_p2n = pin_to_net(pcb)

    print('=== 1. Pines críticos fuera de GND ===')
    for p in MUST_NOT_GND:
        print(f'  {p:8s} esquema={sch_p2n.get(p, "—"):20s} pcb={pcb_p2n.get(p, "—")}')

    prev_net = os.path.join(prev, 'IncuNest.NET')
    if os.path.exists(prev_net):
        _, old = read_protel_net(prev_net)
        merges, splits, to_gnd, from_gnd, added, removed = compare(old, sch)
        print(f'\n=== 2. Esquema: prev -> actual ({prev_net}) ===')
        print(f'  Pines que pasan a {GND}: {to_gnd or "ninguno"}')
        print(f'  Pines que salen de {GND}: {from_gnd or "ninguno"}')
        print(f'  Fusiones de nets ({len(merges)}):')
        for nn, g in merges:
            print(f'    -> {nn}\n{fmt_groups(g)}')
        print(f'  Divisiones de nets ({len(splits)}):')
        for on, g in splits:
            print(f'    {on} ->\n{fmt_groups(g)}')
        print(f'  Pines nuevos ({len(added)}): {", ".join(added)}')
        print(f'  Pines eliminados ({len(removed)}): {", ".join(removed)}')

    print('\n=== 3. Sincronía esquema -> PCB ===')
    merges, splits, to_gnd, from_gnd, added, removed = compare(sch, pcb)
    print(f'  Pines solo en PCB: {added or "ninguno"}')
    print(f'  Pines solo en esquema (sin pad o sin net en PCB): {removed or "ninguno"}')
    print(f'  Nets fusionadas en PCB: {len(merges)}; divididas: {len(splits)}')
    for nn, g in merges:
        print(f'    -> {nn}\n{fmt_groups(g)}')
    for on, g in splits:
        print(f'    {on} ->\n{fmt_groups(g)}')
    name_diff = sorted({(sch_p2n[p], pcb_p2n[p]) for p in set(sch_p2n) & set(pcb_p2n)
                        if sch_p2n[p] != pcb_p2n[p]})
    print(f'  Nets con nombre distinto: {name_diff or "ninguna"}')
    sch_refs = {p.split('-')[0] for p in sch_p2n}
    pcb_refs = set(b.comps)
    print(f'  Componentes solo en PCB: {sorted(pcb_refs - sch_refs)}')
    print(f'  Componentes solo en esquema: {sorted(sch_refs - pcb_refs)}')

    print('\n=== 4. Nets de un solo pin (esquema) ===')
    singles = sorted((n, sorted(p)) for n, p in sch.items() if len(p) == 1)
    for n, p in singles:
        print(f'  {n}: {p}')
    return b, sch


if __name__ == '__main__':
    report(*sys.argv[1:])
