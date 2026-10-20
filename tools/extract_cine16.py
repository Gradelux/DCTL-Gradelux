#!/usr/bin/env python3
# =============================================================================
#  Gradelux FilmLab: extract the Cine16 secondary engine from the original
#  Cine16 DCTL (reference/Cine16_1B_Color_Profile.dctl).
# -----------------------------------------------------------------------------
#  The Cine16 colour science is NOT rewritten. This script copies the original
#  constants, profile tables, helper functions and transform body verbatim and
#  makes only mechanical changes:
#    1. Every Cine16 identifier gets a c16_ / C16_ prefix (FilmLab already
#       defines DI_A, DI_B, DI_C and DI_M, and the generic names luma, sstep,
#       PH, PS, ... could collide later).
#    2. The UI globals p_strength, p_sep, ... become fields of a C16Controls
#       struct (argument c16c; the body already uses a local named c) passed
#       in by FilmLab; p_profile becomes an explicit profile index.
#    3. transform() becomes c16_apply() (DaVinci Intermediate in and out), and
#       Show Skin Mask becomes an explicit argument.
#  Expressions, constants, operation order, the NaN guard and the final
#  [-0.5, 2.0] clamp are unchanged.
#
#    python3 tools/extract_cine16.py > /tmp/c16_module.h
# =============================================================================
import re, sys, os

SRC = os.path.join(os.path.dirname(__file__), '..', 'reference', 'Cine16_1B_Color_Profile.dctl')
src = open(SRC).read().replace('\r\n', '\n')
lines = src.split('\n')

def find(prefix):
    for i, l in enumerate(lines):
        if l.startswith(prefix):
            return i
    raise SystemExit('marker not found: ' + prefix)

lib_start = find('#define DI_A')
tf_start = find('__DEVICE__ float3 transform(')
lib = '\n'.join(lines[lib_start:tf_start]).rstrip() + '\n'
tf = '\n'.join(lines[tf_start:])
tf = tf[:tf.rindex('}') + 1]

# 1. identifier prefixes (whole words only)
RENAME = {
    'DI_A': 'C16_DI_A', 'DI_B': 'C16_DI_B', 'DI_C': 'C16_DI_C', 'DI_M': 'C16_DI_M',
    'DI_LINCUT': 'C16_DI_LINCUT', 'DI_LOGCUT': 'C16_DI_LOGCUT', 'GREY_DI': 'C16_GREY_DI',
    'YR': 'C16_YR', 'YG': 'C16_YG', 'YB': 'C16_YB', 'NPROF': 'C16_NPROF',
    'PH': 'C16_PH', 'PS': 'C16_PS', 'PL': 'C16_PL', 'PG': 'C16_PG',
    'di2lin': 'c16_di2lin', 'lin2di': 'c16_lin2di', 'luma': 'c16_luma', 'sstep': 'c16_sstep',
    'lin3': 'c16_lin3', 'log3': 'c16_log3', 'satAround': 'c16_satAround',
    'hueInterp': 'c16_hueInterp', 'hsvHue': 'c16_hsvHue', 'hsv2rgb': 'c16_hsv2rgb',
    'lvsCurve': 'c16_lvsCurve',
}
def rename(t):
    for a, b in RENAME.items():
        t = re.sub(r'\b%s\b' % a, b, t)
    return t

lib = rename(lib)
tf = rename(tf)

# 2. UI globals -> control struct fields / arguments
CTRL = {'p_strength': 'c16c.strength', 'p_sep': 'c16c.sep', 'p_sat': 'c16c.sat', 'p_subtr': 'c16c.subtr',
        'p_bias': 'c16c.bias', 'p_tone': 'c16c.tone', 'p_skinprot': 'c16c.skinprot', 'p_showskin': 'showMask'}
for a, b in CTRL.items():
    tf = re.sub(r'\b%s\b' % a, b, tf)

# 3. entry point -> c16_apply; the profile index arrives as an argument
old_sig = '__DEVICE__ float3 transform(int p_Width, int p_Height, int p_X, int p_Y, float p_R, float p_G, float p_B)'
assert tf.startswith(old_sig), 'unexpected transform signature'
tf = tf.replace(old_sig, '__DEVICE__ float3 c16_apply(float p_R, float p_G, float p_B, int profile, C16Controls c16c, int showMask)', 1)
old_pi = 'int pi = min(max((int)p_profile, 0), C16_NPROF - 1);'
assert old_pi in tf, 'unexpected profile selection'
tf = tf.replace(old_pi, 'int pi = min(max(profile, 0), C16_NPROF - 1);', 1)
assert 'p_profile' not in tf and 'p_strength' not in tf

out = []
out.append('// ---- Cine16 constants, profile tables and helpers (verbatim, renamed) ----')
out.append(lib)
out.append('// ---- Cine16 controls (the original sliders, passed in by FilmLab) ----')
out.append('typedef struct {')
out.append('    float strength;   // Profile Strength     0 .. 1.5, default 1')
out.append('    float sep;        // Color Separation     0 .. 2,   default 1')
out.append('    float sat;        // Saturation           0 .. 2,   default 1')
out.append('    float subtr;      // Subtractive Density  0 .. 2,   default 1')
out.append('    float bias;       // Print Bias           0 .. 2,   default 1')
out.append('    float tone;       // Profile Tone         0 .. 1.5, default 1')
out.append('    float skinprot;   // Skin Protection      0 .. 1,   default 1')
out.append('} C16Controls;')
out.append('')
out.append('// ---- Cine16 engine: the original transform() body (verbatim, renamed) ----')
out.append('// DaVinci Wide Gamut / DaVinci Intermediate in and out. profile = 0..15')
out.append('// in the original order (0 = Neutral). Keeps the original NaN guard and')
out.append('// the original final clamp of every channel to [-0.5, 2.0].')
out.append(tf)
sys.stdout.write('\n'.join(out) + '\n')
