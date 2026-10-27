"""Sincronía BOM <-> PCB (ODB++) y presencia de LCSC_PN.

python tools/sync.py [exports]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from odb import Board  # noqa: E402
from bom import read_bom  # noqa: E402

# componentes de PCB que no se montan (huellas sin pieza)
NO_PART = {'NT1', 'RF_PCB1'}


def main(exp='exports'):
    b = Board(os.path.join(exp, 'odb'))
    bom = {}
    for r in read_bom(os.path.join(exp, 'IncuNest.xlsx')):
        for d in str(r.get('Designator', '')).replace(' ', '').split(','):
            if d:
                bom[d] = (r.get('LCSC_PN', '').strip(), r.get('Comment', ''))
    pcb = set(b.comps)
    print('Sin LCSC_PN en PCB:', sorted(r for r, c in b.comps.items()
                                         if not c.props.get('LCSC_PN', '').startswith('C') and r not in NO_PART))
    print('En PCB y no en BOM (DNP o no montados):', sorted(pcb - set(bom) - NO_PART))
    print('En BOM y no en PCB:', sorted(set(bom) - pcb))
    diff = [(r, bom[r][0], b.comps[r].props.get('LCSC_PN')) for r in sorted(set(bom) & pcb)
            if bom[r][0] != b.comps[r].props.get('LCSC_PN')]
    print('LCSC_PN distinto BOM vs PCB:', diff or 'ninguno')
    # MPN declarado vs comentario (descripciones posiblemente desactualizadas)
    print('\nComponentes cuyo Manufacturer_Part_Number no aparece en el comentario (revisar, la referencia es LCSC_PN):')
    for r in sorted(pcb & set(bom)):
        mpn = b.comps[r].props.get('Manufacturer_Part_Number', '')
        com = bom[r][1]
        if mpn and mpn.replace(' ', '') not in com.replace(' ', '') and com.replace(' ', '') not in mpn.replace(' ', ''):
            print(f'  {r:5s} LCSC={bom[r][0]:10s} comment="{com}"  MPN="{mpn}"')


if __name__ == '__main__':
    main(*sys.argv[1:])
