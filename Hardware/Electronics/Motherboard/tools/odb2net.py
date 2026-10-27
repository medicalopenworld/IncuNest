"""Genera exports/IncuNest.NET (formato Protel) a partir del ODB++ cuando el Output Job no exporta netlist.

Las nets salen de la PCB (tras el ECO coinciden con el esquema; comprobar con pending_check.py).
python tools/odb2net.py [exports]
"""
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from odb import Board  # noqa: E402


def main(exp='exports'):
    b = Board(os.path.join(exp, 'odb'))
    nets = collections.defaultdict(set)
    out = []
    for ref, c in sorted(b.comps.items()):
        out.append(f"[\n{ref}\n\n\n\n\n\n]")
        for p in c.pins:
            if p.net and p.net != '$NONE$':
                nets[p.net].add(f'{ref}-{p.name}')
    out += ['(\n' + n + '\n' + '\n'.join(sorted(ps)) + '\n)' for n, ps in sorted(nets.items())]
    with open(os.path.join(exp, 'IncuNest.NET'), 'w', encoding='latin-1') as fh:
        fh.write('\n'.join(out) + '\n')
    print(f'{len(b.comps)} componentes, {len(nets)} nets -> {exp}/IncuNest.NET')


if __name__ == '__main__':
    main(*sys.argv[1:])
