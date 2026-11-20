"""Conectividad física de la PCB desde el ODB++: nets abiertas, cortos entre nets y holguras mínimas.

python tools/connectivity.py [exports/odb]
"""
import os
import sys
from collections import defaultdict

import networkx as nx
from shapely.strtree import STRtree

sys.path.insert(0, os.path.dirname(__file__))
from odb import Board, COPPER  # noqa: E402

DRILL = 'drill_plated_bottom_layer-top_layer'


def opens(b):
    """Nets cuyo cobre no une todos sus pines."""
    feats = defaultdict(list)
    for l in COPPER:
        for f in b.layers[l]:
            if f.net and f.net != '$NONE$' and f.pol == 'P' and f.kind in 'PLAS':
                feats[f.net].append((l, f))
    drills = defaultdict(list)
    for d in b.layers[DRILL]:
        if d.net and d.net != '$NONE$':
            drills[d.net].append(d)
    out = []
    for net, fs in feats.items():
        G = nx.Graph()
        by_l = defaultdict(list)
        for l, f in fs:
            G.add_node((l, f.idx))
            by_l[l].append(f)
        for l, lf in by_l.items():
            gs = [f.geom for f in lf]
            t = STRtree(gs)
            for i, g in enumerate(gs):
                for j in t.query(g):
                    j = int(j)
                    if j > i and g.buffer(0.002).intersects(gs[j]):
                        G.add_edge((l, lf[i].idx), (l, lf[j].idx))
        for d in drills.get(net, []):
            G.add_node(('d', d.idx))
            for l, lf in by_l.items():
                for f in lf:
                    if f.geom.intersects(d.geom):
                        G.add_edge(('d', d.idx), (l, f.idx))
        pin_nodes = defaultdict(set)
        for c in b.comps.values():
            for p in c.pins:
                if p.net == net:
                    for l, f in p.feats:
                        key = ('d', f.idx) if l == DRILL else (l, f.idx)
                        if key in G:
                            pin_nodes[p.ref].add(key)
        comp_of = {}
        for k, comp in enumerate(nx.connected_components(G)):
            for n in comp:
                comp_of[n] = k
        groups = defaultdict(list)
        for ref, nodes in pin_nodes.items():
            ks = {comp_of[n] for n in nodes}
            groups[min(ks)].append(ref)
        if len(groups) > 1:
            out.append((net, sorted(groups.values(), key=len, reverse=True)))
    return out


def shorts_and_clearance(b, warn=0.127):
    res = []
    mins = {}
    for l in COPPER:
        fs = [f for f in b.layers[l] if f.pol == 'P' and f.kind in 'PLAS' and f.net]
        gs = [f.geom for f in fs]
        t = STRtree(gs)
        best = (9, None)
        for i, g in enumerate(gs):
            for j in t.query(g.buffer(warn)):
                j = int(j)
                if j <= i or fs[i].net == fs[j].net:
                    continue
                d = g.distance(gs[j])
                if d < 1e-4:
                    c = g.intersection(gs[j]).centroid
                    res.append((l, fs[i].net, fs[j].net, round(c.x, 2), round(c.y, 2)))
                elif d < best[0]:
                    c = g.centroid
                    best = (d, f'{fs[i].net} / {fs[j].net} en ({c.x:.1f},{c.y:.1f})')
        mins[l] = best
    return res, mins


def main(odb_dir='exports/odb'):
    b = Board(odb_dir)
    print('=== Nets abiertas en la PCB (grupos de pines no unidos por cobre) ===')
    op = opens(b)
    for net, groups in op:
        print(f'  {net}: ' + ' | '.join(', '.join(g[:6]) + (' …' if len(g) > 6 else '') for g in groups))
    if not op:
        print('  ninguna')
    print('\n=== Cortos (cobre de nets distintas que se toca) y holgura mínima por capa ===')
    sh, mins = shorts_and_clearance(b)
    for s in sh[:30]:
        print('  CORTO', s)
    if not sh:
        print('  sin cortos')
    for l, (d, w) in mins.items():
        print(f'  {l:13s} holgura mínima entre nets {d:.3f} mm  {w}')


if __name__ == '__main__':
    main(*sys.argv[1:])
