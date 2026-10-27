"""Lectura (solo lectura) de Motherboard.PcbDoc sin exportar: componentes, nets, pads, vías, fills, pistas y reglas.

Sirve para comprobar cambios entre exportaciones; la verificación completa sigue siendo con el ODB++.
Coordenadas en mm, en el sistema de la PcbDoc (no coinciden con el origen del ODB++: compara siempre en relativo).

python tools/pcbdoc.py [PCB/Motherboard.PcbDoc]
"""
import struct
import sys

import olefile

MIL = 0.0254
LAYERS = {1: 'top', 32: 'bottom', 33: 'top_overlay', 34: 'bottom_overlay', 35: 'top_paste', 36: 'bottom_paste',
          37: 'top_solder', 38: 'bottom_solder', 74: 'multi'}
MASK_MODE = {0: 'none', 1: 'regla', 2: 'manual'}


def _u(v):
    return v / 1e4 * MIL


class PcbDoc:
    def __init__(self, path='PCB/Motherboard.PcbDoc'):
        self.o = olefile.OleFileIO(path)
        self.nets = [r.get('NAME') for r in self._text('Nets6/Data')]
        self.comp_recs = self._text('Components6/Data')
        self.comps = [r.get('SOURCEDESIGNATOR') for r in self.comp_recs]

    def _text(self, name, hdr=4):
        d = self.o.openstream(name).read()
        i, out = 0, []
        while i + hdr <= len(d):
            n = struct.unpack('<I', d[i + hdr - 4:i + hdr])[0]
            s = d[i + hdr:i + hdr + n].rstrip(b'\0').decode('latin-1', 'ignore')
            i += hdr + n
            out.append(dict(kv.split('=', 1) for kv in s.split('|') if '=' in kv))
        return out

    def _net(self, k):
        return self.nets[k] if k < len(self.nets) else None

    def _comp(self, k):
        return self.comps[k] if k < len(self.comps) else None

    def comp(self, ref):
        for r in self.comp_recs:
            if r.get('SOURCEDESIGNATOR') == ref:
                return dict(r, x=float(r['X'].rstrip('mil')) * MIL, y=float(r['Y'].rstrip('mil')) * MIL)
        return None

    def rules(self):
        d = self.o.openstream('Rules6/Data').read()
        i, out = 0, []
        while i + 6 <= len(d):
            n = struct.unpack('<I', d[i + 2:i + 6])[0]
            s = d[i + 6:i + 6 + n].rstrip(b'\0').decode('latin-1', 'ignore')
            i += 6 + n
            out.append(dict(kv.split('=', 1) for kv in s.split('|') if '=' in kv))
        return out

    def _simple(self, name, parse):
        d = self.o.openstream(name).read()
        i, out = 0, []
        while i + 5 <= len(d):
            n = struct.unpack('<I', d[i + 1:i + 5])[0]
            out.append(parse(d[i + 5:i + 5 + n]))
            i += 5 + n
        return out

    def vias(self):
        def p(b):
            x, y, dia, hole = struct.unpack('<iiii', b[13:29])
            return dict(x=_u(x), y=_u(y), dia=_u(dia), hole=_u(hole), net=self._net(struct.unpack('<H', b[3:5])[0]),
                        comp=self._comp(struct.unpack('<H', b[7:9])[0]),
                        tent_top=bool(b[1] & 0x20), tent_bottom=bool(b[1] & 0x40))
        return self._simple('Vias6/Data', p)

    def fills(self):
        def p(b):
            x1, y1, x2, y2 = struct.unpack('<iiii', b[13:29])
            return dict(layer=LAYERS.get(b[0], b[0]), comp=self._comp(struct.unpack('<H', b[7:9])[0]),
                        net=self._net(struct.unpack('<H', b[3:5])[0]),
                        cx=_u((x1 + x2) / 2), cy=_u((y1 + y2) / 2), w=abs(_u(x2 - x1)), h=abs(_u(y2 - y1)))
        return self._simple('Fills6/Data', p)

    def tracks(self):
        def p(b):
            x1, y1, x2, y2, w = struct.unpack('<iiiii', b[13:33])
            return dict(layer=LAYERS.get(b[0], b[0]), net=self._net(struct.unpack('<H', b[3:5])[0]),
                        comp=self._comp(struct.unpack('<H', b[7:9])[0]),
                        x1=_u(x1), y1=_u(y1), x2=_u(x2), y2=_u(y2), w=_u(w))
        return self._simple('Tracks6/Data', p)

    def pads(self):
        d = self.o.openstream('Pads6/Data').read()
        i, out = 0, []
        while i < len(d):
            i += 1  # tipo de registro
            subs = []
            for _ in range(6):
                n = struct.unpack('<I', d[i:i + 4])[0]
                subs.append(d[i + 4:i + 4 + n])
                i += 4 + n
            name = subs[0][1:1 + subs[0][0]].decode('latin-1') if subs[0] else ''
            m = subs[4]
            x, y, sx, sy = struct.unpack('<iiii', m[13:29])
            pm, sm = struct.unpack('<ii', m[86:94])
            out.append(dict(name=name, layer=LAYERS.get(m[0], m[0]), net=self._net(struct.unpack('<H', m[3:5])[0]),
                            comp=self._comp(struct.unpack('<H', m[7:9])[0]), x=_u(x), y=_u(y), sx=_u(sx), sy=_u(sy),
                            paste=_u(pm), paste_mode=MASK_MODE.get(m[101], m[101]),
                            mask=_u(sm), mask_mode=MASK_MODE.get(m[102], m[102])))
        return out


if __name__ == '__main__':
    pcb = PcbDoc(*sys.argv[1:])
    print(len(pcb.comps), 'componentes,', len(pcb.nets), 'nets,', len(pcb.vias()), 'vías,', len(pcb.pads()), 'pads')
