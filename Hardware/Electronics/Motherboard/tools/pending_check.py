"""Comprueba los pendientes de CLAUDE.md leyendo directamente los .SchDoc y la .PcbDoc (sin exportar).

python -X utf8 tools/pending_check.py

Las nets salen de la asignación de pads de la PcbDoc (tras el ECO reflejan el esquema). Lo que depende de
polígonos (min-cut, áreas SW, Kelvin, retornos) solo se puede cerrar con el ODB++ (tools/run_all.py).
"""
import collections
import os
import sys

from shapely.geometry import Point, box
from shapely.ops import unary_union

sys.path.insert(0, os.path.dirname(__file__))
from odb import read_protel_net  # noqa: E402
from pcbdoc import PcbDoc  # noqa: E402
from schdoc import all_components  # noqa: E402

OK, NO, INFO = '✅', '❌', '  '


def main(prev_net='exports/prev/IncuNest.NET'):
    sch = {c['des']: c for c in all_components()}
    p = PcbDoc()
    pads, vias = p.pads(), p.vias()
    tracks = p.tracks()
    pn = {(x['comp'], x['name']): x['net'] for x in pads if x['comp']}
    members = collections.defaultdict(set)
    for (c, n), net in pn.items():
        if net:
            members[net].add(f'{c}-{n}')

    def net_of(ref, pin):
        return pn.get((ref, pin))

    def mem(ref, pin):
        n = net_of(ref, pin)
        return sorted(members.get(n, [])) if n else []

    def padbox(x):
        return box(x['x'] - x['sx'] / 2, x['y'] - x['sy'] / 2, x['x'] + x['sx'] / 2, x['y'] + x['sy'] / 2)

    def pad_geom(ref, pins=None, net=None):
        ps = [padbox(x) for x in pads if x['comp'] == ref and (pins is None or x['name'] in pins)
              and (net is None or x['net'] == net)]
        return unary_union(ps) if ps else None

    def vias_near(geom, net, r):
        return sum(1 for v in vias if v['net'] == net and geom.distance(Point(v['x'], v['y'])) <= r)

    def line(ok, text):
        print(f'{OK if ok else NO} {text}')

    old, _ = read_protel_net(prev_net) if os.path.exists(prev_net) else ({}, {})
    pcb_refs = {c for c in p.comps if c}
    print('== Sincronía')
    print(f'{INFO} Esquema vs exportación anterior: nuevos {sorted(set(sch) - set(old))}, eliminados {sorted(set(old) - set(sch))}')
    line(set(sch) == pcb_refs, f'Esquema = PCB (solo esquema: {sorted(set(sch) - pcb_refs)}, solo PCB: {sorted(pcb_refs - set(sch))})')
    sin_lcsc = sorted(d for d, c in sch.items() if not c['params'].get('LCSC_PN') and d not in ('RF_PCB1', 'J1', 'R81', 'R82')
                      and not d.startswith('C1') or (d in ('C17', 'C18', 'C19', 'C20') and False))
    sin_lcsc = sorted(d for d, c in sch.items() if not c['params'].get('LCSC_PN'))
    print(f'{INFO} Sin LCSC_PN en el esquema (revisar que sean DNP): {sin_lcsc}')

    print('\n== Esquema')
    line('R18' not in sch, f"R18 (0 Ω en DC_DC_EN) {'eliminada' if 'R18' not in sch else 'sigue'}; DC_DC_EN = {sorted(members.get('DC_DC_EN', []))}")
    line(net_of('J9', '1') != 'DGND', f"J9-1 en {net_of('J9', '1')} (inversión de batería)")
    line(len(mem('U11', '4')) >= 3, f"CHRG_OK: {mem('U11', '4')}")
    for label, pin in [('IO6 USB_FAULT (pull-up)', '6'), ('IO39 MODEM_PWR_EN (pull-down)', '32'),
                       ('IO40 5V_EN (pull-down)', '33')]:
        m = mem('U1', pin)
        line(any(x.startswith('R') for x in m), f'{label}: {m}')
    m31, m19 = mem('U1', '31'), mem('U1', '19')
    line('J5-3' not in m31, f'Tacómetro J5-3 ↔ IO38 directo: {m31}')
    line('J5-4' not in m19, f'PWM J5-4 ↔ IO11 directo: {m19}')
    line(len(members.get('FAN-', [])) > 2, f"FAN− (flyback): {sorted(members.get('FAN-', []))}")
    line(net_of('J14', 'A4') != '5V@2A', f"VBUS de J14 en {net_of('J14', 'A4')}")
    for j in ('J14', 'J13'):
        line(bool(net_of(j, 'A5')) and bool(net_of(j, 'B5')), f'CC de {j}: A5={net_of(j, "A5")} B5={net_of(j, "B5")}')
    line(len(mem('J14', 'A6')) > 3, f"ESD en D+ de J14: {mem('J14', 'A6')}")
    kws = collections.Counter()
    for c in sch.values():
        t = f"{c['comment']} {c['lib']}".upper()
        for k in ('TPS3431', 'LMV393', 'KSD', 'BSC010', 'USBLC6', '103AT', 'BZT52C15'):
            if k in t:
                kws[k] += 1
    line(all(kws[k] for k in ('TPS3431', 'LMV393')), f'Piezas de seguridad del calefactor y batería presentes: {dict(kws) or "ninguna"}')
    r8 = sch.get('R8')
    print(f"{INFO} R8 = {r8['comment'] if r8 else '—'} (objetivo 2k2 1206 con bimetálico)")
    d10 = sch.get('D10')
    line(d10 and 'SMBJ28' in (d10['comment'] or ''), f"D10 = {d10['comment'] if d10 else '—'} (objetivo SMBJ28CA)")
    for ref, want, why in (('R49', 'C844294', '2 mΩ: 12 A de V_SYS'), ('R59', 'C393074', '2512 5 mΩ 3 W: 12 A desde batería')):
        c = sch.get(ref)
        lc = c['params'].get('LCSC_PN') if c else None
        line(lc == want, f'{ref} LCSC_PN = {lc} (objetivo {want}, {why})')
    for ref, want in (('C105', 'C136005'), ('C109', 'C178548'),('C69', 'C45783'), ('C70', 'C45783'), ('C71', 'C45783'), ('C72', 'C45783'),
                      ('C79', 'C45783'), ('C80', 'C45783'), ('C88', 'C45783'), ('C89', 'C45783')):
        c = sch.get(ref)
        lc = c['params'].get('LCSC_PN') if c else None
        line(lc in (want, 'C7196', 'C784585', 'C116241', 'C165974'), f"{ref} = {c['comment'] if c else '—'} LCSC_PN {lc} (objetivo {want})")
    j9 = sch.get('J9')
    line(j9 and j9['params'].get('LCSC_PN') != 'C8445', f"J9 LCSC_PN = {j9['params'].get('LCSC_PN') if j9 else '—'} (C8445 = 10 A)")

    print('\n== AFE: desacoplo de RX_ANA_SUP')
    ana = 'NetANA'
    ana = net_of('U4', '33')
    caps = [c for c in members.get(ana, []) if c.startswith('C')]
    for pin in ('33', '39'):
        g = pad_geom('U4', [pin])
        best = []
        for c in caps:
            ref, pnum = c.split('-', 1)
            cg = pad_geom(ref, [pnum])
            if cg is not None:
                best.append((g.distance(cg), ref))
        best.sort()
        near = [(r, round(d, 2)) for d, r in best if d <= 2.0]
        line(bool(near), f'U4-{pin}: condensadores a ≤2 mm {near or "ninguno"} (más cercano {best[0][1]} a {best[0][0]:.2f} mm)')
    tx = {r: net_of(r, '1') for r in ('C12', 'C13', 'C15')}
    line(all(v == 'DGND' for v in tx.values()), f'Desacoplo de TX del AFE (5V@2A) a DGND: {tx}')
    for ref in sorted({c.split('-')[0] for c in caps}):
        gnd = pad_geom(ref, net='DGND')
        if gnd is not None and ref in ('C100', 'C101', 'C102'):
            val = sch.get(ref, {}).get('comment') if ref in sch else '?'
            print(f"{INFO} {ref} ({val}, LCSC {sch[ref]['params'].get('LCSC_PN') if ref in sch else '?'}): "
                  f"vías DGND a ≤1 mm del pad de GND: {vias_near(gnd, 'DGND', 1.0)}")

    print('\n== Layout')
    for ref in ('Q12', 'Q11'):
        g = pad_geom(ref, net='DGND')
        n6, n10 = vias_near(g, 'DGND', 0.6), vias_near(g, 'DGND', 1.0)
        line(n10 >= 4, f'{ref}: vías DGND junto al source ≤0,6 mm {n6}, ≤1,0 mm {n10}')
    # Q5 (fototerapia, 0,7 A) no necesita vías térmicas
    for ref, net in (('Q2', 'NetQ2_4'), ('Q13', 'NetC56_2'), ('Q1', 'DC_ACTUATORS')):
        g = pad_geom(ref, net=net)
        n = vias_near(g, net, 1.5)
        line(n >= 8, f'{ref}: vías de {net} a ≤1,5 mm de la pestaña/drain: {n} (objetivo 8–12)')
    u4 = pad_geom('U11', ['33'])
    line(vias_near(u4, 'DGND', 0.0) >= 5, f"U11 EP: vías dentro {vias_near(u4, 'DGND', 0.0)}")
    u1 = pad_geom('U1', [str(i) for i in range(41, 50)])
    n = sum(1 for v in vias if u1.buffer(0.2).contains(Point(v['x'], v['y'])))
    line(n >= 9, f'EPAD del ESP32: vías dentro {n} (objetivo 9–16)')
    for net in ('HEATER', 'NetQ2_4', 'DC_ACTUATORS', 'PHOTOTHERAPY+', 'PHOTOTHERAPY-', 'V_IN_PROTECTED', 'V_SYS'):
        ws = [t['w'] for t in tracks if t['net'] == net]
        nv = sum(1 for v in vias if v['net'] == net)
        print(f"{INFO} {net:15s} pista máx {max(ws) if ws else 0:.2f} mm, vías totales {nv}")
    # vías equivalentes de 0,3 mm de taladro (capacidad ∝ perímetro del taladro)
    def via_eq(geom, net, r):
        vs = [v for v in vias if v['net'] == net and geom.distance(Point(v['x'], v['y'])) <= r]
        return len(vs), sum(v['hole'] for v in vs) / 0.3
    for label, geom, net, want in (
            ('Q7/Q14 → R48', unary_union([pad_geom('Q7', net='V_IN_PROTECTED'), pad_geom('Q14', net='V_IN_PROTECTED')]),
             'V_IN_PROTECTED', 10),
            ('Q1 (source) ← R49', pad_geom('Q1', net='V_SYS'), 'V_SYS', 10)):
        n, eq = via_eq(geom, net, 3.0)
        line(eq >= want, f'Vías {label}: {n} a ≤3 mm = {eq:.1f} vías equivalentes de 0,3 mm (objetivo ≥{want})')
    open_v = collections.Counter(v['comp'] or 'libres' for v in vias if not (v['tent_top'] and v['tent_bottom']))
    print(f'{INFO} Vías no tapadas por alguna cara: {dict(open_v)} (U11 abierta en bottom es intencionado)')
    holes = [x for x in pads if x['comp'] is None and x['sx'] > 2.5 and not x['net']]
    line(not holes, f'Taladros de montaje sin net: {len(holes)}')

    print('\n== Reglas y stackup')
    rules = {r.get('NAME'): r for r in p.rules()}
    for r in rules.values():
        k = r.get('RULEKIND')
        if k == 'SolderMaskExpansion':
            line(r.get('EXPANSION') in ('1.9685mil', '2mil'), f"{r['NAME']}: {r.get('EXPANSION')}")
        elif k == 'MinimumSolderMaskSliver':
            print(f"{OK if r.get('ENABLED') == 'TRUE' else NO} {r['NAME']} ({r.get('SCOPE2EXPRESSION')}): {r.get('MINSOLDERMASKWIDTH')} enabled={r.get('ENABLED')}")
        elif k in ('SilkToSolderMaskClearance', 'SilkToSilkClearance'):
            v = r.get('MINSILKSCREENTOMASKGAP') or r.get('SILKTOSILKCLEARANCE')
            line(v not in ('0mil', None), f"{r['NAME']}: {v}")
        elif k == 'ShortCircuit' and 'PGND' in str(r.get('SCOPE1EXPRESSION')):
            line(False, f"{r['NAME']} (PGND–DGND) sigue")
    b = p._text('Board6/Data')[0]
    inner = [b.get('LAYER2COPTHICK'), b.get('LAYER3COPTHICK')]
    line(all(v and float(v.rstrip('mil')) < 1.0 for v in inner), f'Cobre de capas internas en Altium: {inner} (0,5 oz = 0,69 mil)')
    print('\nSin verificar aquí (necesita el ODB++): áreas SW1/SW2, sense a SW, Kelvin R49/R59, L3 bajo termistor/ESP32/USB, '
          'pin 27 de U11, choques de serigrafía, min-cut con polígonos.')


if __name__ == '__main__':
    main(*sys.argv[1:])
