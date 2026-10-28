#!/usr/bin/env python3
"""Tests every Film Transition DCTL by compiling its own code as C and OpenCL.

  - text rules that keep Resolve's reader happy (ASCII, no quotes, one return)
  - OpenCL compile, like a GPU back end
  - at progress 0 the output equals the outgoing clip, at 1 the incoming clip
  - extreme input never gives NaN / infinity
  - optional --preview DIR renders a filmstrip of each transition

Usage: python3 tools/filmtransitions/test.py [--from a.f32 --to b.f32 --preview DIR]
       (.f32 files: 1920x1080 float32 RGB)
"""
import glob, os, re, subprocess, sys, tempfile
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TMP = tempfile.mkdtemp(prefix='transitions_')

C_SHIM = r'''
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct { float x, y, z; } float3;
typedef struct { float x, y, z, w; } float4;
static inline float3 make_float3(float a, float b, float c) { float3 r = {a, b, c}; return r; }
static inline float4 make_float4(float a, float b, float c, float d) { float4 r = {a, b, c, d}; return r; }
typedef struct { const float *d; int w, h; } Tex;
#define __TEXTURE__ Tex
#define __DEVICE__ static inline
#define __CONSTANT__ static const
static inline float _tex2D(Tex t, int x, int y) {
    if (x < 0 || y < 0 || x >= t.w || y >= t.h) { fprintf(stderr, "tex out of range\n"); exit(3); }
    return t.d[y * t.w + x];
}
#define _sinf sinf
#define _cosf cosf
#define _expf expf
#define _sqrtf sqrtf
#define _powf powf
#define _fabs fabsf
#define _fminf fminf
#define _fmaxf fmaxf
#define _atan2f atan2f
static float TRANSITION_PROGRESS = 0.0f;
'''

C_MAIN = r'''
static float *readf(const char *p, size_t n) {
    float *b = malloc(n * sizeof(float)); FILE *f = fopen(p, "rb");
    if (!f || fread(b, sizeof(float), n, f) != n) { fprintf(stderr, "read %s\n", p); exit(1); }
    fclose(f); return b;
}
int main(int argc, char **argv) {
    // usage: from.f32 to.f32 W H progress out.f32
    int W = atoi(argv[3]), H = atoi(argv[4]);
    TRANSITION_PROGRESS = (float)atof(argv[5]);
    size_t n = (size_t)W * H;
    float *a = readf(argv[1], n * 3), *b = readf(argv[2], n * 3);
    float *ch[8], *ones = malloc(n * 4);
    for (int c = 0; c < 8; c++) ch[c] = malloc(n * 4);
    for (size_t i = 0; i < n; i++) {
        ones[i] = 1.0f;
        for (int c = 0; c < 3; c++) { ch[c][i] = a[3 * i + c]; ch[4 + c][i] = b[3 * i + c]; }
        ch[3][i] = 1.0f; ch[7][i] = 1.0f;
    }
    Tex t[8];
    for (int c = 0; c < 8; c++) { t[c].d = ch[c]; t[c].w = W; t[c].h = H; }
    float *out = malloc(n * 12); long bad = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        float4 r = transition(W, H, x, y, t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7]);
        size_t i = (size_t)y * W + x;
        if (!isfinite(r.x) || !isfinite(r.y) || !isfinite(r.z) || !isfinite(r.w)) bad++;
        out[3 * i] = r.x; out[3 * i + 1] = r.y; out[3 * i + 2] = r.z;
    }
    FILE *f = fopen(argv[6], "wb"); fwrite(out, 4, n * 3, f); fclose(f);
    if (bad) { fprintf(stderr, "NONFINITE %ld\n", bad); return 4; }
    return 0;
}
'''

CL_SHIM = r'''
#define __DEVICE__
#define __CONSTANT__ __constant
#define __TEXTURE__ __read_only image2d_t
__constant sampler_t glx_smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;
#define _tex2D(t_, px_, py_) read_imagef((t_), glx_smp, (int2)((px_), (py_))).x
#define make_float3(a, b, c) ((float3)((a), (b), (c)))
#define make_float4(a, b, c, d) ((float4)((a), (b), (c), (d)))
#define _sinf sin
#define _cosf cos
#define _expf exp
#define _sqrtf sqrt
#define _powf pow
#define _fabs fabs
#define _fminf fmin
#define _fmaxf fmax
#define _atan2f atan2
__constant float glx_progress = 0.5f;
#define TRANSITION_PROGRESS (glx_progress)
'''

CL_KERNEL = r'''
__kernel void glx_kernel(__read_only image2d_t a, __read_only image2d_t b, __read_only image2d_t c, __read_only image2d_t d,
                         __read_only image2d_t e, __read_only image2d_t f, __read_only image2d_t g, __read_only image2d_t h,
                         __global float *out)
{
    int x = get_global_id(0), y = get_global_id(1);
    float4 r = transition(1920, 1080, x, y, a, b, c, d, e, f, g, h);
    out[(y * 1920 + x) * 4] = r.x;
}
'''


def lint(path):
    raw = open(path, 'rb').read()
    probs = []
    if b'\r' in raw: probs.append('Windows line endings')
    if any(c > 127 for c in raw): probs.append('non-ASCII characters')
    text = raw.decode('ascii', 'replace')
    if "'" in text or '"' in text: probs.append('quote characters')
    if text.count('transition(') != 1: probs.append('transition( must appear exactly once')
    main = text[text.index('float4 transition('):]
    if len(re.findall(r'\breturn\b', main)) != 1: probs.append('transition must have one return')
    return probs


def build(path):
    name = os.path.basename(path)[:-5]
    src = open(path).read()
    c = os.path.join(TMP, name + '.c')
    open(c, 'w').write(C_SHIM + src + C_MAIN)
    exe = os.path.join(TMP, name)
    subprocess.check_call(['gcc', '-O2', '-Wall', '-Wno-unused-function', '-o', exe, c, '-lm'])
    cl = os.path.join(TMP, name + '.cl')
    open(cl, 'w').write(CL_SHIM + src + CL_KERNEL)
    r = subprocess.run(['clang', '-x', 'cl', '-cl-std=CL1.2', '-fsyntax-only', '-Wall', '-Wdouble-promotion', cl],
                       capture_output=True, text=True)
    return exe, (r.stderr.strip() if (r.returncode or r.stderr.strip()) else '')


def run(exe, a, b, prog):
    h, w, _ = a.shape
    pa, pb, po = [os.path.join(TMP, n) for n in ('a.f32', 'b.f32', 'o.f32')]
    a.astype(np.float32).tofile(pa); b.astype(np.float32).tofile(pb)
    r = subprocess.run([exe, pa, pb, str(w), str(h), '%.6f' % prog, po], capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(r.stderr.strip())
    return np.fromfile(po, np.float32).reshape(h, w, 3)


def main():
    args = sys.argv[1:]
    get = lambda k: args[args.index(k) + 1] if k in args else None
    rng = np.random.default_rng(3)
    A = rng.uniform(0, 1, (90, 160, 3)).astype(np.float32)
    B = rng.uniform(0, 1, (90, 160, 3)).astype(np.float32)
    ext = rng.uniform(-0.3, 1.5, (90, 160, 3)).astype(np.float32); ext[:20] = -40.0; ext[20:40] = 3000.0
    fails = 0
    previews = []
    for path in sorted(glob.glob(os.path.join(ROOT, 'FilmTransitions', '*.dctl'))):
        name = os.path.basename(path)[:-5]
        probs = lint(path)
        exe, clerr = build(path)
        if clerr: probs.append('OpenCL: ' + clerr.splitlines()[0])
        if not np.allclose(run(exe, A, B, 0.0), A, atol=1e-6): probs.append('progress 0 is not the outgoing clip')
        if not np.allclose(run(exe, A, B, 1.0), B, atol=1e-6): probs.append('progress 1 is not the incoming clip')
        for p in np.linspace(0, 1, 21):
            if not np.isfinite(run(exe, ext, A, p)).all():
                probs.append('non-finite at %.2f' % p); break
        print('%-36s %s' % (name, 'OK' if not probs else 'FAIL: ' + '; '.join(probs)))
        fails += bool(probs)
        previews.append((name, exe))
    if get('--preview'):
        preview(previews, get('--from'), get('--to'), get('--preview'))
    print('\nALL PASSED' if not fails else '\n%d FAILED' % fails)
    sys.exit(1 if fails else 0)


def preview(items, fa, fb, outdir):
    from PIL import Image, ImageDraw, ImageFont
    os.makedirs(outdir, exist_ok=True)
    a = np.fromfile(fa, np.float32).reshape(1080, 1920, 3)
    b = np.fromfile(fb, np.float32).reshape(1080, 1920, 3)
    steps = [0.15, 0.3, 0.42, 0.5, 0.56, 0.7, 0.85]
    tw, th = 320, 180
    font = ImageFont.truetype('DejaVuSans.ttf', 18)
    rows = []
    for name, exe in items:
        row = Image.new('RGB', (tw * len(steps), th + 30), (15, 15, 15))
        dr = ImageDraw.Draw(row)
        for j, p in enumerate(steps):
            o = run(exe, a, b, p)
            im = Image.fromarray((np.clip(o, 0, 1) * 255 + 0.5).astype(np.uint8)).resize((tw, th), Image.LANCZOS)
            row.paste(im, (j * tw, 30))
            dr.text((j * tw + 6, 6), '%d%%' % round(p * 100), fill=(160, 160, 160), font=font)
        dr.text((tw * len(steps) - 330, 6), name.replace('Gradelux_Transition_', ''), fill=(230, 225, 215), font=font)
        row.save(os.path.join(outdir, name + '.png'))
        rows.append(row)
    sheet = Image.new('RGB', (rows[0].width, sum(r.height for r in rows)))
    y = 0
    for r in rows:
        sheet.paste(r, (0, y)); y += r.height
    sheet.save(os.path.join(outdir, 'all.png'))
    print('preview written to', outdir)


if __name__ == '__main__':
    main()
