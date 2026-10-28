#!/usr/bin/env python3
"""Gradelux 35mm Film Burns: procedural film-burn transition renderer.

Pipeline, per frame:
  1. Coherent noise: 3D (x, y, time) value noise, summed over octaves (fBm)
     and domain-warped, so shapes grow and morph smoothly over time.
  2. Exposure layers: each burn layer is light travelling from a source (a
     frame edge or a point). Its front is pushed around by warped noise, so the
     boundary is irregular, and its softness varies over the frame, so one burn
     has soft and hard edges. Layers carry a hot core, a leading glow and small
     secondary leaks near the front.
  3. Film response: exposure becomes colour through per-layer characteristic
     curves. The red layer saturates first, then green, then blue, so light
     goes deep red > crimson > orange > amber > golden > pale yellow > cream.
     Red spreads wider than the other layers (halation), giving the thin red
     rim at the edge of a burn. Bright areas bloom slightly.
  4. Film texture at full resolution: photographic grain that follows
     exposure (none in pure black), frame-to-frame exposure flicker, gate
     weave, dust and optional scratches.
  5. Output: 16-bit frames piped to ffmpeg as ProRes 422 HQ (black
     background, for Screen or Add) and optionally ProRes 4444 with alpha.

Usage:
  python3 tools/filmburns/burns.py OUT_DIR --only NAME [--size 3840x2160] [--work 1280x720]
         [--preview] [--alpha] [--no-mov]
"""
import os, subprocess, sys
import numpy as np
from PIL import Image

import presets as P


# =============================================================================
#  Noise
# =============================================================================

class Noise:
    def __init__(self, seed):
        rng = np.random.default_rng(seed)
        self.table = rng.random(1 << 20).astype(np.float32)
        self.mask = (1 << 20) - 1

    def _h(self, ix, iy, it):
        return self.table[((ix * 73856093) ^ (iy * 19349663) ^ (it * 83492791)) & self.mask]

    def value(self, x, y, t):
        """Smooth 3D value noise in [0, 1]."""
        x0 = np.floor(x); y0 = np.floor(y); t0 = np.floor(t)
        fx = x - x0; fy = y - y0; ft = t - t0
        fx = fx * fx * fx * (fx * (fx * 6 - 15) + 10)
        fy = fy * fy * fy * (fy * (fy * 6 - 15) + 10)
        ft = ft * ft * ft * (ft * (ft * 6 - 15) + 10)
        ix = x0.astype(np.int64); iy = y0.astype(np.int64); it = int(t0)
        c000 = self._h(ix, iy, it); c100 = self._h(ix + 1, iy, it)
        c010 = self._h(ix, iy + 1, it); c110 = self._h(ix + 1, iy + 1, it)
        c001 = self._h(ix, iy, it + 1); c101 = self._h(ix + 1, iy, it + 1)
        c011 = self._h(ix, iy + 1, it + 1); c111 = self._h(ix + 1, iy + 1, it + 1)
        a = c000 + (c100 - c000) * fx; b = c010 + (c110 - c010) * fx
        c = c001 + (c101 - c001) * fx; d = c011 + (c111 - c011) * fx
        e = a + (b - a) * fy; f = c + (d - c) * fy
        return e + (f - e) * ft

    def fbm(self, x, y, t, octaves=4, lac=2.03, gain=0.5):
        s, amp, norm = 0.0, 1.0, 0.0
        for o in range(octaves):
            k = lac ** o
            s = s + amp * self.value(x * k + o * 17.13, y * k - o * 9.71, t * (1.0 + 0.35 * o) + o * 3.3)
            norm += amp; amp *= gain
        return s / norm

    def warped(self, x, y, t, warp=1.2, octaves=4):
        """Domain-warped fBm: organic, smoke-like but photographic shapes."""
        qx = self.fbm(x + 1.7, y + 9.2, t, 3)
        qy = self.fbm(x + 8.3, y + 2.8, t, 3)
        return self.fbm(x + warp * (qx - 0.5) * 2.0, y + warp * (qy - 0.5) * 2.0, t, octaves)

    def curve(self, t, seed=0.0):
        """1D smooth noise for flicker, in [0, 1]."""
        return float(self.value(np.array([t]), np.array([seed]), 0.0)[0])


# =============================================================================
#  Image helpers
# =============================================================================

def box1d(a, r, axis):
    if r < 1:
        return a
    pad = [(0, 0)] * a.ndim
    pad[axis] = (r + 1, r)
    p = np.pad(a, pad, mode='edge')
    c = np.cumsum(p, axis=axis, dtype=np.float64)
    hi = np.take(c, np.arange(2 * r + 1, c.shape[axis]), axis=axis)
    lo = np.take(c, np.arange(0, c.shape[axis] - 2 * r - 1), axis=axis)
    return ((hi - lo) / (2 * r + 1)).astype(np.float32)


def blur(a, sigma):
    """Gaussian-like blur from three box passes per axis."""
    if sigma < 0.5:
        return a
    r = max(1, int(round(sigma * 0.86)))
    for _ in range(3):
        a = box1d(a, r, 0)
        a = box1d(a, r, 1)
    return a


def resize_f(a, w, h):
    return np.asarray(Image.fromarray(a.astype(np.float32), mode='F').resize((w, h), Image.BICUBIC))


def sig(x):
    return 1.0 / (1.0 + np.exp(-np.clip(x, -30, 30)))


def keys(k, u):
    """Smooth keyframe interpolation. k: list of (u, value)."""
    if u <= k[0][0]:
        return k[0][1]
    for (u0, v0), (u1, v1) in zip(k, k[1:]):
        if u <= u1:
            s = (u - u0) / max(u1 - u0, 1e-6)
            s = s * s * (3 - 2 * s)
            return v0 + (v1 - v0) * s
    return k[-1][1]


# =============================================================================
#  Exposure
# =============================================================================

def layer_exposure(L, X, Y, A, u, noise, nt):
    """Exposure from one burn layer at normalised time u."""
    src = L['source']
    dx = keys(L.get('drift_x', [(0, 0.0)]), u)
    dy = keys(L.get('drift_y', [(0, 0.0)]), u)
    x = X - dx; y = Y - dy
    if src[0] == 'L':
        d = x.copy()
    elif src[0] == 'R':
        d = A - x
    elif src[0] == 'T':
        d = y.copy()
    elif src[0] == 'B':
        d = 1.0 - y
    else:  # ('P', px, py, sx, sy) point source with an elliptical spread
        px, py = keys(src[1], u), keys(src[2], u)
        d = np.sqrt(((x - px) / src[3]) ** 2 + ((y - py) / src[4]) ** 2)
    # cross-axis bend so edge light is not a straight line
    if src[0] in 'LR':
        d = d + L.get('bend', 0.0) * (y - L.get('bend_at', 0.5)) ** 2
    elif src[0] in 'TB':
        d = d + L.get('bend', 0.0) * (x - A * 0.5) ** 2

    sc = L.get('scale', 1.2)
    sp = L.get('speed', 1.0)
    w = noise.warped(x * sc + L['seed'], y * sc, nt * sp, warp=L.get('swirl', 0.9))
    fine = noise.fbm(x * sc * 5.0 - L['seed'], y * sc * 5.0, nt * sp * 1.7, 3)
    dw = d + L.get('warp', 0.28) * (w - 0.5) * 2.0 + L.get('fine', 0.012) * (fine - 0.5) * 2.0

    front = keys(L['front'], u)
    # softness changes across the burn: hard in places, very soft in others
    sv = noise.fbm(x * 1.3 + 40.0, y * 1.3, nt * 0.6, 3)
    soft = L.get('soft', 0.05) * (0.2 + 1.8 * sv ** 1.5)
    m = sig((front - dw) / soft)

    inner = 0.7 + 0.6 * noise.fbm(x * 0.7 + 60.0, y * 0.7, nt * 0.8 * sp, 3)
    g = keys(L['gain'], u)
    # light is strongest near where it gets in and weaker further across
    reach = np.exp(-np.maximum(dw, 0.0) / L.get('falloff', 1.1))
    # streaks: brighter and darker bands running parallel to the edge the light comes from
    st = L.get('streak', 0.3 if src[0] in 'LRTB' else 0.0)
    if st > 0:
        band = noise.fbm(d * L.get('streak_freq', 5.0) + L['seed'] * 3.0, np.full_like(d, L['seed']), nt * 0.5, 3)
        inner = inner * (1.0 - st + 2.0 * st * band)
    E = g * m * inner * reach
    # leading glow ahead of the front (faint, so it renders as red)
    tl = L.get('tail', 0.10)
    E = E + g * L.get('tail_gain', 0.18) * np.exp(-np.maximum(dw - front, 0.0) / tl) * (1.0 - m) * inner
    # hot core well inside the burn
    if 'core' in L:
        cg = keys(L['core'], u)
        if cg > 0:
            co = L.get('core_off', 0.3)
            E = E + cg * sig((front - co - dw) / (soft * 1.6 + 0.02)) * (0.6 + 0.8 * noise.fbm(x * 2.2 + 80.0, y * 2.2, nt, 3))
    # small secondary leaks near the front
    if L.get('spots', 0.0) > 0:
        s2 = noise.warped(x * 2.0 + 90.0, y * 2.0, nt * 1.1 * sp, warp=0.6, octaves=3)
        near = np.exp(-np.abs(dw - front) / 0.35)
        E = E + g * L['spots'] * sig((s2 - 0.62) / 0.05) * near * reach
    return E


def perf_light(X, Y, A, u, noise, nt, side, pitch, strength):
    """Light leaking through sprocket holes onto the film edge."""
    x = X if side == 'L' else A - X
    y = Y + keys([(0, 0.0), (1, 0.12)], u)                          # film slipping a little
    k = np.floor(y / pitch)
    fy = y / pitch - k - 0.5
    core = np.exp(-((fy / 0.24) ** 4)) * np.exp(-(((x - 0.035) / 0.028) ** 4))
    glow = np.exp(-((fy / 0.45) ** 2)) * np.exp(-(np.maximum(x - 0.02, 0) / 0.09))
    irregular = np.clip(noise.fbm(k * 3.1 + 5.0, x * 2.0, nt * 1.2, 2) * 1.6 - 0.35, 0, 1.3)
    return strength * (0.55 * core + 0.6 * glow) * irregular


def film_response(E, R, res):
    """Exposure to colour with halation. R: response parameters."""
    h, w = E.shape
    Eh = (1 - R['halation']) * E + R['halation'] * blur(E, R['halation_px'] * w / 1280.0)
    Eg = 0.85 * E + 0.15 * blur(E, 0.4 * R['halation_px'] * w / 1280.0)
    r = 1.0 - np.exp(-R['kr'] * Eh)
    g = 1.0 - np.exp(-R['kg'] * np.power(np.maximum(Eg, 0), R['pg']))
    b = 1.0 - np.exp(-R['kb'] * np.power(np.maximum(E, 0), R['pb']))
    if R.get('magenta', 0) > 0:                                     # pink contamination in faint light
        faint = (1.0 - np.exp(-E * 2.5)) * np.exp(-E * 1.2)
        b = b + R['magenta'] * 0.35 * faint
        g = g - R['magenta'] * 0.06 * faint
    img = np.stack([r, g, b], -1)
    lum = img @ np.array([0.30, 0.59, 0.11], np.float32)
    img = img * (1 - R.get('density', 0.08)) + lum[..., None] * R.get('density', 0.08)   # photographic density, not neon
    bright = np.maximum(lum - 0.72, 0)
    bl = blur(bright, 0.035 * w)
    img = img + R.get('bloom', 0.22) * bl[..., None] * np.array([1.0, 0.82, 0.55], np.float32)
    return np.clip(img, 0, 1.2)


# =============================================================================
#  Texture at output resolution
# =============================================================================

def add_grain(img, rng, amount):
    h, w, _ = img.shape
    lum = img.mean(-1)
    weight = np.sqrt(np.clip(lum, 0, 1)) * np.clip(1.08 - lum, 0, 1) * 1.6 + 0.03 * np.clip(lum, 0, 1)
    fine = blur(rng.standard_normal((h, w), dtype=np.float32), 0.7) * 2.2
    coarse = resize_f(rng.standard_normal((h // 3, w // 3), dtype=np.float32), w, h) * 0.6
    shared = fine + coarse
    for c in range(3):
        chroma = blur(rng.standard_normal((h // 2, w // 2), dtype=np.float32), 0.6)
        chroma = resize_f(chroma, w, h) * 1.2
        img[..., c] += amount * weight * (0.82 * shared + 0.18 * chroma)
    return img


def add_dust(img, rng, count, scratch=None):
    h, w, _ = img.shape
    mask = np.ones((h, w), np.float32)
    for _ in range(count):
        cx, cy = rng.random() * w, rng.random() * h
        r = (1.0 + 3.5 * rng.random() ** 2) * w / 3840.0 * 2.0
        x0, x1 = int(max(cx - 4 * r, 0)), int(min(cx + 4 * r, w))
        y0, y1 = int(max(cy - 4 * r, 0)), int(min(cy + 4 * r, h))
        if x1 <= x0 or y1 <= y0:
            continue
        yy, xx = np.mgrid[y0:y1, x0:x1]
        d = np.sqrt((xx - cx) ** 2 + ((yy - cy) * (0.6 + 0.8 * rng.random())) ** 2)
        mask[y0:y1, x0:x1] *= 1.0 - 0.75 * np.clip(1.5 - d / r, 0, 1)
    if scratch is not None:
        sx, strength = scratch
        xs = np.arange(w)
        line = np.exp(-((xs - sx * w) / (1.2 * w / 3840.0 * 2.0)) ** 2) * strength
        vis = np.clip(blur(rng.random((h // 8, 1)).astype(np.float32), 3) * 2.0 - 0.3, 0, 1)
        vis = resize_f(vis, 1, h)[:, :1]
        mask *= 1.0 - line[None, :] * vis
    return img * mask[..., None]


# =============================================================================
#  Render
# =============================================================================

def render_frames(preset, size, work, seed=None):
    """Yields (index, float RGB at output size) for every frame."""
    W, H = size
    w, h = work
    A = w / h
    n = preset['frames']
    noise = Noise(preset.get('seed', 7) if seed is None else seed)
    rng = np.random.default_rng(preset.get('seed', 7) + 1)
    X, Y = np.meshgrid(np.linspace(0, A, w, dtype=np.float32), np.linspace(0, 1, h, dtype=np.float32))
    R = dict(P.RESPONSE_DEFAULT); R.update(preset.get('response', {}))
    for f in range(n):
        u = f / (n - 1)
        nt = f * preset.get('noise_rate', 0.22)          # how fast the shapes evolve
        # gate weave: the whole burn jitters slightly frame to frame
        gw = preset.get('weave', 0.003)
        jx, jy = gw * rng.normal(), gw * 0.8 * rng.normal()
        Xs, Ys = X + jx, Y + jy
        E = np.zeros((h, w), np.float32)
        for L in preset['layers']:
            E += layer_exposure(L, Xs, Ys, A, u, noise, nt)
        if 'perfs' in preset:
            pp = preset['perfs']
            E += keys(pp['gain'], u) * perf_light(Xs, Ys, A, u, noise, nt, pp['side'], pp.get('pitch', 0.25), 1.0)
        fl = keys(preset.get('flash', [(0, 0.0)]), u)
        if fl > 0:
            fd = preset.get('flash_dir', 0.0)   # flash brighter towards one side (+ right, - left)
            side = 1.0 + fd * (Xs / A - 0.5) * 2.0
            shape = 1.0
            if 'flash_shrink' in preset:
                thr = keys(preset['flash_shrink'], u)
                shape = sig((noise.warped(Xs * 0.9 + 300.0, Ys * 0.9, nt * 0.4, warp=0.6, octaves=3) - thr) / 0.06)
            E += fl * shape * np.maximum(side, 0.2) * (0.55 + 0.9 * noise.fbm(Xs * 0.6 + 200.0, Ys * 0.6, nt * 0.5, 3))
        # exposure fluctuation: every frame is its own exposure
        fk = preset.get('flicker', 0.12)
        E *= 1.0 + fk * (noise.curve(f * 0.9, 3.0) - 0.5) * 2.0 + 0.5 * fk * rng.normal() * 0.3
        img = film_response(np.maximum(E, 0), R, w)
        big = np.stack([resize_f(img[..., c], W, H) for c in range(3)], -1)
        big = add_grain(big, rng, preset.get('grain', 0.035))
        tex = preset.get('dust', 0)
        scratch = None
        if preset.get('scratch'):
            sx, st = preset['scratch']
            scratch = (sx + 0.002 * rng.normal(), st * (0.6 + 0.4 * rng.random()))
        if tex or scratch:
            big = add_dust(big, rng, int(rng.poisson(tex)) if tex else 0, scratch)
        yield f, np.clip(big, 0, 1)


def write_videos(name, preset, out, size, work, preview=True, alpha=False, mov=True):
    W, H = size
    procs = []
    if mov:
        p = subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb48le', '-s', '%dx%d' % (W, H),
                              '-r', '24', '-i', '-', '-c:v', 'prores_ks', '-profile:v', '3', '-pix_fmt', 'yuv422p10le',
                              '-vendor', 'apl0', '-color_primaries', 'bt709', '-color_trc', 'bt709', '-colorspace', 'bt709',
                              os.path.join(out, name + '.mov')], stdin=subprocess.PIPE)
        procs.append(('mov', p))
    if alpha:
        p = subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgba64le', '-s', '%dx%d' % (W, H),
                              '-r', '24', '-i', '-', '-c:v', 'prores_ks', '-profile:v', '4', '-pix_fmt', 'yuva444p10le',
                              '-alpha_bits', '16', '-vendor', 'apl0',
                              os.path.join(out, name + '_Alpha.mov')], stdin=subprocess.PIPE)
        procs.append(('alpha', p))
    if preview:
        pw, ph = 1280, 720
        p = subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', '%dx%d' % (pw, ph),
                              '-r', '24', '-i', '-', '-c:v', 'libx264', '-crf', '14', '-pix_fmt', 'yuv420p',
                              os.path.join(out, name + '_preview.mp4')], stdin=subprocess.PIPE)
        procs.append(('preview', p))
    frames_small = []
    for f, img in render_frames(preset, size, work):
        for kind, p in procs:
            if kind == 'mov':
                p.stdin.write((img * 65535 + 0.5).astype('<u2').tobytes())
            elif kind == 'alpha':
                # straight colour with alpha: light colour normalised, alpha = how much light
                a = np.clip(img.max(-1, keepdims=True), 0, 1)
                col = np.where(a > 1e-4, img / np.maximum(a, 1e-4), 0)
                rgba = np.concatenate([np.clip(col, 0, 1), a], -1)
                p.stdin.write((rgba * 65535 + 0.5).astype('<u2').tobytes())
        small = np.asarray(Image.fromarray((img * 255 + 0.5).astype(np.uint8)).resize((1280, 720), Image.LANCZOS))
        frames_small.append(small)
        for kind, p in procs:
            if kind == 'preview':
                p.stdin.write(small.tobytes())
    for _, p in procs:
        p.stdin.close(); p.wait()
    return frames_small


def main():
    args = sys.argv[1:]
    out = args[0]
    get = lambda k, d=None: args[args.index(k) + 1] if k in args else d
    size = tuple(map(int, get('--size', '3840x2160').split('x')))
    work = tuple(map(int, get('--work', '1280x720').split('x')))
    only = get('--only')
    os.makedirs(out, exist_ok=True)
    for name, preset in P.PRESETS.items():
        if only and only not in name:
            continue
        frames = write_videos(name, preset, out, size, work, preview=True,
                              alpha='--alpha' in args, mov='--no-mov' not in args)
        np.save(os.path.join(out, name + '_frames.npy'), np.stack(frames))
        print('rendered', name, len(frames), 'frames')


if __name__ == '__main__':
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    main()
