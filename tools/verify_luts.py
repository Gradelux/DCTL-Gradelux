#!/usr/bin/env python3
"""Verification suite for the baked CineCore LUTs. Exits non-zero on failure."""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from colorspace import *
from bake_luts import load_cube, sample, bake

BAKED, SRC = sys.argv[1], sys.argv[2]
fails = []
def check(name, ok, info=""):
    print(("  PASS  " if ok else "  FAIL  ") + name + (("   " + info) if info else ""))
    if not ok: fails.append(name)

names = sorted(n[:-5] for n in os.listdir(BAKED) if n.endswith('.cube'))
looks = [n for n in names if not n.startswith('_')]
print(f"{len(names)} files ({len(looks)} looks + reference)\n")

luts = {}
print("--- 1. structure, syntax, size ---")
bad_size = []; bad_syntax = []
for n in names:
    try:
        s, d = load_cube(os.path.join(BAKED, n + '.cube'))
        luts[n] = (s, d)
        if s != 33: bad_size.append(f"{n}={s}")
    except Exception as e:
        bad_syntax.append(f"{n}: {e}")
check("every file parses as valid .cube", not bad_syntax, "; ".join(bad_syntax))
check("every LUT is 33-point", not bad_size, "; ".join(bad_size))
check("every LUT has 35937 entries", all(len(d) == 35937 for _, d in luts.values()))

print("\n--- 2. numeric validity ---")
bad = []
for n, (s, d) in luts.items():
    for c in d:
        for v in c:
            if v != v or math.isinf(v): bad.append(n); break
        else: continue
        break
check("no NaN or Inf anywhere", not bad, "; ".join(bad))
rng = {n: (min(min(c) for c in d), max(max(c) for c in d)) for n, (s, d) in luts.items()}
oob = [f"{n}[{lo:.4f},{hi:.4f}]" for n, (lo, hi) in rng.items() if lo < -1e-6 or hi > 1.0 + 1e-6]
check("all values inside the 0..1 code range", not oob, "; ".join(oob))

print("--- 3. monotonic neutral axis ---")
# Measured on the diagonal GRID VALUES. Sampling the trilinear diagonal instead
# would mix in off-diagonal corners, which is a property of 3D LUT
# interpolation rather than of the bake.
nonmono = []
for n, (s, d) in luts.items():
    vals = [sum(d[i + i*s + i*s*s]) / 3.0 for i in range(s)]
    drops = [abs(vals[i] - vals[i-1]) for i in range(1, s) if vals[i] < vals[i-1] - 1e-9]
    if drops: nonmono.append(f"{n}(max {max(drops):.2e})")
check("neutral grid axis is monotonic in every LUT", not nonmono, "; ".join(nonmono))

# The trilinear diagonal can dip slightly even when the grid is monotonic.
# Reported for magnitude rather than treated as a bake defect.
worst_dip = 0.0; dip_at = None
for n, (s, d) in luts.items():
    prev = None
    for i in range(401):
        x = i / 400.0
        m = sum(sample(s, d, x, x, x)) / 3.0
        if prev is not None and m < prev:
            if prev - m > worst_dip: worst_dip, dip_at = prev - m, n
        prev = m
# Budget calibrated to the measured 33^3 characteristic, not to make the test
# pass: uniform grid points in DI are exponential in linear, so interpolating
# the curved round trip introduces small non-monotonicities. Source LUTs show
# zero; 65^3 halves it rather than removing it. See DEVNOTES 1.8.2.
check("trilinear diagonal dips within the 33^3 budget", worst_dip < 5e-3,
      f"largest {worst_dip:.2e} DI = {worst_dip/DI_C:.4f} stop, in {dip_at}")

# Neutral preservation is only meaningful for the reference. A creative look is
# entitled to tint greys, and most of these do.
s, d = luts['_Reference_Identity']
mx = max(max(d[i + i*s + i*s*s]) - min(d[i + i*s + i*s*s]) for i in range(s))
check("reference keeps neutrals neutral", mx < 1e-6, f"largest R-B spread {mx:.2e}")
tint = sorted(((max(max(d2[i+i*s2+i*s2*s2]) - min(d2[i+i*s2+i*s2*s2]) for i in range(s2)), n)
               for n, (s2, d2) in luts.items() if not n.startswith('_')), reverse=True)
print(f"        creative looks tint neutrals by design; strongest: "
      f"{tint[0][1]} {tint[0][0]:.4f}, weakest: {tint[-1][1]} {tint[-1][0]:.4f}")

print("\n--- 4. endpoints ---")
blacks = {n: sample(s, d, 0, 0, 0) for n, (s, d) in luts.items()}
whites = {n: sample(s, d, 1, 1, 1) for n, (s, d) in luts.items()}
check("black endpoint is at or near DI 0 in every LUT",
      all(max(v) < 0.12 for v in blacks.values()),
      "max " + f"{max(max(v) for v in blacks.values()):.4f}")
DIW = lin_to_di(1.0)
check("white endpoint never exceeds DI diffuse white",
      all(max(v) <= DIW + 1e-4 for v in whites.values()),
      f"diffuse white = {DIW:.6f}, max seen {max(max(v) for v in whites.values()):.6f}")

print("\n--- 5. the reference identity LUT ---")
s, d = luts['_Reference_Identity']
# AT GRID POINTS, where no interpolation is involved, so this measures the
# round trip itself rather than 33^3 sampling.
worst = 0.0; worst_at = 0.0
for i in range(s):
    x = i / (s - 1)
    if di_to_lin(x) > 1.0: break          # above diffuse white the domain clamp bites
    e = max(abs(c - x) for c in d[i + i*s + i*s*s])
    if e > worst: worst, worst_at = e, x
check("reference round trip is exact at grid points below diffuse white",
      worst < 1e-6, f"max deviation {worst:.2e} at DI {worst_at:.4f}")

sat = 0.0; n_tested = 0
for ri in range(0, s, 2):
    for gi in range(0, s, 2):
        for bi in range(0, s, 2):
            lin = [di_to_lin(i / (s - 1)) for i in (ri, gi, bi)]
            c709 = mv(XYZ_TO_REC709, mv(DWG_TO_XYZ, lin))
            if min(c709) < 0.0 or max(c709) > 1.0: continue   # outside Rec.709, clamp expected
            y = d[ri + gi*s + bi*s*s]
            sat = max(sat, max(abs(y[k] - [ri, gi, bi][k] / (s - 1)) for k in range(3)))
            n_tested += 1
check("reference round trip is exact for in-gamut colour too", sat < 1e-6,
      f"max {sat:.2e} over {n_tested} in-gamut grid points")

# What 33^3 costs BETWEEN grid points, where the clamp forms a corner.
interp = 0.0; interp_at = 0.0
for i in range(401):
    x = i / 400.0
    if di_to_lin(x) > 1.0: break
    e = max(abs(c - x) for c in sample(s, d, x, x, x))
    if e > interp: interp, interp_at = e, x
print(f"        33^3 interpolation error between grid points peaks at "
      f"{interp:.5f} DI near the clamp knee at DI {interp_at:.4f}")
flat = max(abs(sample(s, d, 1, 1, 1)[k] - sample(s, d, 0.6, 0.6, 0.6)[k]) for k in range(3))
check("reference saturates above diffuse white as expected", flat < 0.01, f"spread {flat:.6f}")

print("\n--- 6. direction of the transform ---")
n0, (s0, d0) = 'Koda', luts['Koda']
ss, sd = load_cube(os.path.join(SRC, 'Koda.cube'))
pred_err = 0.0
for i in range(s0):                       # grid points only: no interpolation
    x = i / (s0 - 1)
    lin = di_to_lin(x)
    c = mv(XYZ_TO_REC709, mv(DWG_TO_XYZ, [lin]*3))
    v = [min(max(rec709_encode(max(t, 0.0)), 0.0), 1.0) for t in c]
    look = sample(ss, sd, *v)
    back = mv(XYZ_TO_DWG, mv(REC709_TO_XYZ, [rec709_decode(t) for t in look]))
    pred = [lin_to_di(t) for t in back]
    act = d0[i + i*s0 + i*s0*s0]
    pred_err = max(pred_err, max(abs(pred[k] - act[k]) for k in range(3)))
check("baked output matches an independent forward recomputation at every grid point",
      pred_err < 1e-6, f"max {pred_err:.2e}")

unconverted = []
for n in looks:
    ss, sd = load_cube(os.path.join(SRC, n + '.cube'))
    s, d = luts[n]
    diff = max(max(abs(sample(s, d, x, x, x)[k] - sample(ss, sd, x, x, x)[k]) for k in range(3))
               for x in [0.1, 0.3, 0.5, 0.7, 0.9])
    if diff < 0.02: unconverted.append(f"{n}({diff:.4f})")
check("every baked LUT differs substantially from its Rec.709 source",
      not unconverted, "; ".join(unconverted))

print("\n--- 7. looks are distinct ---")
dupes = []
sig = {}
for n in looks:
    s, d = luts[n]
    sig[n] = [sample(s, d, *p) for p in
              [(0.2,0.2,0.2),(0.4,0.4,0.4),(0.45,0.2,0.15),(0.2,0.4,0.2),(0.15,0.2,0.45),(0.5,0.45,0.2)]]
for i, a in enumerate(looks):
    for b in looks[i+1:]:
        m = max(abs(sig[a][k][c] - sig[b][k][c]) for k in range(6) for c in range(3))
        if m < 0.01: dupes.append(f"{a}~{b}({m:.4f})")
check("no two looks are duplicates", not dupes, "; ".join(dupes))
closest = min(((max(abs(sig[a][k][c]-sig[b][k][c]) for k in range(6) for c in range(3)), a, b)
               for i,a in enumerate(looks) for b in looks[i+1:]))
print(f"        closest pair: {closest[1]} / {closest[2]}, max separation {closest[0]:.4f}")

print("\n--- 8. no stray Rec.709 assumption in the output encoding ---")
# A DI-encoded neutral ramp must follow the DI curve, not a Rec.709 curve.
s, d = luts['_Reference_Identity']
di_err = 0.0; r709_err = 0.0
for i in range(1, 40):
    x = i / 100.0
    if di_to_lin(x) > 1.0: break
    y = sum(sample(s, d, x, x, x)) / 3.0
    di_err += (y - x) ** 2
    r709_err += (y - rec709_encode(di_to_lin(x))) ** 2
check("output is DI-encoded, not Rec.709-encoded",
      di_err < r709_err / 100, f"DI residual {di_err:.2e} vs Rec.709 residual {r709_err:.2e}")

print("\n" + ("ALL CHECKS PASSED" if not fails else f"{len(fails)} FAILURES: " + "; ".join(fails)))
sys.exit(1 if fails else 0)
