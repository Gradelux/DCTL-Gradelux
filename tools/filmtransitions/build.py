#!/usr/bin/env python3
"""Builds the Gradelux Film Transitions pack: light-leak and burn transitions.

Each transition is a standalone Transition DCTL. In Resolve: Edit page >
Effects > OpenFX > Transitions > ResolveFX Color > DCTL, then pick the file in
the DCTL List. The look follows the transition length you set on the timeline.

How they work: a leak of light (deep red, then orange, amber and cream at
its hottest) builds over the outgoing clip, peaks, and the cut to the
incoming clip happens hidden under the peak. Then the leak fades off the new
clip. Everything is computed per pixel from the two clips, so it is fast.

Written in the plain style Resolve accepted in testing: no controls, no quote
characters, clips read only inside transition(), one return at the end.

Usage: python3 tools/filmtransitions/build.py   (writes FilmTransitions/*.dctl)
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, 'FilmTransitions')

# Positions use frame-normalised coordinates: x and y run from -1 to 1 across
# the frame (y down). Times are fractions of the transition (0 to 1).
#   o0, o1     leak centre at the start and at the end
#   r0, r1     leak radius at the start and at full strength
#   stretch    (x, y) shape of the leak
#   flood      how much the whole frame fills with light at the peak
#   gain       brightness of the leak core (above 1 overexposes to cream)
#   rise, fall envelope: (start, full) and (start, gone)
#   cut        (start, end) of the switch from the outgoing to the incoming clip
#   palette    0 warm red-amber, 1 golden, 2 deep red, 3 burn to cream white
#   noise      organic unevenness of the leak shape
#   twin       1 adds a mirrored leak from the opposite side
#   flashes    1 adds two short flash frames before the peak
TRANSITIONS = [
    ('Gradelux_Transition_01_RedLeak', 'Red Leak',
     'A deep red glow creeps in from the right, blooms into an amber flood and hides the cut.',
     dict(o0=(1.30, 0.05), o1=(0.85, 0.10), r0=0.30, r1=2.20, stretch=(1.0, 1.5), flood=0.55, gain=1.05,
          rise=(0.00, 0.50), fall=(0.52, 0.90), cut=(0.48, 0.52), palette=0, noise=0.35, twin=0, flashes=0)),
    ('Gradelux_Transition_02_AmberFlash', 'Amber Flash',
     'A quick golden flash that fills the frame from the centre.',
     dict(o0=(0.10, 0.00), o1=(-0.10, 0.05), r0=0.20, r1=2.60, stretch=(1.0, 1.0), flood=0.90, gain=1.05,
          rise=(0.15, 0.50), fall=(0.52, 0.80), cut=(0.48, 0.52), palette=1, noise=0.25, twin=0, flashes=0)),
    ('Gradelux_Transition_03_BurnOut', 'Burn Out',
     'A red-orange flare sweeps in from the left and burns out to overexposed cream white.',
     dict(o0=(-1.35, 0.35), o1=(0.55, -0.10), r0=0.40, r1=2.40, stretch=(1.0, 1.3), flood=0.75, gain=1.55,
          rise=(0.00, 0.50), fall=(0.53, 0.72), cut=(0.49, 0.53), palette=3, noise=0.45, twin=0, flashes=0)),
    ('Gradelux_Transition_04_OrangeSweep', 'Orange Sweep',
     'A soft orange leak travels across the frame from left to right.',
     dict(o0=(-1.70, -0.20), o1=(1.70, 0.20), r0=0.80, r1=1.15, stretch=(0.9, 1.6), flood=0.15, gain=1.15,
          rise=(0.00, 0.40), fall=(0.60, 1.00), cut=(0.46, 0.54), palette=0, noise=0.40, twin=0, flashes=0)),
    ('Gradelux_Transition_05_CornerFlare', 'Corner Flare',
     'A hot red flare blooms from the bottom right corner.',
     dict(o0=(1.15, 0.95), o1=(0.80, 0.60), r0=0.30, r1=2.00, stretch=(1.1, 1.0), flood=0.40, gain=1.15,
          rise=(0.00, 0.50), fall=(0.52, 0.90), cut=(0.48, 0.52), palette=2, noise=0.35, twin=0, flashes=0)),
    ('Gradelux_Transition_06_GoldDissolve', 'Gold Dissolve',
     'A gentle golden glow with a soft dissolve between the clips.',
     dict(o0=(0.00, 0.00), o1=(0.00, 0.00), r0=2.80, r1=3.00, stretch=(1.0, 1.0), flood=0.50, gain=0.62,
          rise=(0.05, 0.50), fall=(0.50, 0.95), cut=(0.30, 0.70), palette=1, noise=0.20, twin=0, flashes=0)),
    ('Gradelux_Transition_07_TwinLeaks', 'Twin Leaks',
     'Warm leaks enter from both sides and meet in the middle.',
     dict(o0=(1.30, 0.10), o1=(0.70, -0.05), r0=0.30, r1=1.80, stretch=(1.0, 1.4), flood=0.50, gain=1.05,
          rise=(0.00, 0.50), fall=(0.52, 0.90), cut=(0.48, 0.52), palette=0, noise=0.35, twin=1, flashes=0)),
    ('Gradelux_Transition_08_FlashFrames', 'Flash Frames',
     'End-of-roll flash frames: two quick bursts of light, then a full burn that hides the cut.',
     dict(o0=(0.20, 0.10), o1=(-0.20, 0.00), r0=2.20, r1=3.00, stretch=(1.0, 1.0), flood=1.00, gain=1.45,
          rise=(0.42, 0.50), fall=(0.53, 0.68), cut=(0.49, 0.53), palette=3, noise=0.35, twin=0, flashes=1)),
]

COMMON = r'''
// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

__DEVICE__ float glx_floor(float x)
{
    x = _fminf(_fmaxf(x, -16777216.0f), 16777216.0f);
    float t = (float)((int)x);
    return (t > x) ? t - 1.0f : t;
}
__DEVICE__ float glx_fract(float x) { return x - glx_floor(x); }
__DEVICE__ float glx_clamp(float x, float a, float b) { return _fminf(_fmaxf(x, a), b); }
__DEVICE__ float glx_sat(float x) { return glx_clamp(x, 0.0f, 1.0f); }
__DEVICE__ float glx_mix(float a, float b, float t) { return a + (b - a) * t; }
__DEVICE__ float glx_smooth(float e0, float e1, float x)
{
    float t = glx_sat((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}
__DEVICE__ float3 glx_mix3(float3 a, float3 b, float t)
{
    return make_float3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}
__DEVICE__ float3 glx_scale3(float3 a, float s) { return make_float3(a.x * s, a.y * s, a.z * s); }

__DEVICE__ float glx_hash2(float x, float y)
{
    float a = glx_fract(x * 0.1031f);
    float b = glx_fract(y * 0.1030f);
    float c = glx_fract(x * 0.0973f);
    float d = a * (b + 33.33f) + b * (c + 33.33f) + c * (a + 33.33f);
    a += d; b += d; c += d;
    return glx_fract((a + b) * c);
}
__DEVICE__ float glx_noise2(float x, float y)
{
    float ix = glx_floor(x), iy = glx_floor(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float a = glx_hash2(ix, iy), b = glx_hash2(ix + 1.0f, iy);
    float c = glx_hash2(ix, iy + 1.0f), d = glx_hash2(ix + 1.0f, iy + 1.0f);
    return glx_mix(glx_mix(a, b, fx), glx_mix(c, d, fx), fy);
}
__DEVICE__ float glx_fbm2(float x, float y)
{
    return 0.55f * glx_noise2(x, y) + 0.30f * glx_noise2(x * 2.1f + 3.1f, y * 2.1f - 1.7f)
         + 0.15f * glx_noise2(x * 4.3f - 2.3f, y * 4.3f + 5.9f);
}

// One soft blob of leaked light. nx, ny: frame-normalised position.
__DEVICE__ float glx_blob(float nx, float ny, float ox, float oy, float R, float sx, float sy, float n)
{
    float dx = (nx - ox) / sx, dy = (ny - oy) / sy;
    float d = _sqrtf(dx * dx + dy * dy) / _fmaxf(R, 0.001f);
    d += n;
    return _expf(-d * d * 3.4f);
}

// Colour of leaked light at intensity I: deep red when faint, then orange,
// amber and, at its hottest, cream white.
__DEVICE__ float3 glx_leakColor(float I, int palette)
{
    float3 c0 = make_float3(0.50f, 0.035f, 0.012f);
    float3 c1 = make_float3(1.00f, 0.38f, 0.08f);
    float3 c2 = make_float3(1.00f, 0.70f, 0.26f);
    float3 c3 = make_float3(1.00f, 0.95f, 0.80f);
    if (palette == 1) { c0 = make_float3(0.70f, 0.30f, 0.04f); c1 = make_float3(1.00f, 0.62f, 0.15f); c2 = make_float3(1.00f, 0.80f, 0.36f); }
    if (palette == 2) { c0 = make_float3(0.50f, 0.02f, 0.03f); c1 = make_float3(0.95f, 0.16f, 0.07f); c2 = make_float3(1.00f, 0.50f, 0.30f); }
    if (palette == 3) { c2 = make_float3(1.00f, 0.82f, 0.45f); c3 = make_float3(1.00f, 0.97f, 0.86f); }
    float3 c = glx_scale3(c0, glx_smooth(0.02f, 0.38f, I));
    c = glx_mix3(c, c1, glx_smooth(0.25f, 0.65f, I));
    c = glx_mix3(c, c2, glx_smooth(0.60f, 0.95f, I));
    c = glx_mix3(c, c3, glx_smooth(0.95f, 1.40f, I));
    return c;
}

// Adds light like an overexposure: highlights roll off instead of clipping.
__DEVICE__ float glx_expose(float p, float l)
{
    float base = _fmaxf(p, 0.0f);
    return base + _fmaxf(1.0f - base, 0.0f) * glx_sat(l) + (p < 0.0f ? p : 0.0f);
}
'''

MAIN = r'''
// =============================================================================
//  MAIN
// =============================================================================

__DEVICE__ float4 transition(int p_Width, int p_Height, int p_X, int p_Y, __TEXTURE__ p_FromTexR, __TEXTURE__ p_FromTexG, __TEXTURE__ p_FromTexB, __TEXTURE__ p_FromTexA, __TEXTURE__ p_ToTexR, __TEXTURE__ p_ToTexG, __TEXTURE__ p_ToTexB, __TEXTURE__ p_ToTexA)
{
    float t = glx_sat(TRANSITION_PROGRESS);

    float3 fromC = make_float3(_tex2D(p_FromTexR, p_X, p_Y), _tex2D(p_FromTexG, p_X, p_Y), _tex2D(p_FromTexB, p_X, p_Y));
    float3 toC = make_float3(_tex2D(p_ToTexR, p_X, p_Y), _tex2D(p_ToTexG, p_X, p_Y), _tex2D(p_ToTexB, p_X, p_Y));
    float fromA = _tex2D(p_FromTexA, p_X, p_Y);
    float toA = _tex2D(p_ToTexA, p_X, p_Y);

    // the cut happens hidden under the brightest moment of the leak
    float cutMix = glx_smooth(CUT_0, CUT_1, t);
    float3 pic = glx_mix3(fromC, toC, cutMix);
    float alpha = glx_mix(fromA, toA, cutMix);

    // strength of the leak over time
    float env = glx_smooth(RISE_0, RISE_1, t) * (1.0f - glx_smooth(FALL_0, FALL_1, t));
    if (FLASHES != 0) {
        float f1 = _expf(-((t - 0.26f) * (t - 0.26f)) / 0.0005f);
        float f2 = _expf(-((t - 0.36f) * (t - 0.36f)) / 0.0004f);
        env = _fmaxf(env, _fmaxf(0.55f * f1, 0.80f * f2));
    }

    // frame-normalised position, -1 to 1 across the frame
    float nx = ((float)p_X + 0.5f) / (float)p_Width * 2.0f - 1.0f;
    float ny = ((float)p_Y + 0.5f) / (float)p_Height * 2.0f - 1.0f;

    // the leak drifts and grows; its edge is uneven and slowly changes shape
    float travel = glx_smooth(0.0f, 1.0f, t);
    float ox = glx_mix(O0X, O1X, travel), oy = glx_mix(O0Y, O1Y, travel);
    // the leak grows while it builds, then stays large and simply fades
    float grow = glx_smooth(RISE_0, RISE_1, t);
    if (FLASHES != 0) grow = _fmaxf(grow, glx_smooth(0.2f, 0.4f, t));
    float R = glx_mix(R_0, R_1, grow * grow * (3.0f - 2.0f * grow) * grow);
    float n = NOISE * (glx_fbm2(nx * 1.3f + t * 0.9f + 3.0f, ny * 1.3f - t * 0.6f) - 0.5f) * 1.6f;
    float I = glx_blob(nx, ny, ox, oy, R, STRETCH_X, STRETCH_Y, n);
    if (TWIN != 0) I = _fmaxf(I, glx_blob(nx, ny, -ox, oy, R, STRETCH_X, STRETCH_Y, -n * 0.8f));
    float floodI = FLOOD * env * env * env * env * (0.85f + 0.3f * glx_fbm2(nx * 0.8f - t * 0.5f, ny * 0.8f + 2.0f));
    I = _fmaxf(I * env, floodI) * GAIN;

    float3 light = glx_leakColor(I, PALETTE);
    float3 outC = make_float3(glx_expose(pic.x, light.x), glx_expose(pic.y, light.y), glx_expose(pic.z, light.z));

    // past full brightness the film overexposes towards the light itself
    float over = glx_smooth(0.95f, 1.45f, I);
    outC = glx_mix3(outC, light, over * 0.85f);

    float4 result = make_float4(outC.x, outC.y, outC.z, alpha);
    return result;
}
'''


def f(x):
    return '%.4ff' % x


def wrap(text, width):
    words, lines, cur = text.split(), [], ''
    for w in words:
        if cur and len(cur) + len(w) + 1 > width:
            lines.append(cur); cur = w
        else:
            cur = (cur + ' ' + w).strip()
    lines.append(cur)
    return lines


def build():
    os.makedirs(OUT, exist_ok=True)
    for name, title, desc, p in TRANSITIONS:
        body = MAIN
        rep = {
            'CUT_0': f(p['cut'][0]), 'CUT_1': f(p['cut'][1]),
            'RISE_0': f(p['rise'][0]), 'RISE_1': f(p['rise'][1]),
            'FALL_0': f(p['fall'][0]), 'FALL_1': f(p['fall'][1]),
            'O0X': f(p['o0'][0]), 'O0Y': f(p['o0'][1]), 'O1X': f(p['o1'][0]), 'O1Y': f(p['o1'][1]),
            'R_0': f(p['r0']), 'R_1': f(p['r1']),
            'STRETCH_X': f(p['stretch'][0]), 'STRETCH_Y': f(p['stretch'][1]),
            'NOISE': f(p['noise']), 'FLOOD': f(p['flood']), 'GAIN': f(p['gain']),
            'PALETTE': str(p['palette']), 'TWIN': str(p['twin']), 'FLASHES': str(p['flashes']),
        }
        for k in sorted(rep, key=len, reverse=True):
            body = body.replace(k, rep[k])
        head = '// =============================================================================\n'
        head += '//  GRADELUX FILM TRANSITIONS - %s\n' % title.upper()
        head += '// =============================================================================\n//\n'
        for line in wrap(desc, 74):
            head += '//  %s\n' % line
        head += '//\n//  Edit page: Effects, OpenFX, Transitions, ResolveFX Color, DCTL.\n'
        head += '//  Drop it between two clips, then pick this file in the DCTL List.\n'
        head += '//  The effect follows the transition length set on the timeline.\n'
        head += '// =============================================================================\n'
        path = os.path.join(OUT, name + '.dctl')
        open(path, 'w').write(head + COMMON + body)
        print('wrote', os.path.relpath(path, ROOT))


if __name__ == '__main__':
    build()
