"""Imprime los pines de uno o varios componentes con su net (netlist del esquema).

python tools/nets.py U4 U11 ...      (opcional: --net NOMBRE para listar una net)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from odb import read_protel_net, pin_to_net  # noqa: E402


def pin_key(p):
    s = p.split('-', 1)[1]
    return (0, int(s), '') if s.isdigit() else (1, 0, s)


def main(args, netfile='exports/IncuNest.NET'):
    comps, nets = read_protel_net(netfile)
    p2n = pin_to_net(nets)
    if args and args[0] == '--net':
        for n in args[1:]:
            print(n, sorted(nets.get(n, [])))
        return
    for ref in args:
        print(ref, comps.get(ref))
        for p in sorted((p for p in p2n if p.split('-')[0] == ref), key=pin_key):
            n = p2n[p]
            mem = sorted(nets[n])
            print(f'   {p:8s} {n:20s} {mem if len(mem) < 12 else str(len(mem)) + " pines"}')


if __name__ == '__main__':
    main(sys.argv[1:])
