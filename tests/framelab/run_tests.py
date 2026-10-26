#!/usr/bin/env python3
"""FrameLab tests: compiles the DCTL as C and checks it numerically.

Run from the repo root:  python3 tests/framelab/run_tests.py [--preview out.png]

Checks
  1. Pass-through: with Frame Style None and every effect off, output equals
     input exactly, including negative and above-1 values.
  2. Robustness: random settings on extreme input (negative, huge, tiny)
     never produce NaN or infinity and never read outside the image.
  3. Resolutions: tiny, portrait, ultra-wide and 4K frames render cleanly.
  4. Determinism: the same frame and settings give identical output.
"""
import os, subprocess, sys, tempfile
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
DCTL = os.path.join(ROOT, 'Gradelux_FrameLab_v1.dctl')
TMP = tempfile.mkdtemp(prefix='framelab_')
BIN = os.path.join(TMP, 'render')

PARAMS = {  # name: (kind, lo, hi)
    'frameStyle': ('i', 0, 10), 'filmBase': ('i', 0, 3), 'background': ('i', 0, 3),
    'frameSize': ('f', 0.5, 1.5), 'filmRoll': ('f', -10, 10), 'rotation': ('f', -20, 20),
    'cornerRound': ('f', 0, 2), 'gateSoft': ('f', 0, 1), 'roughness': ('f', 0, 1),
    'edgeMarks': ('i', 0, 1), 'soundtrack': ('i', 0, 1), 'dustAmt': ('f', 0, 1),
    'scratchAmt': ('f', 0, 1), 'weaveAmt': ('f', 0, 1), 'flickerAmt': ('f', 0, 1),
    'vignetteAmt': ('f', 0, 1), 'leakAmt': ('f', 0, 1), 'leakColor': ('i', 0, 3),
    'leakPos': ('f', -1, 1), 'leakSize': ('f', 0.2, 2), 'dateOn': ('i', 0, 1),
    'dateFmt': ('i', 0, 2), 'dateYear': ('i', 0, 99), 'dateMonth': ('i', 1, 12),
    'dateDay': ('i', 1, 31), 'stampSize': ('f', 0.5, 2), 'leaderOn': ('i', 0, 1),
    'leaderFps': ('i', 12, 60), 'animSource': ('i', 0, 1), 'animTime': ('f', 0, 10000),
    'variation': ('i', 0, 100),
}
OFF = {'frameStyle': 0, 'dustAmt': 0, 'scratchAmt': 0, 'weaveAmt': 0, 'flickerAmt': 0,
       'vignetteAmt': 0, 'leakAmt': 0, 'dateOn': 0, 'leaderOn': 0}

def build():
    gen = os.path.join(HERE, 'framelab_gen.c')
    subprocess.check_call([sys.executable, os.path.join(HERE, 'build_c.py'), DCTL, gen])
    subprocess.check_call(['gcc', '-O2', '-Wall', '-Wno-unused-function', '-o', BIN,
                           os.path.join(HERE, 'render.c'), '-I', HERE, '-lm'])

def run(img, params):
    h, w, _ = img.shape
    inp, out = os.path.join(TMP, 'in.f32'), os.path.join(TMP, 'out.f32')
    img.astype(np.float32).tofile(inp)
    r = subprocess.run([BIN, inp, str(w), str(h), out] + ['%s=%s' % kv for kv in params.items()],
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
    for style in range(11):
        out = run(ext, {'frameStyle': style, 'dustAmt': 1, 'scratchAmt': 1, 'weaveAmt': 1, 'flickerAmt': 1,
                        'leakAmt': 1, 'dateOn': 1, 'leaderOn': 1, 'animSource': 1, 'animTime': 30})
        if not np.isfinite(out).all():
            print('style %d all-effects FAIL' % style); fails += 1
    print('every style with every effect at maximum: OK' if not fails else '')

    # 3. resolutions
    for w, h in [(17, 9), (64, 36), (1080, 1920), (2400, 600), (3840, 2160)]:
        try:
            out = run(scene(w, h), {'frameStyle': int(rng.integers(1, 11)), 'dateOn': 1, 'leakAmt': 0.5})
            print('resolution %4dx%-4d OK' % (w, h) if np.isfinite(out).all() else 'resolution FAIL')
        except RuntimeError as e:
            print('resolution %dx%d FAIL %s' % (w, h, e)); fails += 1

    # 4. determinism
    p = random_params(rng)
    a, b = run(img, p), run(img, p)
    print('determinism: %s' % ('OK' if np.array_equal(a, b) else 'FAIL'))
    fails += not np.array_equal(a, b)

    if '--preview' in sys.argv:
        preview(sys.argv[sys.argv.index('--preview') + 1])
    print('\nALL PASSED' if fails == 0 else '\n%d FAILURES' % fails)
    sys.exit(1 if fails else 0)

def preview(path):
    from PIL import Image, ImageDraw, ImageFont
    src = scene(1920, 1080)
    looks = [
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
    tw, th, pad = 640, 360, 44
    sheet = Image.new('RGB', (tw * 3, (th + pad) * 4), (18, 18, 18))
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
