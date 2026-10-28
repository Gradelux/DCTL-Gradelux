#!/usr/bin/env python3
"""Tests every Film Matte DCTL by compiling its own code as C and OpenCL.

  - Resolve text rules (plain labels, no quotes, unique control names, ...)
  - OpenCL compile with every control as a text macro (like Resolve)
  - Opacity 0 returns the input bit-for-bit
  - random settings on extreme input never give NaN / infinity
  - odd resolutions render cleanly

Usage: python3 tools/filmborders/test.py   (from the repo root)
"""
import glob, os, subprocess, sys, tempfile
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tests', 'framelab'))
sys.argv = sys.argv[:1]
import run_tests as rt

TMP = tempfile.mkdtemp(prefix='mattes_')
fails = 0
rng = np.random.default_rng(5)
for path in sorted(glob.glob(os.path.join(ROOT, 'FilmMattes', '*.dctl'))):
    name = os.path.basename(path)[:-5]
    probs = rt.lint(path)
    cl = os.path.join(TMP, name + '.cl')
    subprocess.check_call([sys.executable, os.path.join(ROOT, 'tests', 'framelab', 'build_cl.py'), path, cl])
    r = subprocess.run(['clang', '-x', 'cl', '-cl-std=CL1.2', '-fsyntax-only', '-Wall', '-Wdouble-promotion', cl],
                       capture_output=True, text=True)
    if r.returncode or r.stderr.strip():
        probs.append('OpenCL: ' + r.stderr.strip().splitlines()[0])
    gen = os.path.join(TMP, name + '_gen.c')
    subprocess.check_call([sys.executable, os.path.join(ROOT, 'tests', 'framelab', 'build_c.py'), path, gen])
    src = open(os.path.join(ROOT, 'tests', 'framelab', 'render.c')).read().replace('#include "framelab_gen.c"', '#include "%s"' % gen)
    open(os.path.join(TMP, name + '.c'), 'w').write(src)
    rt.BIN = os.path.join(TMP, name)
    subprocess.check_call(['gcc', '-O2', '-w', '-o', rt.BIN, os.path.join(TMP, name + '.c'), '-lm'])
    rt.PARAMS = rt.read_params(path)
    img = rt.scene(320, 180)
    img[:15] = rng.uniform(-0.5, 3.0, (15, 320, 3))
    if not np.array_equal(rt.run(img, {'ui_mix': 0}), img):
        probs.append('Opacity 0 is not an exact pass-through')
    ext = rt.extreme(200, 112, rng)
    for i in range(60):
        if not np.isfinite(rt.run(ext, rt.random_params(rng))).all():
            probs.append('non-finite output'); break
    for w, h in [(17, 9), (1080, 1920), (2400, 600), (3840, 2160)]:
        if not np.isfinite(rt.run(rt.scene(w, h), {})).all():
            probs.append('bad output at %dx%d' % (w, h))
    print('%-28s %s' % (name, 'OK' if not probs else 'FAIL: ' + '; '.join(probs)))
    fails += bool(probs)
print('\nALL PASSED' if not fails else '\n%d FAILED' % fails)
sys.exit(1 if fails else 0)
