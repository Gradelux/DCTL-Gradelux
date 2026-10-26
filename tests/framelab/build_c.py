#!/usr/bin/env python3
"""Turn Gradelux_FrameLab_v1.dctl into a C program for testing on a computer.

The DCTL code is used as-is. Only the DEFINE_UI_PARAMS lines are replaced with
C globals, and a small shim maps DCTL built-ins (_tex2D, _sinf, ...) to C.

Usage: python3 tests/framelab/build_c.py Gradelux_FrameLab_v1.dctl out.c
"""
import re, sys

SHIM = r'''
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { float x, y, z; } float3;
typedef struct { float x, y; } float2;
static inline float3 make_float3(float a, float b, float c) { float3 r = {a, b, c}; return r; }
static inline float2 make_float2(float a, float b) { float2 r = {a, b}; return r; }
typedef struct { const float *d; int w, h; } Tex;
#define __TEXTURE__ Tex
#define __DEVICE__ static inline
#define __CONSTANT__ static const
static inline float _tex2D(Tex t, int x, int y) {
    if (x < 0 || y < 0 || x >= t.w || y >= t.h) { fprintf(stderr, "tex out of range %d %d\n", x, y); exit(3); }
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
'''

def main(src, out):
    text = open(src).read()
    decls, setters = [], []
    def repl(m):
        args = m.group(1)
        name = args.split(',')[0].strip()
        kind = args.split(',')[2].strip()
        default = args.split(',')[3].strip()
        if kind == 'DCTLUI_COMBO_BOX':
            enums = re.search(r'\{([^}]*)\}', args).group(1)
            decls.append('enum { %s };' % enums)
            decls.append('static int %s = %s;' % (name, default))
            setters.append('if (!strcmp(k, "%s")) { %s = (int)v; return 1; }' % (name, name))
        elif kind in ('DCTLUI_SLIDER_INT', 'DCTLUI_CHECK_BOX'):
            decls.append('static int %s = %s;' % (name, default))
            setters.append('if (!strcmp(k, "%s")) { %s = (int)v; return 1; }' % (name, name))
        else:
            decls.append('static float %s = %sf;' % (name, default if '.' in default else default + '.0'))
            setters.append('if (!strcmp(k, "%s")) { %s = (float)v; return 1; }' % (name, name))
        return ''
    body = re.sub(r'^DEFINE_UI_PARAMS\((.*)\)\s*$', repl, text, flags=re.M)
    setter = 'static int set_param(const char *k, double v) {\n    %s\n    return 0;\n}\n' % '\n    '.join(setters)
    with open(out, 'w') as f:
        f.write(SHIM + '\n'.join(decls) + '\n' + body + '\n' + setter)

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
