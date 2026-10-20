#!/usr/bin/env python3
# =============================================================================
#  Gradelux FilmLab v9: Cine16 secondary engine regression
# -----------------------------------------------------------------------------
#  Compares numerical RGB output, sample by sample, bit for bit (float32):
#
#   A. STANDALONE MATCH. The original Cine16 DCTL (reference/) against FilmLab v9
#      with Primary Film Look Off, Secondary Film Look = the same profile,
#      Film Print None and every other FilmLab control neutral, for all 16
#      profiles. Inputs: RGB patch grid, greyscale ramp, skin tones, saturated
#      colours, extended-range values and random pixels. Run with the
#      original defaults, random slider settings within the original ranges,
#      slider extremes, and Show Skin Mask.
#   B. SECONDARY OFF. (1) FilmLab v9 with Secondary Film Look Off and the
#      secondary sliders / skin-mask switch set randomly against the same with
#      the sliders at default, for all 18 looks + Off and all 6 print options
#      (the secondary controls must be inert). (2) With Film Look Off, v9
#      against v8 (Version 9 changes only the primary looks, so every print
#      and Look-Off configuration must match v8 bit-for-bit).
#   C. CHAINING. FilmLab v9 (Primary X + Secondary Y + Print Z) against the
#      three engines run as separate programs: v9 (Look X, no print) ->
#      original Cine16 (profile Y) -> v9 (Look Off, Print Z). This confirms
#      the processing order and that each engine is unchanged in the chain.
#   D. SANITY. Profile N against the wrong profile N+1 must differ, proving the
#      comparison can detect a change.
#
#    cd tests && python3 cine16_regression.py                 (FilmLab v9)
#    cd tests && FILMLAB=9.1 python3 cine16_regression.py     (FilmLab v9.1: only Secondary Strength
#       remains adjustable, so slider variation covers strength only; the skin-mask
#       view no longer exists)
#    cd tests && FILMLAB=10 python3 cine16_regression.py      (FilmLab v10: as v9.1, no prints:
#       print option None only)
#  Exit status 0 only if every comparison is bit-identical.
# =============================================================================
import os, subprocess, sys, tempfile, random, math
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
PROFILES = ["Neutral", "Warm", "Cool", "Pastel", "Rich", "Golden", "Faded", "Reversal", "Night",
            "Daylight", "Portrait", "Chrome", "Emerald", "Steel Chrome", "Vivid", "Cinema E"]
LOOKS = ["Off", "Verona", "Verdant", "Midnight", "Aurelia", "Solara", "Moss", "Chroma", "Silvergrain",
         "Tungsten", "Halcyon", "Argent", "Nocturne", "Pastelle", "Patina", "Opaline", "Glacier",
         "Terracotta", "Velour"]
DEFAULTS = [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]                 # strength sep sat subtr bias tone skinprot
RANGES = [(0, 1.5), (0, 2), (0, 2), (0, 2), (0, 2), (0, 1.5), (0, 1)]

def f32(x):
    return float(np.float32(x))

def hx(x):
    return f32(x).hex()

# ---- build runners ----------------------------------------------------------
tmp = tempfile.mkdtemp(prefix='c16reg_')
def build(macro, name):
    exe = os.path.join(tmp, name)
    subprocess.check_call(['gcc', '-O2', '-w', '-D' + macro, '-o', exe, os.path.join(HERE, 'runner.c'), '-lm'])
    return exe
V10 = os.environ.get('FILMLAB', '9') == '10'
V91 = os.environ.get('FILMLAB', '9') == '9.1' or V10        # v10 keeps the v9.1 secondary controls
FLNAME = 'v10' if V10 else 'v9.1' if V91 else 'v9'
NPR = 1 if V10 else 6                                          # print options (v10: None only)
RUN_C16, RUN_V8 = build('RUN_CINE16', 'run_c16'), build('RUN_FILMLAB8', 'run_v8')
RUN_FL = (build('RUN_FILMLAB10', 'run_v10') if V10 else build('RUN_FILMLAB91', 'run_v91') if V91
          else build('RUN_FILMLAB9', 'run_v9'))
def rand_sliders():
    # v9.1 keeps only Secondary Strength; the other six stay at the original defaults
    if V91: return [hx(rng.uniform(*RANGES[0]))] + [hx(v) for v in DEFAULTS[1:]]
    return [hx(rng.uniform(lo, hi)) for lo, hi in RANGES]

def run(exe, lines):
    out = subprocess.run([exe], input='\n'.join(lines) + '\n', capture_output=True, text=True, check=True).stdout
    return [tuple(float.fromhex(t) for t in l.split()) for l in out.strip().split('\n')]

# ---- input sets (DaVinci Intermediate code values) ---------------------------
# linear Rec.709 -> linear DWG, and DI encode, for building scene-like colours
M709_TO_DWG = np.linalg.inv(np.array([[1.8986148993, -0.7921761834, -0.1064387159],
                                      [-0.1689487865, 1.4889757541, -0.3200269676],
                                      [-0.1215391606, -0.3156758531, 1.4372150137]]))
def lin_to_di(x):
    return x * 10.44426855 if x <= 0.00262409 else (math.log2(x + 0.0075) + 7.0) * 0.07329248
def from709(r, g, b):
    d = M709_TO_DWG @ np.array([r, g, b])
    return tuple(lin_to_di(v) for v in d)
def s2l(c):
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

rng = random.Random(16)
SETS = {}
SETS['rgb patches'] = [(r / 16, g / 16, b / 16) for r in range(17) for g in range(17) for b in range(17)]
SETS['grey ramp'] = [(v, v, v) for v in (-0.1 + 1.4 * i / 1023 for i in range(1024))]
skins = [(255, 224, 204), (241, 194, 167), (224, 172, 140), (194, 150, 130), (141, 85, 36), (92, 58, 40),
         (233, 189, 165), (175, 120, 95), (120, 75, 55), (210, 160, 125)]
SETS['skin tones'] = [from709(s2l(r) * m, s2l(g) * m, s2l(b) * m) for (r, g, b) in skins
                      for m in (2 ** (e / 4) for e in range(-16, 13))]
sat = []
for e in (-2, -1, 0, 1, 2):
    for s in (0.6, 0.9, 1.0):
        for h in range(0, 360, 3):
            hh = h / 60; k = int(hh) % 6; f = hh - int(hh); v = 0.35 * 2 ** e
            p, q, t = v * (1 - s), v * (1 - s * f), v * (1 - s * (1 - f))
            rgb = [(v, t, p), (q, v, p), (p, v, t), (p, q, v), (t, p, v), (v, p, q)][k]
            sat.append(from709(*rgb))
for P in [(1, 0, 0), (0, 1, 0), (0, 0, 1), (1, 1, 0), (0, 1, 1), (1, 0, 1)]:
    for e in range(-6, 5):
        m = 0.18 * 2 ** e
        sat.append(tuple(lin_to_di(c * m + 0.001) for c in P))        # DWG primaries / secondaries
SETS['saturated'] = sat
ext = [(rng.uniform(-0.6, 2.6), rng.uniform(-0.6, 2.6), rng.uniform(-0.6, 2.6)) for _ in range(3000)]
ext += [(v, v, v) for v in (-0.6 + 3.2 * i / 399 for i in range(400))]
ext += [(rng.uniform(1.0, 2.6), rng.uniform(-0.6, 0.3), rng.uniform(-0.6, 2.6)) for _ in range(600)]
SETS['extended range'] = ext
SETS['random pixels'] = [(rng.random(), rng.random(), rng.random()) for _ in range(4000)]
ALL = [(cat, rgb) for cat in SETS for rgb in SETS[cat]]

fails = 0
def compare(a, b):
    """bit-identical count, mismatches, max abs difference"""
    mism, mx = 0, 0.0
    for x, y in zip(a, b):
        if x != y:
            # NaN-safe equality: identical NaN payloads print the same
            if all((math.isnan(u) and math.isnan(v)) or u == v for u, v in zip(x, y)):
                continue
            mism += 1
            mx = max(mx, max(abs(u - v) for u, v in zip(x, y)))
    return mism, mx

# ---- A. standalone match -------------------------------------------------------
print('A. STANDALONE MATCH: original Cine16 DCTL vs FilmLab %s (Primary Off, Secondary = profile, Print None)' % FLNAME)
print('   counts are samples whose RGB output differs in any bit (float32)\n')
cats = list(SETS) + ['random sliders', 'slider extremes', 'show skin mask']
print('   %-14s' % 'profile' + ''.join(' %15s' % c for c in cats))
totalA = 0
for p, name in enumerate(PROFILES):
    row = []
    # defaults on every input set
    for cat in SETS:
        pts = SETS[cat]
        la = ['%d %s 0 %s %s %s' % (p, ' '.join(hx(v) for v in DEFAULTS), hx(r), hx(g), hx(b)) for r, g, b in pts]
        lb = ['0 %d 0 %s 0 %s %s %s' % (p + 1, ' '.join(hx(v) for v in DEFAULTS), hx(r), hx(g), hx(b)) for r, g, b in pts]
        m, mx = compare(run(RUN_C16, la), run(RUN_FL, lb)); row.append((m, len(pts), mx))
    # random sliders within the original ranges, on random inputs from all sets
    la, lb = [], []
    for i in range(4000):
        sl = rand_sliders()
        cat, (r, g, b) = ALL[rng.randrange(len(ALL))]
        la.append('%d %s 0 %s %s %s' % (p, ' '.join(sl), hx(r), hx(g), hx(b)))
        lb.append('0 %d 0 %s 0 %s %s %s' % (p + 1, ' '.join(sl), hx(r), hx(g), hx(b)))
    m, mx = compare(run(RUN_C16, la), run(RUN_FL, lb)); row.append((m, len(la), mx))
    # slider extremes: every slider at its min or max (128 corners) on a subset
    la, lb = [], []
    sub = SETS['rgb patches'][::37] + SETS['skin tones'][::7] + SETS['extended range'][::97]
    for corner in range(2 if V91 else 128):
        sl = ([hx(RANGES[0][corner])] + [hx(v) for v in DEFAULTS[1:]]) if V91 else [hx(RANGES[j][(corner >> j) & 1]) for j in range(7)]
        for r, g, b in sub:
            la.append('%d %s 0 %s %s %s' % (p, ' '.join(sl), hx(r), hx(g), hx(b)))
            lb.append('0 %d 0 %s 0 %s %s %s' % (p + 1, ' '.join(sl), hx(r), hx(g), hx(b)))
    m, mx = compare(run(RUN_C16, la), run(RUN_FL, lb)); row.append((m, len(la), mx))
    # Show Skin Mask (removed from v9.1: skipped)
    pts = [] if V91 else SETS['skin tones'] + SETS['rgb patches'][::5]
    la = ['%d %s 1 %s %s %s' % (p, ' '.join(hx(v) for v in DEFAULTS), hx(r), hx(g), hx(b)) for r, g, b in pts]
    lb = ['0 %d 0 %s 1 %s %s %s' % (p + 1, ' '.join(hx(v) for v in DEFAULTS), hx(r), hx(g), hx(b)) for r, g, b in pts]
    m, mx = compare(run(RUN_C16, la), run(RUN_FL, lb)) if pts else (0, 0.0); row.append((m, len(pts), mx))
    print('   %-14s' % name + ''.join(' %8d/%-6d' % (m, n) for m, n, mx in row))
    totalA += sum(m for m, n, mx in row)
nA = 16 * (sum(len(v) for v in SETS.values()) + 4000 + (2 if V91 else 128) * len(sub) + len(pts))
print('\n   total differing samples: %d of %d  -> %s\n' % (totalA, nA, 'BIT-IDENTICAL' if totalA == 0 else 'DIFFERENCES FOUND'))
fails += totalA

# ---- B. secondary Off -----------------------------------------------------------
print('B. SECONDARY OFF: (1) secondary controls inert when Off; (2) with Film Look Off, %s = v8' % FLNAME)
D = ' '.join(hx(v) for v in DEFAULTS)
la, lb = [], []
pts = SETS['rgb patches'][::3] + SETS['grey ramp'][::4] + SETS['skin tones'] + SETS['saturated'][::3] + SETS['extended range'][::5]
for look in range(len(LOOKS)):
    for pr in range(NPR):
        for r, g, b in pts:
            sl = rand_sliders()
            la.append('%d 0 %d %s 0 %s %s %s' % (look, pr, D, hx(r), hx(g), hx(b)))
            lb.append('%d 0 %d %s %d %s %s %s' % (look, pr, ' '.join(sl), rng.randrange(2), hx(r), hx(g), hx(b)))
m1, mx = compare(run(RUN_FL, la), run(RUN_FL, lb))
print('   (1) %d looks x %d print options x %d inputs = %d samples; differing: %d -> %s'
      % (len(LOOKS), NPR, len(pts), len(la), m1, 'BIT-IDENTICAL' if m1 == 0 else 'DIFFERENCES FOUND'))
la = []
for sec in range(17):
    for pr in range(NPR):
        for r, g, b in pts[::2]:
            sl = rand_sliders()
            la.append('0 %d %d %s 0 %s %s %s' % (sec, pr, ' '.join(sl), hx(r), hx(g), hx(b)))
m2, mx = compare(run(RUN_V8, la), run(RUN_FL, la))
print('   (2) Film Look Off, 17 secondary options x %d print options, random secondary sliders: %d samples; differing: %d -> %s\n'
      % (NPR, len(la), m2, 'BIT-IDENTICAL' if m2 == 0 else 'DIFFERENCES FOUND'))
fails += m1 + m2

# ---- C. chaining ----------------------------------------------------------------
if V10:
    print('C. CHAINING: v10 (Primary + Secondary) vs v10 primary -> original Cine16, as separate programs (third stage: v10 all Off = identity)')
else:
    print('C. CHAINING: %s (Primary + Secondary + Print) vs %s primary -> original Cine16 -> %s print, as separate programs' % (FLNAME, FLNAME, FLNAME))
pts = SETS['rgb patches'][::11] + SETS['skin tones'][::3] + SETS['saturated'][::9] + SETS['grey ramp'][::16]
combos = [(0, 6, 0), (1, 6, 0), (1, 6, 1 if NPR > 1 else 0)]                # the three requested combinations
combos += [(rng.randrange(len(LOOKS)), rng.randrange(1, 17), rng.randrange(NPR)) for _ in range(60)]
totC, nC = 0, 0
for look, sec, pr in combos:
    s1 = run(RUN_FL, ['%d 0 0 %s 0 %s %s %s' % (look, D, hx(r), hx(g), hx(b)) for r, g, b in pts])
    s2 = run(RUN_C16, ['%d %s 0 %s %s %s' % (sec - 1, D, x[0].hex(), x[1].hex(), x[2].hex()) for x in s1])
    s3 = run(RUN_FL, ['0 0 %d %s 0 %s %s %s' % (pr, D, x[0].hex(), x[1].hex(), x[2].hex()) for x in s2])
    v9 = run(RUN_FL, ['%d %d %d %s 0 %s %s %s' % (look, sec, pr, D, hx(r), hx(g), hx(b)) for r, g, b in pts])
    m, mx = compare(s3, v9); totC += m; nC += len(pts)
    if (look, sec, pr) in combos[:3]:
        print('   Primary %-13s / Secondary %-8s / Print %-15s : %d of %d differ'
              % (LOOKS[look], PROFILES[sec - 1], ['None', 'Cinechrome 2383', 'Velachrome 2393', 'Lumachrome 3513',
                                                    'Retrochrome 3510', 'Silkchrome 2383'][pr], m, len(pts)))
print('   + %d random primary / secondary / print combinations' % (len(combos) - 3))
print('   total: %d of %d samples differ -> %s\n' % (totC, nC, 'BIT-IDENTICAL' if totC == 0 else 'DIFFERENCES FOUND'))
fails += totC

# ---- D. sanity: the comparison must detect a wrong profile ---------------------
print('D. SANITY: original profile N vs %s profile N+1 (deliberately wrong) must differ' % FLNAME)
pts = SETS['rgb patches'][::7] + SETS['skin tones']
bad = 0
for p in range(16):
    q = (p + 1) % 16
    la = ['%d %s 0 %s %s %s' % (p, ' '.join(hx(v) for v in DEFAULTS), hx(r), hx(g), hx(b)) for r, g, b in pts]
    lb = ['0 %d 0 %s 0 %s %s %s' % (q + 1, ' '.join(hx(v) for v in DEFAULTS), hx(r), hx(g), hx(b)) for r, g, b in pts]
    m, mx = compare(run(RUN_C16, la), run(RUN_FL, lb))
    if m == 0: bad += 1
    print('   %-13s vs %-13s: %4d of %d differ (max %.4f DI)' % (PROFILES[p], PROFILES[q], m, len(pts), mx))
print('   -> %s\n' % ('comparison detects profile changes' if bad == 0 else 'WARNING: %d wrong pairs not detected' % bad))
fails += bad

print('ALL CINE16 REGRESSIONS BIT-IDENTICAL' if fails == 0 else 'REGRESSION DIFFERENCES: %d' % fails)
sys.exit(1 if fails else 0)
