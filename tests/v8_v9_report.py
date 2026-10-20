#!/usr/bin/env python3
# =============================================================================
#  Gradelux FilmLab: Version 8 -> Version 9 difference report (the 13 V8 looks)
# -----------------------------------------------------------------------------
#  Builds math_test_v9.c in its DUMP_LOOKS_V8 / DUMP_LOOKS_V9 modes (identical
#  inputs: a -8..+8 stop grey ramp and 3000 random DaVinci Intermediate
#  colours per look, every other control neutral) and reports, per look:
#    - the grey response change v9 - v8 in stops at -6 .. +6 stops,
#    - the colour change through a viewing transform (CIELAB): mean dE, and
#      how it splits into lightness (dL*), chroma (dC*) and hue (dh, chroma-
#      weighted, colours with C* > 10),
#    - the dropdown mapping check: the v8 look each v9 look is closest to
#      (must be itself for every look).
#  Middle-grey anchoring is a tonal change, so for the 11 looks whose
#  parameters are unchanged the differences should be mostly lightness, with
#  small chroma and hue changes. Argent and Nocturne were redesigned.
#
#    cd tests && python3 v8_v9_report.py
# =============================================================================
import os, subprocess, tempfile, math
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
NAMES = ["Off", "Verona", "Verdant", "Midnight", "Aurelia", "Solara", "Moss", "Chroma", "Silvergrain",
         "Tungsten", "Halcyon", "Argent", "Nocturne", "Pastelle"]
REDESIGNED = {11, 12}

tmp = tempfile.mkdtemp(prefix='v8v9_')
def dump(mode):
    exe = os.path.join(tmp, mode)
    subprocess.check_call(['gcc', '-O2', '-w', '-D' + mode, '-o', exe, os.path.join(HERE, 'math_test_v9.c'), '-lm'])
    out = subprocess.run([exe], capture_output=True, text=True, check=True).stdout.split('\n')
    rows = [l.split() for l in out if l.strip()]
    look = np.array([int(r[0]) for r in rows])
    vals = np.array([[float.fromhex(t) for t in r[1:]] for r in rows])
    return look, vals[:, :3], vals[:, 3:]
l8, in8, o8 = dump('DUMP_LOOKS_V8')
l9, in9, o9 = dump('DUMP_LOOKS_V9')
assert (l8 == l9).all() and np.array_equal(in8, in9), 'dumps are not on identical inputs'

def di_to_lin(x):
    return np.where(x <= 0.02740668, x / 10.44426855, 2.0 ** (x / 0.07329248 - 7.0) - 0.0075)
DWG2709 = np.array([[1.8986148993, -0.7921761834, -0.1064387159],
                    [-0.1689487865, 1.4889757541, -0.3200269676],
                    [-0.1215391606, -0.3156758531, 1.4372150137]])
YDWG = np.array([0.27411851, 0.8736319, -0.14775041])
def lab(di):
    l = di_to_lin(di) @ DWG2709.T
    x = np.clip(l * 0.8, 0, None)
    d = np.clip((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0, 1)
    M = np.array([[0.4124, 0.3576, 0.1805], [0.2126, 0.7152, 0.0722], [0.0193, 0.1192, 0.9505]])
    X = d @ M.T / np.array([0.95047, 1.0, 1.08883])
    f = np.where(X > 0.008856, np.cbrt(X), 7.787 * X + 16 / 116)
    return np.stack([116 * f[:, 1] - 16, 500 * (f[:, 0] - f[:, 1]), 200 * (f[:, 1] - f[:, 2])], 1)

stops = [-6, -4, -2, 0, 2, 4, 6]
print('Version 8 -> Version 9: the 13 Version 8 looks (identical inputs, every other control neutral)\n')
print('Grey response change, v9 - v8, in stops (luminance) at exposure (stops from 18% grey):')
print('%-12s' % 'look' + ''.join('%7d' % s for s in stops) + '   colours: dE   dL*    dC*   |dh| deg   closest v8 look')
bad_map = 0
lab8_by = {l: lab(o8[(l8 == l)][65:]) for l in range(1, 14)}
for l in range(1, 14):
    sel = (l8 == l)
    g8, g9 = o8[sel][:65], o9[sel][:65]
    y8 = di_to_lin(g8) @ YDWG; y9 = di_to_lin(g9) @ YDWG
    row = []
    for s in stops:
        i = int(round((s + 8) / 0.25))
        row.append(math.log2(y9[i] / y8[i]))
    a = lab8_by[l]; b = lab(o9[sel][65:])
    dE = np.sqrt(((a - b) ** 2).sum(1)).mean()
    dL = (b[:, 0] - a[:, 0]).mean()
    Ca, Cb = np.hypot(a[:, 1], a[:, 2]), np.hypot(b[:, 1], b[:, 2])
    dC = (Cb - Ca).mean()
    m = Ca > 10
    dh = np.abs((np.degrees(np.arctan2(b[m, 2], b[m, 1])) - np.degrees(np.arctan2(a[m, 2], a[m, 1])) + 180) % 360 - 180)
    dhw = (dh * Ca[m]).sum() / Ca[m].sum() if m.any() else float('nan')
    # mapping: which v8 look is the v9 look closest to?
    dists = {k: np.sqrt(((lab8_by[k] - b) ** 2).sum(1)).mean() for k in range(1, 14)}
    best = min(dists, key=dists.get)
    if best != l: bad_map += 1
    print('%-12s' % NAMES[l] + ''.join('%+7.2f' % v for v in row) +
          '   %6.2f %+6.2f %+6.2f %7s      %s%s' % (dE, dL, dC, ('%.2f' % dhw) if dhw == dhw else 'mono', NAMES[best],
                                                      '  (redesigned)' if l in REDESIGNED else ''))
print('\nDropdown mapping: %s' % ('every v9 look is closest to the same-numbered v8 look'
                                  if bad_map == 0 else '%d looks map elsewhere' % bad_map))
raise SystemExit(1 if bad_map else 0)
