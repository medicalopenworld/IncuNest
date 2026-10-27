"""Parser ODB++ (exportación de Altium) y netlist Protel para la revisión de la Motherboard.

Uso como librería:
    from odb import Board, read_protel_net
    b = Board('exports/odb')           # nets, componentes, features por capa
    sch = read_protel_net('exports/IncuNest.NET')

Unidades: coordenadas en mm; los símbolos de ODB++ vienen en µm y se pasan a mm.
"""
import math
import os
import re
from collections import defaultdict

from shapely.geometry import LineString, Point, Polygon, box
from shapely.ops import unary_union
from shapely import affinity

COPPER = ['top_layer', 'ground_plane', 'power_plane', 'bottom_layer']
# espesor relativo de cobre (top/bottom 1 oz, internas 0,5 oz)
CU_WEIGHT = {'top_layer': 1.0, 'ground_plane': 0.5, 'power_plane': 0.5, 'bottom_layer': 1.0}
ARC_SEG = 24


def _f(s):
    return float(s)


# ---------------------------------------------------------------- símbolos
def symbol_shape(sym):
    """Devuelve (kind, params) en mm a partir del nombre de símbolo ODB++."""
    m = re.fullmatch(r'r([\d.]+)', sym)
    if m:
        return 'round', (float(m.group(1)) / 1000,)
    m = re.fullmatch(r's([\d.]+)', sym)
    if m:
        w = float(m.group(1)) / 1000
        return 'rect', (w, w, 0)
    m = re.fullmatch(r'rect([\d.]+)x([\d.]+)(?:xr([\d.]+))?', sym)
    if m:
        return 'rect', (float(m.group(1)) / 1000, float(m.group(2)) / 1000,
                        float(m.group(3) or 0) / 1000)
    m = re.fullmatch(r'oval([\d.]+)x([\d.]+)', sym)
    if m:
        return 'oval', (float(m.group(1)) / 1000, float(m.group(2)) / 1000)
    raise ValueError('símbolo no soportado: ' + sym)


def symbol_width(sym):
    """Ancho de trazo de un símbolo usado en líneas/arcos."""
    k, p = symbol_shape(sym)
    return p[0] if k == 'round' else min(p[0], p[1])


def pad_geom(sym, x, y, angle=0.0, mirror=False):
    k, p = symbol_shape(sym)
    if k == 'round':
        return Point(x, y).buffer(p[0] / 2, 16)
    if k == 'rect':
        w, h, r = p
        g = box(-w / 2, -h / 2, w / 2, h / 2)
        if r > 0:
            g = box(-w / 2 + r, -h / 2 + r, w / 2 - r, h / 2 - r).buffer(r, 8)
    else:  # oval
        w, h = p
        d = min(w, h)
        if w >= h:
            g = LineString([(-(w - d) / 2, 0), ((w - d) / 2, 0)]).buffer(d / 2, 16)
        else:
            g = LineString([(0, -(h - d) / 2), (0, (h - d) / 2)]).buffer(d / 2, 16)
    if mirror:
        g = affinity.scale(g, -1, 1, origin=(0, 0))
    if angle:
        g = affinity.rotate(g, -angle, origin=(0, 0))  # ODB++: horario
    return affinity.translate(g, x, y)


def arc_points(xs, ys, xe, ye, xc, yc, cw):
    r = math.hypot(xs - xc, ys - yc)
    a0 = math.atan2(ys - yc, xs - xc)
    a1 = math.atan2(ye - yc, xe - xc)
    if cw:
        if a1 >= a0:
            a1 -= 2 * math.pi
    else:
        if a1 <= a0:
            a1 += 2 * math.pi
    if abs(xs - xe) < 1e-9 and abs(ys - ye) < 1e-9:
        a1 = a0 + (-2 * math.pi if cw else 2 * math.pi)
    n = max(4, int(abs(a1 - a0) / (2 * math.pi) * 64))
    return [(xc + r * math.cos(a0 + (a1 - a0) * i / n), yc + r * math.sin(a0 + (a1 - a0) * i / n))
            for i in range(n + 1)]


# ---------------------------------------------------------------- features
class Feature:
    __slots__ = ('idx', 'kind', 'sym', 'pol', 'data', 'attrs', '_geom', 'net')

    def __init__(self, idx, kind, sym, pol, data, attrs):
        self.idx, self.kind, self.sym, self.pol, self.data, self.attrs = idx, kind, sym, pol, data, attrs
        self._geom = None
        self.net = None

    @property
    def width(self):
        return symbol_width(self.sym) if self.sym else None

    @property
    def length(self):
        if self.kind == 'L':
            xs, ys, xe, ye = self.data
            return math.hypot(xe - xs, ye - ys)
        if self.kind == 'A':
            return LineString(arc_points(*self.data)).length
        return 0.0

    def centerline(self):
        if self.kind == 'L':
            xs, ys, xe, ye = self.data
            return LineString([(xs, ys), (xe, ye)]) if (xs, ys) != (xe, ye) else Point(xs, ys)
        if self.kind == 'A':
            return LineString(arc_points(*self.data))
        return None

    @property
    def geom(self):
        if self._geom is None:
            if self.kind == 'P':
                x, y, ang, mir = self.data
                self._geom = pad_geom(self.sym, x, y, ang, mir)
            elif self.kind in ('L', 'A'):
                w = self.width
                cl = self.centerline()
                self._geom = cl.buffer(w / 2, 8)
            elif self.kind == 'S':
                self._geom = self.data
            else:
                self._geom = Polygon()
        return self._geom


def _parse_orient(tokens):
    """Orientación de pad: '0'..'7' o '8 ang'/'9 ang' (9 = espejado)."""
    o = int(tokens[0])
    if o < 8:
        return (o % 4) * 90.0, o >= 4, 1
    return float(tokens[1]), o == 9, 2


def read_features(path):
    syms = {}
    feats = []
    with open(path, encoding='latin-1') as fh:
        lines = fh.read().splitlines()
    i = 0
    n = len(lines)
    while i < n:
        ln = lines[i].strip()
        i += 1
        if not ln or ln.startswith('#'):
            continue
        if ln.startswith('$'):
            k, v = ln[1:].split(None, 1)
            syms[int(k)] = v.split()[0]
            continue
        body, _, attrs = ln.partition(';')
        t = body.split()
        if t[0] == 'P':
            x, y, s = _f(t[1]), _f(t[2]), int(t[3])
            ang, mir, _ = _parse_orient(t[6:])
            feats.append(Feature(len(feats), 'P', syms[s], t[4], (x, y, ang, mir), attrs))
        elif t[0] == 'L':
            feats.append(Feature(len(feats), 'L', syms[int(t[5])], t[6],
                                 tuple(map(_f, t[1:5])), attrs))
        elif t[0] == 'A':
            xs, ys, xe, ye, xc, yc = map(_f, t[1:7])
            feats.append(Feature(len(feats), 'A', syms[int(t[7])], t[8],
                                 (xs, ys, xe, ye, xc, yc, t[10].upper() == 'Y'), attrs))
        elif t[0] == 'S':
            pol = t[1]
            islands, holes = [], []
            cur, cur_hole = None, False
            while i < n:
                l2 = lines[i].strip()
                i += 1
                if l2.startswith('SE'):
                    break
                q = l2.split()
                if q[0] == 'OB':
                    cur = [(_f(q[1]), _f(q[2]))]
                    cur_hole = q[3] == 'H'
                elif q[0] == 'OS':
                    cur.append((_f(q[1]), _f(q[2])))
                elif q[0] == 'OC':
                    xs, ys = cur[-1]
                    cur.extend(arc_points(xs, ys, _f(q[1]), _f(q[2]), _f(q[3]), _f(q[4]),
                                          q[5].upper() == 'Y')[1:])
                elif q[0] == 'OE':
                    if len(cur) >= 3:
                        poly = Polygon(cur).buffer(0)
                        (holes if cur_hole else islands).append(poly)
            g = unary_union(islands)
            if holes:
                g = g.difference(unary_union(holes))
            feats.append(Feature(len(feats), 'S', None, pol, g, attrs))
        elif t[0] == 'T' or t[0] == 'B':
            feats.append(Feature(len(feats), t[0], None, 'P', None, attrs))
    return feats


# ---------------------------------------------------------------- componentes
class Pin:
    __slots__ = ('comp', 'idx', 'name', 'x', 'y', 'net', 'side', 'feats')

    def __init__(self, comp, idx, name, x, y, net, side):
        self.comp, self.idx, self.name, self.x, self.y, self.net, self.side = comp, idx, name, x, y, net, side
        self.feats = []  # [(layer, feature)]

    @property
    def ref(self):
        return f'{self.comp.ref}.{self.name}'


class Component:
    def __init__(self, idx, side, pkg, x, y, rot, mirror, ref, part):
        self.idx, self.side, self.pkg, self.x, self.y, self.rot, self.mirror = idx, side, pkg, x, y, rot, mirror
        self.ref, self.part = ref, part
        self.props = {}
        self.pins = []

    def pin(self, name):
        for p in self.pins:
            if p.name == str(name):
                return p
        raise KeyError(f'{self.ref}.{name}')


def read_components(path, side, net_names):
    comps = []
    cur = None
    with open(path, encoding='latin-1') as fh:
        for ln in fh:
            ln = ln.rstrip('\n')
            if ln.startswith('CMP '):
                body = ln.split(';')[0].split()
                cur = Component(len(comps), side, int(body[1]), _f(body[2]), _f(body[3]), _f(body[4]),
                                body[5] == 'M', body[6], ' '.join(body[7:]))
                comps.append(cur)
            elif ln.startswith('PRP ') and cur:
                m = re.match(r"PRP (\S+) '(.*)'", ln)
                if m:
                    cur.props[m.group(1)] = m.group(2)
            elif ln.startswith('TOP ') and cur:
                t = ln.split()
                net = int(t[6])
                cur.pins.append(Pin(cur, int(t[1]), t[8], _f(t[2]), _f(t[3]),
                                    net_names[net] if net >= 0 else None, side))
    return comps


# ---------------------------------------------------------------- eda/data
def read_eda(path):
    """Devuelve (layer_names, net_names, fid_net{(layer, feat): net}, pin_fids{(side, comp, pin): [(layer, feat)]})."""
    lyr, nets = [], []
    fid_net = {}
    pin_fids = defaultdict(list)
    cur_pin = None
    with open(path, encoding='latin-1') as fh:
        for ln in fh:
            t = ln.split()
            if not t:
                continue
            if t[0] == 'LYR':
                lyr = t[1:]
            elif t[0] == 'NET':
                nets.append(t[1].split(';')[0])
                cur_pin = None
            elif t[0] == 'SNT':
                cur_pin = (t[2], int(t[3]), int(t[4])) if t[1] == 'TOP' else None
            elif t[0] == 'FID' and nets:
                key = (lyr[int(t[2])], int(t[3]))
                fid_net[key] = nets[-1]
                if cur_pin:
                    pin_fids[cur_pin].append(key)
            elif t[0] == 'PKG':
                break
    return lyr, nets, fid_net, pin_fids


# ---------------------------------------------------------------- tablero
class Board:
    def __init__(self, odb_dir):
        step = os.path.join(odb_dir, 'steps', 'pcb')
        self.layers_eda, self.nets, fid_net, pin_fids = read_eda(os.path.join(step, 'eda', 'data'))
        self.layers = {}
        for lname in COPPER + ['drill_plated_bottom_layer-top_layer']:
            fp = os.path.join(step, 'layers', lname, 'features')
            feats = read_features(fp)
            for f in feats:
                f.net = fid_net.get((lname, f.idx))
            self.layers[lname] = feats
        self.comps = {}
        for side, lname in (('T', 'comp_+_top'), ('B', 'comp_+_bot')):
            for c in read_components(os.path.join(step, 'layers', lname, 'components'), side, self.nets):
                for p in c.pins:
                    for (l, fi) in pin_fids.get((side, c.idx, p.idx), []):
                        if l in self.layers:
                            p.feats.append((l, self.layers[l][fi]))
                self.comps[c.ref] = c
        self.profile = self._read_profile(os.path.join(step, 'profile'))

    def _read_profile(self, path):
        feats = read_features(path)
        g = unary_union([f.geom for f in feats if f.kind == 'S'])
        return g

    # nets de pines --------------------------------------------------------
    def pin_nets(self):
        """{ 'R1.1': net } para todos los pines."""
        return {p.ref: p.net for c in self.comps.values() for p in c.pins}

    def net_pins(self):
        d = defaultdict(list)
        for c in self.comps.values():
            for p in c.pins:
                d[p.net].append(p.ref)
        return d

    def vias(self):
        """Taladros (x, y, diámetro, net) de la capa de taladro pasante."""
        out = []
        for f in self.layers['drill_plated_bottom_layer-top_layer']:
            if f.kind == 'P':
                out.append((f.data[0], f.data[1], symbol_width(f.sym), f.net, f.attrs))
        return out

    def copper(self, layer, net, kinds=('P', 'L', 'A', 'S')):
        fs = [f for f in self.layers[layer] if f.net == net and f.kind in kinds and f.pol == 'P']
        return unary_union([f.geom for f in fs]) if fs else Polygon()


# ---------------------------------------------------------------- netlist Protel
def read_protel_net(path):
    """Devuelve (comps{ref: (footprint, comment)}, nets{name: set('R1-1', ...)})."""
    comps, nets = {}, {}
    with open(path, encoding='latin-1') as fh:
        txt = fh.read()
    for m in re.finditer(r'\[\s*\n(.*?)\n\]', txt, re.S):
        ls = [l.strip() for l in m.group(1).split('\n')]
        comps[ls[0]] = (ls[1] if len(ls) > 1 else '', ls[2] if len(ls) > 2 else '')
    for m in re.finditer(r'\(\s*\n(.*?)\n\)', txt, re.S):
        ls = [l.strip() for l in m.group(1).split('\n') if l.strip()]
        nets[ls[0]] = set(ls[1:])
    return comps, nets


def pin_to_net(nets):
    d = {}
    for n, pins in nets.items():
        for p in pins:
            d[p] = n
    return d
