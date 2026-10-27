"""Análisis geométrico de la PCB a partir del ODB++ (ver "Método de análisis" en CLAUDE.md).

python tools/pcbcheck.py [exports/odb]

Secciones:
  1. Min-cut de rutas de potencia (ancho equivalente en mm de cobre de 1 oz)
  2. Kelvin de los shunts (solo pistas <= 0,45 mm)
  3. Etapa de potencia del BQ25730 (áreas SW, vías, distancias de sense)
  4. Retornos: pistas de top/bottom fuera del DGND de la capa adyacente
  5. Lazos y desacoplos (distancias pin a pin)
  6. Térmico, RF, antena y DFM
"""
import math
import os
import sys
from collections import defaultdict

import networkx as nx
from shapely.geometry import Point
from shapely.ops import unary_union
from shapely.strtree import STRtree

sys.path.insert(0, os.path.dirname(__file__))
from odb import Board, COPPER, CU_WEIGHT, read_features  # noqa: E402

DRILL = 'drill_plated_bottom_layer-top_layer'
INF = 1e9
VIA_EQ = 0.5          # mm equivalentes por vía
KELVIN_MAX_W = 0.45   # mm
ADJ = {'top_layer': 'ground_plane', 'bottom_layer': 'power_plane'}


def is_via(f):
    return f.kind == 'P' and '1=2' in f.attrs


class NetGraph:
    """Grafo de conectividad de una net con capacidades en nodos (pistas, vías) y en contactos pad-polígono."""

    def __init__(self, board, net):
        self.b, self.net = board, net
        self.G = nx.DiGraph()
        self.feats = {}
        per_layer = {}
        for l in COPPER:
            fs = [f for f in board.layers[l] if f.net == net and f.pol == 'P' and f.kind in 'PLAS']
            per_layer[l] = fs
            for f in fs:
                self._add(('c', l, f.idx), f, self._cap(l, f))
        drills = [f for f in board.layers[DRILL] if f.net == net and f.kind in 'PL']
        for f in drills:
            self._add(('d', f.idx), f, VIA_EQ if is_via(f) else INF)
        # contactos en la misma capa
        for l, fs in per_layer.items():
            geoms = [f.geom for f in fs]
            tree = STRtree(geoms)
            w = CU_WEIGHT[l]
            for i, f in enumerate(fs):
                for j in tree.query(geoms[i]):
                    j = int(j)
                    if j <= i:
                        continue
                    g = fs[j]
                    if not geoms[i].buffer(0.002).intersects(geoms[j]):
                        continue
                    cap = INF
                    kinds = {f.kind, g.kind}
                    if kinds == {'P', 'S'}:
                        pad, srf = (f, g) if f.kind == 'P' else (g, f)
                        # contacto pad-polígono: perímetro del pad dentro del cobre (alivios térmicos)
                        cap = pad.geom.boundary.intersection(srf.geom.buffer(0.01)).length * w
                        cap = max(cap, 1e-3)
                    self._link(('c', l, f.idx), ('c', l, g.idx), cap)
        # taladros -> cobre de cada capa
        for d in drills:
            hole = d.geom
            for l, fs in per_layer.items():
                for f in fs:
                    if f.geom.intersects(hole):
                        self._link(('d', d.idx), ('c', l, f.idx), INF)

    def _cap(self, l, f):
        if f.kind in 'LA':
            return f.width * CU_WEIGHT[l]
        return INF

    def _add(self, key, f, cap):
        self.feats[key] = f
        self.G.add_edge((key, 'i'), (key, 'o'), capacity=cap)

    def _link(self, a, b, cap):
        self.G.add_edge((a, 'o'), (b, 'i'), capacity=cap)
        self.G.add_edge((b, 'o'), (a, 'i'), capacity=cap)

    def pin_nodes(self, pins):
        out = []
        for p in pins:
            for (l, f) in p.feats:
                key = ('d', f.idx) if l == DRILL else ('c', l, f.idx)
                if key in self.feats:
                    out.append(key)
        return out

    def maxflow(self, src_pins, dst_pins):
        G = self.G.copy()
        s, t = self.pin_nodes(src_pins), self.pin_nodes(dst_pins)
        if not s or not t:
            return None, []
        for k in s:
            G.add_edge('S', (k, 'i'), capacity=INF)
        for k in t:
            G.add_edge((k, 'o'), 'T', capacity=INF)
        val, part = nx.minimum_cut(G, 'S', 'T')
        reach, _ = part
        cut = []
        for u in reach:
            if not isinstance(u, tuple):
                continue
            for v, d in G[u].items():
                if v in reach or d['capacity'] >= INF or v == 'T':
                    continue
                if u[1] == 'i' and v == (u[0], 'o'):
                    cut.append((u[0], self.feats[u[0]], d['capacity']))
                else:  # contacto pad-polígono
                    cut.append((('e',) + u[0], self.feats[u[0]], d['capacity']))
        return val, cut


def pins(b, spec):
    """'R59-1' o 'Q14-5-6-7-8' -> lista de Pin."""
    ref, name = spec.split('-', 1)
    c = b.comps[ref]
    names = [name] if name in {p.name for p in c.pins} else name.split(',')
    out = [p for p in c.pins if p.name in names]
    if not out:
        raise KeyError(spec)
    return out


def describe_cut(cut):
    parts = []
    for key, f, cap in cut:
        if key[0] == 'e':
            parts.append(f'{key[2][:3]} contacto pad-polígono {cap:.2f}mm@({f.geom.centroid.x:.1f},{f.geom.centroid.y:.1f})')
        elif key[0] == 'd':
            parts.append(f'vía@({f.data[0]:.1f},{f.data[1]:.1f})' if is_via(f) else 'TH')
        elif f.kind in 'LA':
            parts.append(f'{key[1][:3]} pista {f.width:.2f}mm@({f.geom.centroid.x:.1f},{f.geom.centroid.y:.1f})')
        elif f.kind in 'PS':
            parts.append(f'{key[1][:3]} contacto pad-polígono@({f.geom.centroid.x:.1f},{f.geom.centroid.y:.1f})')
    agg = defaultdict(int)
    for p in parts:
        agg[p.split('@')[0] if p.startswith('vía') else p] += 1
    return ', '.join(f'{n}× {k}' if n > 1 else k for k, n in agg.items())


POWER_PATHS = [
    # (descripción, net, origen, destino, corriente objetivo A)
    ('J11 -> Q7 (entrada 24 V)', 'NetC43_2', ['J11-1'], ['Q7-1,2,3'], 8),
    ('J8 -> Q14 (mechero)', 'NetC103_2', ['J8-1'], ['Q14-1,2,3'], 8),
    ('Q7/Q14 -> R48', 'V_IN_PROTECTED', ['Q7-5,6,7,8,9', 'Q14-5-6-7-8'], ['R48-2'], 8),
    ('R48 -> Q9', 'NetC27_2', ['R48-1'], ['Q9-5-6-7-8'], 8),
    ('Q10 -> R49 (V_OUT)', 'Vsense+', ['Q10-5-6-7-8'], ['R49-2'], 10),
    ('Q13 <-> V_OUT', 'Vsense+', ['Q13-1,2,3'], ['R49-2'], 10),
    ('R49 -> Q1 (V_SYS)', 'V_SYS', ['R49-1'], ['Q1-3'], 10),
    ('Q13 -> R59 (BAT)', 'NetC56_2', ['Q13-5,6,7,8,9'], ['R59-2'], 10),
    ('J9 -> R59 (BAT)', 'NetC58_1', ['J9-2'], ['R59-1'], 10),
    ('Q1 -> J3-2 (DC_ACT calefactor)', 'DC_ACTUATORS', ['Q1-2'], ['J3-2'], 8),
    ('Q1 -> R17 (fototerapia)', 'DC_ACTUATORS', ['Q1-2'], ['R17-1'], 4),
    ('R17 -> J4-2', 'PHOTOTHERAPY+', ['R17-2'], ['J4-2'], 4),
    ('J4-1 -> Q5', 'PHOTOTHERAPY-', ['J4-1'], ['Q5-4'], 4),
    ('J3-1 -> R46', 'HEATER', ['J3-1'], ['R46-1'], 8),
    ('R46 -> Q2', 'NetQ2_4', ['R46-2'], ['Q2-4'], 8),
]


def run_paths(b):
    print('=== 1. Min-cut de rutas de potencia (mm equivalentes de 1 oz; vía = 0,5) ===')
    print('     objetivo: >=3,5 mm a 8 A, >=4,7 mm a 10 A')
    graphs = {}
    res = []
    for desc, net, src, dst, amps in POWER_PATHS:
        g = graphs.setdefault(net, NetGraph(b, net))
        sp = [p for s in src for p in pins(b, s)]
        dp = [p for s in dst for p in pins(b, s)]
        val, cut = g.maxflow(sp, dp)
        need = 3.5 if amps <= 8 else 4.7
        if amps < 8:
            need = 3.5 * amps / 8
        ok = val is not None and val >= need
        if val is None:
            print(f'  ?? {desc:34s} {net:16s} sin pads de origen/destino en la net (src={[p.ref for p in sp]}, dst={[p.ref for p in dp]})')
            continue
        print(f'  {"OK " if ok else "!! "}{desc:34s} {net:16s} {round(val, 2):>6} mm'
              f'  (obj {need:.1f})  cuello: {describe_cut(cut)}')
        res.append((desc, net, val, need, cut))
    return res


# ---------------------------------------------------------------- Kelvin
KELVIN = [
    # shunt, (pin sense, pad shunt), ...
    ('R48', [('R53-2', 'R48-2'), ('R54-2', 'R48-1')]),
    ('R59', [('R61-2', 'R59-2'), ('R62-2', 'R59-1')]),
    ('R49', [('U7-12', 'R49-2'), ('U7-11', 'R49-1')]),
    ('R17', [('U7-15', 'R17-1'), ('U7-14', 'R17-2')]),
    ('R13', [('U7-2', 'R13-2'), ('U7-1', 'R13-1')]),
    ('R46', [('U8-12', 'R46-1'), ('U8-11', 'R46-2')]),
    ('R19', [('U8-15', 'R19-1'), ('U8-14', 'R19-2')]),
]


def kelvin_ok(b, sense_spec, pad_spec):
    sp, pp = pins(b, sense_spec)[0], pins(b, pad_spec)[0]
    net = sp.net
    if pp.net != net:
        return f'net distinta ({net} / {pp.net})'
    allowed = {}
    sense_keys = {(l, f.idx) for l, f in sp.feats}
    pad_keys = {(l, f.idx) for l, f in pp.feats}
    G = nx.Graph()
    for l in COPPER:
        for f in b.layers[l]:
            if f.net != net or f.pol != 'P':
                continue
            k = (l, f.idx)
            if f.kind in 'LA' and f.width <= KELVIN_MAX_W:
                allowed[k] = f
            elif k in sense_keys or k in pad_keys:
                allowed[k] = f
            elif is_via_pad(b, l, f):
                allowed[k] = f
    for d in b.layers[DRILL]:
        if d.net == net and is_via(d):
            allowed[(DRILL, d.idx)] = d
    keys = list(allowed)
    for k in keys:
        G.add_node(k)
    by_layer = defaultdict(list)
    for k in keys:
        by_layer[k[0]].append(k)
    for l, ks in by_layer.items():
        if l == DRILL:
            continue
        geoms = [allowed[k].geom for k in ks]
        tree = STRtree(geoms)
        for i, k in enumerate(ks):
            for j in tree.query(geoms[i]):
                j = int(j)
                if j > i and geoms[i].buffer(0.002).intersects(geoms[j]):
                    G.add_edge(k, ks[j])
    for k in by_layer.get(DRILL, []):
        hole = allowed[k].geom
        for l, ks in by_layer.items():
            if l == DRILL:
                continue
            for k2 in ks:
                if allowed[k2].geom.intersects(hole):
                    G.add_edge(k, k2)
    s = [(l, f.idx) for l, f in sp.feats if (l, f.idx) in allowed]
    t = [(l, f.idx) for l, f in pp.feats if (l, f.idx) in allowed]
    for a in s:
        for c in t:
            if a in G and c in G and nx.has_path(G, a, c):
                return 'OK'
    # diagnóstico: componente fino alcanzable desde el pin de sense y qué cobre no permitido toca
    comp = set()
    for a in s:
        if a in G:
            comp |= nx.node_connected_component(G, a)
    thin = unary_union([allowed[k].geom for k in comp if k[0] != DRILL and k not in sense_keys] or [Point(sp.x, sp.y)])
    padg = unary_union([f.geom for l, f in pp.feats if l in COPPER])
    touch = []
    for l in COPPER:
        for f in b.layers[l]:
            if f.net == net and f.pol == 'P' and (l, f.idx) not in allowed and f.geom.intersects(
                    unary_union([allowed[k].geom for k in comp if k[0] == l] or [Point(0, 0)]).buffer(0.002)):
                c = f.geom.centroid
                what = f'pista {f.width:.2f}mm' if f.kind in 'LA' else ('polígono' if f.kind == 'S' else 'pad')
                touch.append(f'{what} {l[:3]} ({c.x:.1f},{c.y:.1f})')
    return f'NO: la pista fina se une a {", ".join(sorted(set(touch))[:3]) or "nada"}; distancia de la parte fina al pad del shunt {thin.distance(padg):.2f} mm'


_via_pads = {}


def is_via_pad(b, layer, f):
    """Pad de vía en una capa de cobre: P redondo en la misma posición que un taladro de vía."""
    if f.kind != 'P':
        return False
    if not _via_pads:
        for d in b.layers[DRILL]:
            if is_via(d):
                _via_pads[(round(d.data[0], 3), round(d.data[1], 3))] = d
    return (round(f.data[0], 3), round(f.data[1], 3)) in _via_pads and f.sym.startswith('r')


def run_kelvin(b):
    print('\n=== 2. Kelvin (pin de sense -> pad del shunt solo por pistas <= 0,45 mm y vías) ===')
    for shunt, pairs in KELVIN:
        out = [f'{s}->{p}: {kelvin_ok(b, s, p)}' for s, p in pairs]
        print(f'  {shunt}: ' + ' | '.join(out))


# ---------------------------------------------------------------- etapa de potencia
def net_area(b, net, layer):
    fs = [f.geom for f in b.layers[layer] if f.net == net and f.pol == 'P' and f.kind in 'PLAS']
    return unary_union(fs) if fs else None


def vias_of(b, net):
    return [d for d in b.layers[DRILL] if d.net == net and is_via(d)]


def min_dist(b, nets_a, nets_b, layers=COPPER, same_layer=True):
    best = (INF, None)
    for la in layers:
        ga = [f for f in b.layers[la] if f.net in nets_a and f.pol == 'P' and f.kind in 'PLAS']
        if not ga:
            continue
        lbs = [la] if same_layer else layers
        for lb in lbs:
            gb = [f for f in b.layers[lb] if f.net in nets_b and f.pol == 'P' and f.kind in 'PLAS']
            if not gb:
                continue
            tb = STRtree([f.geom for f in gb])
            for f in ga:
                j = tb.nearest(f.geom)
                d = f.geom.distance(gb[int(j)].geom)
                if d < best[0]:
                    p = f.geom.centroid
                    best = (d, f'{f.net}({la[:3]}) <-> {gb[int(j)].net}({lb[:3]}) cerca de ({p.x:.1f},{p.y:.1f})')
    return best


def run_powerstage(b):
    print('\n=== 3. Etapa de potencia BQ25730 ===')
    for ref in ['Q9', 'Q12', 'L1', 'Q11', 'Q10', 'C44', 'C45', 'C46', 'C47', 'C27', 'C28', 'C37', 'C51', 'C29', 'U11']:
        c = b.comps[ref]
        print(f'  {ref:4s} ({c.x:6.2f}; {c.y:6.2f}) rot {c.rot:.0f} cara {c.side}')
    for net in ['SW1', 'SW2']:
        tot = 0
        det = []
        for l in COPPER:
            a = net_area(b, net, l)
            if a is not None and a.area > 0:
                tot += a.area
                det.append(f'{l[:3]} {a.area:.1f}')
        print(f'  Área {net}: {tot:.1f} mm² ({", ".join(det)}); vías en {net}: {len(vias_of(b, net))}')
    # vías a GND bajo los source de Q12 y Q11 (pines 1-2-3) y de Q9/Q10 no aplican
    gnd_v = vias_of(b, 'DGND')
    for ref in ['Q12', 'Q11']:
        c = b.comps[ref]
        src = unary_union([f.geom for p in c.pins if set(p.name.split('-')) & {'1', '2', '3'} for l, f in p.feats if l in COPPER])
        body = unary_union([f.geom for p in c.pins for l, f in p.feats if l in COPPER]).convex_hull
        n_src = sum(1 for d in gnd_v if src.buffer(0.6).contains(Point(d.data[0], d.data[1])))
        n_body = sum(1 for d in gnd_v if body.buffer(1.0).contains(Point(d.data[0], d.data[1])))
        print(f'  Vías DGND junto a los source de {ref}: {n_src} (a <=0,6 mm de los pads); bajo/alrededor del encapsulado: {n_body}')
    for sense in [('ACP', 'ACN'), ('NetC57_1', 'NetC57_2')]:
        for sw in ['SW1', 'SW2']:
            d, where = min_dist(b, set(sense), {sw})
            d2, where2 = min_dist(b, set(sense), {sw}, same_layer=False)
            print(f'  {"/".join(sense):18s} a {sw}: misma capa {d:.2f} mm [{where}] | cualquier capa {d2:.2f} mm')
    # trazas de sense sobre la net de potencia del shunt (R48: V_IN_PROTECTED/NetC27_2 finas)
    for net in ['V_IN_PROTECTED', 'NetC27_2']:
        thin = [f for l in COPPER for f in b.layers[l] if f.net == net and f.kind in 'LA' and f.width <= KELVIN_MAX_W]
        if thin:
            g = unary_union([f.geom for f in thin])
            dmin = min(g.distance(net_area(b, sw, l)) for sw in ['SW1', 'SW2'] for l in COPPER
                       if net_area(b, sw, l) is not None)
            print(f'  Pistas finas de {net} (sense R48): distancia mínima a SW1/SW2 {dmin:.2f} mm')


# ---------------------------------------------------------------- retornos
def run_returns(b, nets=None):
    print('\n=== 4. Retornos: longitud de pista de top/bottom fuera del DGND de la capa adyacente (mm) ===')
    res = defaultdict(float)
    tot = defaultdict(float)
    for l, adj in ADJ.items():
        gnd = net_area(b, 'DGND', adj)
        for f in b.layers[l]:
            if f.kind not in 'LA' or f.net in (None, 'DGND'):
                continue
            cl = f.centerline()
            tot[(f.net, l)] += cl.length
            res[(f.net, l)] += cl.difference(gnd).length
    rows = sorted(((v, k) for k, v in res.items() if v > 0.5), reverse=True)
    for v, (n, l) in rows[:40]:
        print(f'  {n:20s} {l[:3]}: {v:6.1f} de {tot[(n, l)]:6.1f}')
    return res


def run_region_tracks(b):
    print('\n=== 4b. Pistas en power_plane (L3) bajo zonas sensibles ===')
    zones = {
        'Termistor (J7/R44/R45/U9/U3/C23)': ['J7', 'R44', 'R45', 'U9', 'U3', 'C23', 'C99', 'R83'],
        'ESP32 (U1)': ['U1'],
        'AFE (U4/Y1)': ['U4', 'Y1'],
    }
    for name, refs in zones.items():
        hull = unary_union([f.geom for r in refs for p in b.comps[r].pins for l, f in p.feats if l in COPPER])
        hull = hull.convex_hull.buffer(0.5)
        for l in ['power_plane', 'ground_plane']:
            fs = [f for f in b.layers[l] if f.kind in 'LA' and f.geom.intersects(hull)]
            nets = defaultdict(float)
            for f in fs:
                nets[f.net] += f.centerline().intersection(hull).length
            if nets:
                print(f'  {name} {l}: ' + ', '.join(f'{n} {v:.1f}mm' for n, v in sorted(nets.items(), key=lambda x: -x[1])))
            else:
                print(f'  {name} {l}: sin pistas')


# ---------------------------------------------------------------- lazos y desacoplos
def pin_dist(b, a, c):
    pa, pc = pins(b, a), pins(b, c)
    ga = unary_union([f.geom for p in pa for l, f in p.feats if l in COPPER])
    gc = unary_union([f.geom for p in pc for l, f in p.feats if l in COPPER])
    return ga.distance(gc)


DECOUPLING = [
    # (descripción, pin de alimentación del IC, pin del condensador)
    ('U11 REGN - C55', 'U11-28', 'C55-2'),
    ('U11 VDDA - C61', 'U11-7', 'C61-2'),
    ('U11 VBUS - C108', 'U11-1', 'C108-1'),
    ('U11 BTST1 - C38', 'U11-30', 'C38-2'),
    ('U11 BTST2 - C39', 'U11-25', 'C39-2'),
    ('U11 SRP/SRN - C57', 'U11-20', 'C57-2'),
    ('Q9 drain - C44', 'Q9-5-6-7-8', 'C44-1'),
    ('Q9 drain - C47', 'Q9-5-6-7-8', 'C47-1'),
    ('Q12 source - C44 GND', 'Q12-1-2-3', 'C44-2'),
    ('Q10 drain - C37', 'Q10-5-6-7-8', 'C37-1'),
    ('Q11 source - C37 GND', 'Q11-1-2-3', 'C37-2'),
    ('Q11 source - C51 GND', 'Q11-1-2-3', 'C51-2'),
    ('U12 VIN - C74', 'U12-1', 'C74-2'),
    ('U12 GND - C74', 'U12-3', 'C74-1'),
    ('U13 VIN - C84', 'U13-1', 'C84-2'),
    ('U14 VIN - C93', 'U14-1', 'C93-2'),
    ('U1 3V3 - C3', 'U1-2', 'C3-2'),
    ('U1 3V3 - C2', 'U1-2', 'C2-1'),
    ('U1 EN - C1', 'U1-3', 'C1-2'),
    ('U4 RX_ANA_SUP(33) - C96', 'U4-33', 'C96-2'),
    ('U4 RX_ANA_SUP(39) - C96', 'U4-39', 'C96-2'),
    ('U4 RX_DIG_SUP - C11', 'U4-31', 'C11-2'),
    ('U4 LED_DRV_SUP - C13', 'U4-17', 'C13-2'),
    ('U4 TX_CTRL_SUP - C12', 'U4-11', 'C12-2'),
    ('U4 BG - C8', 'U4-7', 'C8-2'),
    ('U4 TX_REF - C9', 'U4-9', 'C9-2'),
    ('U18 VDD - C106', 'U18-6', 'C106-1'),
    ('U18 VDD - C104', 'U18-6', 'C104-1'),
    ('U7 VS - C24', 'U7-4', 'C24-2'),
    ('U8 VS - C25', 'U8-4', 'C25-2'),
    ('U9 VDD - C26', 'U9-5', 'C26-2'),
    ('U5 VBAT - C105', 'U5-34', 'C105-1'),
    ('U5 VBAT - C14', 'U5-34', 'C14-2'),
    ('U2 IN - C?', 'U2-1', 'R19-2'),
    ('U10 VCAP - C43', 'U10-1', 'C43-1'),
    ('U17 VCAP - C103', 'U17-1', 'C103-1'),
    ('Q2 gate - R16', 'Q2-1', 'R16-1'),
    ('Q5 gate - R29', 'Q5-1', 'R29-1'),
    ('U18 OUTB - R16', 'U18-5', 'R16-2'),
    ('U18 OUTA - R29', 'U18-7', 'R29-2'),
]


def run_decoupling(b):
    print('\n=== 5. Distancias pin a pin (borde de pad a borde de pad, mm) ===')
    for desc, a, c in DECOUPLING:
        try:
            print(f'  {desc:30s} {pin_dist(b, a, c):6.2f}')
        except Exception as e:  # noqa: BLE001
            print(f'  {desc:30s} error {e}')


# ---------------------------------------------------------------- térmico / RF / DFM
def run_thermal(b):
    print('\n=== 6a. Térmico (cobre del drain/pestaña y vías de la misma net) ===')
    for ref, net in [('Q13', 'NetC56_2'), ('Q2', 'NetQ2_4'), ('Q5', 'PHOTOTHERAPY-'), ('Q1', 'DC_ACTUATORS'),
                     ('Q7', 'V_IN_PROTECTED'), ('Q14', 'V_IN_PROTECTED')]:
        c = b.comps[ref]
        tab = [f for p in c.pins if p.net == net for l, f in p.feats if l in COPPER]
        tabg = unary_union([f.geom for f in tab])
        side_layer = 'top_layer' if c.side == 'T' else 'bottom_layer'
        other = 'bottom_layer' if c.side == 'T' else 'top_layer'
        out = []
        for l in (side_layer, other):
            a = net_area(b, net, l)
            if a is None:
                out.append(f'{l[:3]} 0')
                continue
            # cobre conectado a la pestaña en esa capa (componente conexo)
            geoms = getattr(a, 'geoms', [a])
            conn = [g for g in geoms if g.intersects(tabg.buffer(0.05))] if l == side_layer else geoms
            out.append(f'{l[:3]} {sum(g.area for g in conn):.0f} mm²')
        vias = [d for d in vias_of(b, net) if tabg.buffer(1.5).contains(Point(d.data[0], d.data[1]))]
        print(f'  {ref} ({net}, cara {c.side}): cobre ' + ', '.join(out) + f'; vías bajo/junto a la pestaña: {len(vias)}')


def run_rf_dfm(b, odb_dir):
    print('\n=== 6b. RF, antena del ESP32, taladros y DFM ===')
    # pista RF del SIM800
    for net in ['NetC17_2']:
        for l in COPPER:
            fs = [f for f in b.layers[l] if f.net == net and f.kind in 'LA']
            if fs:
                ws = sorted({round(f.width, 3) for f in fs})
                gnd = net_area(b, 'DGND', l)
                g = unary_union([f.geom for f in fs])
                print(f'  RF {net} en {l}: anchos {ws} mm, long {sum(f.length for f in fs):.1f} mm, '
                      f'gap mínimo a DGND {g.distance(gnd):.3f} mm')
    # antena ESP32: zona frente a la fila de pines 15-26
    u1 = b.comps['U1']
    p1, p15, p26, p27 = (pins(b, f'U1-{n}')[0] for n in (1, 15, 26, 27))
    print(f'  U1 pin1 ({p1.x:.1f},{p1.y:.1f}) pin15 ({p15.x:.1f},{p15.y:.1f}) pin26 ({p26.x:.1f},{p26.y:.1f}) pin27 ({p27.x:.1f},{p27.y:.1f})')
    # orientación: el eje pin1->pin15 apunta hacia el lado sin antena; la antena está en el extremo opuesto
    vx, vy = p15.x - p1.x, p15.y - p1.y
    n = math.hypot(vx, vy)
    ux, uy = vx / n, vy / n
    cx = (p1.x + p27.x) / 2
    cy = (p1.y + p27.y) / 2
    # extremo de pines 1/40 está ~1 mm del borde de la zona de antena; la antena ocupa ~6,5 mm más allá
    from shapely.geometry import Polygon as Poly
    px, py = -uy, ux
    half_w = math.hypot(p27.x - p1.x, p27.y - p1.y) / 2 + 1.5
    a0 = (cx - ux * 1.0, cy - uy * 1.0)
    a1 = (cx - ux * 7.5, cy - uy * 7.5)
    ant = Poly([(a0[0] + px * half_w, a0[1] + py * half_w), (a0[0] - px * half_w, a0[1] - py * half_w),
                (a1[0] - px * half_w, a1[1] - py * half_w), (a1[0] + px * half_w, a1[1] + py * half_w)])
    print(f'  Zona de antena estimada: {[tuple(round(v, 1) for v in c) for c in ant.exterior.coords[:4]]}; '
          f'fuera de placa: {ant.difference(b.profile).area:.1f} mm² de {ant.area:.1f}')
    for l in COPPER:
        a = unary_union([f.geom for f in b.layers[l] if f.pol == 'P' and f.kind in 'PLAS']).intersection(ant)
        print(f'    cobre en zona de antena {l}: {a.area:.1f} mm²')
    # taladros
    vias = [d for d in b.layers[DRILL] if is_via(d)]
    sizes = defaultdict(int)
    for d in vias:
        sizes[round(d.width, 2)] += 1
    print(f'  Vías por diámetro de taladro: {dict(sizes)}')
    big = [d for d in b.layers[DRILL] if d.kind == 'P' and d.width >= 3.0]
    rf = b.comps.get('RF_PCB1')
    for d in big:
        dist = math.hypot(d.data[0] - rf.x, d.data[1] - rf.y) if rf else 0
        print(f'  Taladro {d.width:.2f} mm en ({d.data[0]:.1f},{d.data[1]:.1f}) net {d.net}; a {dist:.1f} mm de RF_PCB1')
    # máscara: puente mínimo entre aperturas en U11 (BQ25730, 0,4 mm)
    step = os.path.join(odb_dir, 'steps', 'pcb', 'layers')
    for comp, layer in [('U11', 'bottom_solder'), ('U4', 'top_solder'), ('U7', 'bottom_solder')]:
        fs = read_features(os.path.join(step, layer, 'features'))
        c = b.comps[comp]
        region = unary_union([f.geom for p in c.pins for l, f in p.feats if l in COPPER]).convex_hull.buffer(0.3)
        ops = [f.geom for f in fs if f.kind == 'P' and f.geom.intersects(region)]
        merged = unary_union(ops)
        nparts = len(getattr(merged, 'geoms', [merged]))
        dmin = INF
        for i in range(len(ops)):
            for j in range(i + 1, len(ops)):
                d = ops[i].distance(ops[j])
                if 0 < d < dmin:
                    dmin = d
        print(f'  Máscara {comp}: {len(ops)} aperturas, {nparts} regiones tras unir (si < aperturas hay '
              f'aperturas solapadas = sin puente), puente mínimo {dmin if dmin < INF else 0:.3f} mm')
    for side in ['top', 'bottom']:
        silk = read_features(os.path.join(step, f'{side}_overlay', 'features'))
        mask = read_features(os.path.join(step, f'{side}_solder', 'features'))
        sg = [f.geom for f in silk if f.kind in 'LAPS']
        mg = unary_union([f.geom for f in mask if f.kind in 'PS'])
        viol = [g for g in sg if g.distance(mg) < 0.15]
        over = sum(g.intersection(mg).area for g in sg)
        print(f'  Serigrafía {side}: {len(viol)} elementos a <0,15 mm de una apertura de máscara '
              f'(área de serigrafía sobre aperturas {over:.2f} mm², la recorta el fabricante)')


def main(odb_dir='exports/odb'):
    b = Board(odb_dir)
    run_paths(b)
    run_kelvin(b)
    run_powerstage(b)
    run_returns(b)
    run_region_tracks(b)
    run_decoupling(b)
    run_thermal(b)
    run_rf_dfm(b, odb_dir)


if __name__ == '__main__':
    main(*sys.argv[1:])
