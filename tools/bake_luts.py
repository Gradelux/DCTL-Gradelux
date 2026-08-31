#!/usr/bin/env python3
"""Software bake: Rec.709 look LUTs -> DaVinci Intermediate / DaVinci Wide Gamut.

Resolve's Generate 3D LUT is entirely out of this path. Every step is explicit
arithmetic using the published matrices in colorspace.py.

Per grid point, in DaVinci Intermediate code values:

    DI code  -> linear DWG           DaVinci Intermediate EOTF
             -> CIE XYZ              DWG_TO_XYZ        (supplied, verbatim)
             -> linear Rec.709       XYZ_TO_REC709     (published BT.709)
             -> Rec.709 code         BT.709 OETF
             -> [ the creative look ] clamped to the LUT domain, as a LUT node does
             -> linear Rec.709       inverse OETF
             -> CIE XYZ              REC709_TO_XYZ
             -> linear DWG           XYZ_TO_DWG        (supplied, verbatim)
             -> DI code              DaVinci Intermediate OETF

The Rec.709 matrix and transfer function appear here because the SOURCE LUTs
are Rec.709 LUTs; converting into and out of that space is the entire job.
They are confined to this offline baker and appear nowhere in the DCTL.
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from colorspace import *

def load_cube(path):
    size, data, title = None, [], None
    for line in open(path):
        t = line.strip()
        if not t or t.startswith('#'): continue
        if t.startswith('TITLE'): title = t; continue
        if t.startswith('DOMAIN'): continue
        if t.startswith('LUT_3D_SIZE'): size = int(t.split()[1]); continue
        if t.startswith('LUT_1D_SIZE'): raise ValueError(f"{path}: 1D LUT unsupported")
        p = t.split()
        if len(p) == 3: data.append((float(p[0]), float(p[1]), float(p[2])))
    if size is None or len(data) != size ** 3:
        raise ValueError(f"{path}: expected {size}^3 entries, found {len(data)}")
    return size, data

def sample(size, data, r, g, b):
    """Trilinear sample, input clamped to the 0..1 domain exactly as a LUT node does."""
    def axis(v):
        x = min(max(v, 0.0), 1.0) * (size - 1)
        i = int(min(math.floor(x), size - 2))
        return i, x - i
    ir, fr = axis(r); ig, fg = axis(g); ib, fb = axis(b)
    out = [0.0, 0.0, 0.0]
    for dz in (0, 1):
        wz = fb if dz else 1.0 - fb
        if wz == 0.0: continue
        for dy in (0, 1):
            wy = fg if dy else 1.0 - fg
            if wy == 0.0: continue
            for dx in (0, 1):
                w = (fr if dx else 1.0 - fr) * wy * wz
                if w == 0.0: continue
                c = data[(ir + dx) + (ig + dy) * size + (ib + dz) * size * size]
                out[0] += w * c[0]; out[1] += w * c[1]; out[2] += w * c[2]
    return out

def make_encoders(mode):
    if mode == 'rec709':  return rec709_encode, rec709_decode
    if mode == 'gamma24': return (lambda L: gamma_encode(L, 2.4)), (lambda V: gamma_decode(V, 2.4))
    if mode == 'gamma22': return (lambda L: gamma_encode(L, 2.2)), (lambda V: gamma_decode(V, 2.2))
    raise ValueError(mode)

def bake(src_size, src_data, out_size=33, mode='rec709', identity=False):
    """Returns out_size^3 of DI->DI entries, red index varying fastest."""
    enc, dec = make_encoders(mode)
    # DI code -> Rec.709 code is per-channel only after the matrix, so the
    # per-axis DI->linear step is precomputed once.
    lin_axis = [di_to_lin(i / (out_size - 1)) for i in range(out_size)]
    out = []
    for bi in range(out_size):
        lb = lin_axis[bi]
        for gi in range(out_size):
            lg = lin_axis[gi]
            for ri in range(out_size):
                lr = lin_axis[ri]
                x = mv(DWG_TO_XYZ, [lr, lg, lb])
                c = mv(XYZ_TO_REC709, x)
                v = [enc(max(t, 0.0)) for t in c]          # negative = outside Rec.709 gamut
                v = [min(max(t, 0.0), 1.0) for t in v]      # the LUT domain clamp
                look = v if identity else sample(src_size, src_data, v[0], v[1], v[2])
                lin709 = [dec(min(max(t, 0.0), 1.0)) for t in look]
                x2 = mv(REC709_TO_XYZ, lin709)
                dwg = mv(XYZ_TO_DWG, x2)
                out.append(tuple(lin_to_di(t) for t in dwg))
    return out

def write_cube(path, size, data, title):
    with open(path, 'w') as f:
        f.write(f'TITLE "{title}"\n')
        f.write(f'LUT_3D_SIZE {size}\n')
        f.write('DOMAIN_MIN 0.0 0.0 0.0\n')
        f.write('DOMAIN_MAX 1.0 1.0 1.0\n\n')
        for c in data:
            f.write(f'{c[0]:.8f} {c[1]:.8f} {c[2]:.8f}\n')

if __name__ == '__main__':
    src_dir, out_dir = sys.argv[1], sys.argv[2]
    mode = sys.argv[3] if len(sys.argv) > 3 else 'rec709'
    os.makedirs(out_dir, exist_ok=True)
    names = sorted(n[:-5] for n in os.listdir(src_dir) if n.endswith('.cube'))

    ref = bake(2, [(0,0,0)]*8, 33, mode, identity=True)
    write_cube(os.path.join(out_dir, '_Reference_Identity.cube'), 33, ref,
               'CineCore reference identity - DaVinci Intermediate in and out')
    print(f"  {'_Reference_Identity':<22} architecture reference, no creative look")

    for n in names:
        s, d = load_cube(os.path.join(src_dir, n + '.cube'))
        out = bake(s, d, 33, mode)
        write_cube(os.path.join(out_dir, n + '.cube'), 33, out,
                   f'CineCore {n} - DaVinci Wide Gamut / DaVinci Intermediate in and out')
        print(f"  {n:<22} from {s}^3 source -> 33^3 DI")
