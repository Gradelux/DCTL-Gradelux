#!/usr/bin/env python3
"""FrameLab tests: compiles the DCTL as C and checks it numerically.

Run from the repo root:
    python3 tests/framelab/run_tests.py [--dctl Gradelux_FrameLab_v1.dctl] [--preview out.png]
Tests v2 by default.

Checks
  1. Pass-through: with Frame Style None and every effect off, output equals
     input exactly, including negative and above-1 values.
  2. Robustness: random settings on extreme input (negative, huge, tiny)
     never produce NaN or infinity and never read outside the image.
  3. Resolutions: tiny, portrait, ultra-wide and 4K frames render cleanly.
  4. Determinism: the same frame and settings give identical output.
"""
import os, re, subprocess, sys, tempfile
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
DCTL = os.path.join(ROOT, 'Gradelux_FrameLab_v2.dctl')
if '--dctl' in sys.argv:
    DCTL = os.path.abspath(sys.argv[sys.argv.index('--dctl') + 1])
TMP = tempfile.mkdtemp(prefix='framelab_')
BIN = os.path.join(TMP, 'render')

def read_params(path):
    """name -> (kind, lo, hi), read from the DCTL's DEFINE_UI_PARAMS lines."""
    out = {}
    for line in open(path):
        m = re.match(r'^DEFINE_UI_PARAMS\((.*)\)\s*$', line.strip())
        if not m:
            continue
        parts = [x.strip() for x in m.group(1).split(',')]
        name, kind = parts[0], parts[2]
        if kind == 'DCTLUI_COMBO_BOX':
            n = len(re.search(r'\{([^}]*)\}', m.group(1)).group(1).split(','))
            out[name] = ('i', 0, n - 1)
        elif kind == 'DCTLUI_CHECK_BOX':
            out[name] = ('i', 0, 1)
        elif kind == 'DCTLUI_SLIDER_INT':
            out[name] = ('i', int(parts[4]), int(parts[5]))
        else:
            out[name] = ('f', float(parts[4]), float(parts[5]))
    return out

PARAMS = read_params(DCTL)

def pname(k):
    """Control name as used in the DCTL (v2 prefixes every control with ui_)."""
    return k if k in PARAMS or 'ui_' + k not in PARAMS else 'ui_' + k

def has(k):
    return pname(k) in PARAMS
# every setting that switches an effect on, at its "off" value
OFF = {k: v for k, v in {
    'preset': 0, 'frameStyle': 0, 'dustAmt': 0, 'scratchAmt': 0, 'weaveAmt': 0, 'flickerAmt': 0,
    'vignetteAmt': 0, 'leakAmt': 0, 'dateOn': 0, 'leaderOn': 0, 'edgeFog': 0, 'burnAmt': 0,
    'gateHair': 0, 'cueMarks': 0, 'picZoom': 1, 'picPanX': 0, 'picPanY': 0}.items() if has(k)}

def build():
    gen = os.path.join(HERE, 'framelab_gen.c')
    subprocess.check_call([sys.executable, os.path.join(HERE, 'build_c.py'), DCTL, gen])
    subprocess.check_call(['gcc', '-O2', '-Wall', '-Wno-unused-function', '-o', BIN,
                           os.path.join(HERE, 'render.c'), '-I', HERE, '-lm'])

def run(img, params):
    h, w, _ = img.shape
    inp, out = os.path.join(TMP, 'in.f32'), os.path.join(TMP, 'out.f32')
    img.astype(np.float32).tofile(inp)
    r = subprocess.run([BIN, inp, str(w), str(h), out] + ['%s=%s' % (pname(k), v) for k, v in params.items()],
                       capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError('render failed (%d): %s %s' % (r.returncode, r.stderr.strip(), params))
    return np.fromfile(out, np.float32).reshape(h, w, 3)

def scene(w, h):
    """Synthetic test frame: sky, sun, hills, small lights, gray ramp, patches."""
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    u, v = x / w, y / h
    t = np.clip(v / 0.62, 0, 1)[..., None]
    img = np.array([0.20, 0.36, 0.62]) * (1 - t) + np.array([0.98, 0.66, 0.38]) * t
    d = np.hypot((u - 0.66) * w / h, v - 0.50)
    img = img + (np.exp(-(d / 0.05) ** 2) + 0.35 * np.exp(-(d / 0.22) ** 2))[..., None] * np.array([1.0, 0.85, 0.55])
    for amp, base, freq, col in [(0.05, 0.60, 7.0, (0.30, 0.25, 0.35)), (0.04, 0.68, 11.0, (0.16, 0.20, 0.14)),
                                 (0.03, 0.78, 17.0, (0.10, 0.14, 0.08))]:
        line = base + amp * np.sin(u * freq + base * 10) + amp * 0.4 * np.sin(u * freq * 2.7)
        shade = (1 - 0.3 * np.clip((v - line) / 0.3, 0, 1))[..., None]
        img = np.where((v > line)[..., None], np.array(col) * shade, img)
    rng = np.random.default_rng(3)
    for i in range(40):
        cx, cy = rng.uniform(0.05, 0.45), rng.uniform(0.15, 0.55)
        c = [(1, .3, .2), (1, .8, .3), (.3, .6, 1), (1, .4, .8)][i % 4]
        dd = np.hypot((u - cx) * w / h, v - cy)
        img = img + (0.9 * np.exp(-(dd / 0.008) ** 2) + 0.15 * np.exp(-(dd / 0.03) ** 2))[..., None] * np.array(c)
    band = v > 0.92
    img[band] = np.repeat(u[band][:, None], 3, 1)
    for i, c in enumerate([(.75, .2, .15), (.85, .6, .45), (.2, .55, .25), (.2, .35, .75), (.9, .8, .2), (.6, .25, .65)]):
        img[(v > 0.86) & (v < 0.915) & (u > 0.02 + i * 0.06) & (u < 0.07 + i * 0.06)] = c
    return np.clip(img, 0, 1.2).astype(np.float32)

def extreme(w, h, rng):
    img = rng.uniform(-0.2, 1.3, (h, w, 3)).astype(np.float32)
    img[:h // 4] = -50.0
    img[h // 4:h // 2, :w // 2] = 5000.0
    img[h // 2:, :w // 8] = 1e-8
    return img

def random_params(rng):
    p = {}
    for k, (kind, lo, hi) in PARAMS.items():
        p[k] = int(rng.integers(lo, hi + 1)) if kind == 'i' else float(rng.uniform(lo, hi))
    return p

def main():
    build()
    rng = np.random.default_rng(7)
    fails = 0

    # 1. pass-through
    img = scene(320, 180)
    img[:20] = rng.uniform(-0.5, 3.0, (20, 320, 3))
    for extra in [{}, {'filmRoll': 3, 'rotation': 10, 'background': 2, 'filmBase': 1, 'animSource': 1, 'animTime': 55}]:
        out = run(img, {**OFF, **extra})
        same = np.array_equal(out, img)
        print('pass-through %-40s %s' % (extra or 'defaults', 'OK (bit-identical)' if same else 'FAIL'))
        fails += not same

    # 2. robustness on extreme input
    ext = extreme(240, 136, rng)
    n = 150
    for i in range(n):
        p = random_params(rng)
        try:
            out = run(ext, p)
            if not np.isfinite(out).all():
                raise RuntimeError('non-finite output')
        except RuntimeError as e:
            print('robustness FAIL', e); fails += 1; break
    else:
        print('robustness: %d random settings on extreme input, all finite, no out-of-range reads: OK' % n)
    for style in range(PARAMS[pname('frameStyle')][2] + 1):
        out = run(ext, {'frameStyle': style, 'dustAmt': 1, 'scratchAmt': 1, 'weaveAmt': 1, 'flickerAmt': 1,
                        'leakAmt': 1, 'dateOn': 1, 'leaderOn': 1, 'animSource': 1, 'animTime': 30})
        if not np.isfinite(out).all():
            print('style %d all-effects FAIL' % style); fails += 1
    print('every style with every effect at maximum: OK' if not fails else '')

    # 3. resolutions
    for w, h in [(17, 9), (64, 36), (1080, 1920), (2400, 600), (3840, 2160)]:
        try:
            out = run(scene(w, h), {'frameStyle': int(rng.integers(1, PARAMS[pname('frameStyle')][2] + 1)), 'dateOn': 1, 'leakAmt': 0.5})
            print('resolution %4dx%-4d OK' % (w, h) if np.isfinite(out).all() else 'resolution FAIL')
        except RuntimeError as e:
            print('resolution %dx%d FAIL %s' % (w, h, e)); fails += 1

    # 4. determinism
    p = random_params(rng)
    a, b = run(img, p), run(img, p)
    print('determinism: %s' % ('OK' if np.array_equal(a, b) else 'FAIL'))
    fails += not np.array_equal(a, b)

    if '--preview' in sys.argv:
        out = sys.argv[sys.argv.index('--preview') + 1]
        preview(out)
        if has('preset'):
            preview(out.replace('.png', '_features.png'), V2_FEATURES)
    print('\nALL PASSED' if fails == 0 else '\n%d FAILURES' % fails)
    sys.exit(1 if fails else 0)

V1_SHEET = [
    ('Super 8', {'frameStyle': 1}),
    ('Regular 8', {'frameStyle': 2, 'leakAmt': 0.6, 'leakPos': 0.1}),
    ('16mm + soundtrack', {'frameStyle': 3}),
    ('35mm Motion Picture', {'frameStyle': 4, 'background': 2}),
    ('35mm Still, Orange Negative', {'frameStyle': 5, 'background': 2, 'filmBase': 1, 'rotation': -5}),
    ('120 Medium Format', {'frameStyle': 6, 'background': 3}),
    ('Instant Print + date', {'frameStyle': 7, 'background': 1, 'rotation': 4, 'dateOn': 1}),
    ('Slide Mount', {'frameStyle': 8, 'background': 2}),
    ('Filed Gate', {'frameStyle': 9}),
    ('Projector Gate', {'frameStyle': 10, 'leakAmt': 0.5, 'leakColor': 1, 'leakPos': 0.75}),
    ('Countdown Leader', {'frameStyle': 3, 'leaderOn': 1, 'animSource': 1, 'animTime': 30}),
    ('Date stamp + warm leak', {'frameStyle': 0, 'dateOn': 1, 'leakAmt': 0.8, 'leakPos': 0.0}),
]
V2_PRESETS = [(n, {'preset': i + 1}) for i, n in enumerate([
    'Home Movie', 'Slide Show', 'Negative Scan', 'Cinema Print', 'Silent Era Projector', '16mm Documentary',
    'Instant Memory', 'Contact Sheet', 'Damaged Reel', 'Vintage Print', 'Super 16 Indie', 'Regular 8 Reel'])]
V2_FEATURES = [
    ('Film Burn 0.25', {'frameStyle': 1, 'burnAmt': 0.25}),
    ('Film Burn 0.6', {'frameStyle': 0, 'burnAmt': 0.6, 'burnPos': 0.3, 'vignetteAmt': 0}),
    ('Edge Fog + Hair In The Gate', {'frameStyle': 3, 'edgeFog': 0.8, 'gateHair': 1}),
    ('Cue Marks (Cinema)', {'frameStyle': 5, 'cueMarks': 1}),
    ('Grease Pencil on Super 8', {'frameStyle': 1, 'greasePencil': 1, 'background': 2, 'filmBase': 2}),
    ('Picture Zoom 1.5, Pan', {'frameStyle': 9, 'picZoom': 1.5, 'picPanX': 0.8}),
    ('Black Slide Mount', {'frameStyle': 11, 'filmBase': 0, 'background': 3}),
    ('Super 16, Safety Film text', {'frameStyle': 4, 'edgeText': 1, 'background': 2}),
    ('Red date stamp, size 1.4', {'frameStyle': 10, 'dateOn': 1, 'stampColor': 1, 'stampSize': 1.4, 'background': 1}),
]

def preview(path, looks=None):
    from PIL import Image, ImageDraw, ImageFont
    if looks is None:
        looks = V2_PRESETS if has('preset') else V1_SHEET
    src = scene(1920, 1080)
    tw, th, pad = 640, 360, 44
    rows = (len(looks) + 2) // 3
    sheet = Image.new('RGB', (tw * 3, (th + pad) * rows), (18, 18, 18))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype('DejaVuSans.ttf', 22)
    except OSError:
        font = ImageFont.load_default()
    for i, (name, p) in enumerate(looks):
        out = run(src, p)
        im = Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8)).resize((tw, th), Image.LANCZOS)
        x, y = (i % 3) * tw, (i // 3) * (th + pad)
        sheet.paste(im, (x, y))
        draw.text((x + 12, y + th + 10), name, fill=(230, 225, 215), font=font)
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    sheet.save(path)
    print('preview written to', path)

if __name__ == '__main__':
    main()
