#!/usr/bin/env python3
"""Turn a FrameLab DCTL into OpenCL C so clang can check it the way a GPU
compiler would (reserved words like `half`, no doubles, address spaces).

Usage: python3 tests/framelab/build_cl.py Gradelux_FrameLab_v2.dctl out.cl
       clang -x cl -cl-std=CL1.2 -fsyntax-only -Werror=implicit-function-declaration out.cl
"""
import re, sys

SHIM = r'''
#define __DEVICE__
#define __CONSTANT__ __constant
#define __TEXTURE__ __read_only image2d_t
__constant sampler_t glx_smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;
#define _tex2D(t_, px_, py_) read_imagef((t_), glx_smp, (int2)((px_), (py_))).x
#define make_float3(a, b, c) ((float3)((a), (b), (c)))
#define make_float2(a, b) ((float2)((a), (b)))
#define _sinf sin
#define _cosf cos
#define _expf exp
#define _sqrtf sqrt
#define _powf pow
#define _fabs fabs
#define _fminf fmin
#define _fmaxf fmax
#define _atan2f atan2
'''

def main(src, out):
    text = open(src).read()
    decls = []
    def repl(m):
        args = m.group(1)
        parts = [p.strip() for p in args.split(',')]
        name, kind, default = parts[0], parts[2], parts[3]
        if kind == 'DCTLUI_COMBO_BOX':
            enums = re.search(r'\{([^}]*)\}', args).group(1)
            decls.append('enum { %s };' % enums)
            decls.append('__constant int %s = %s;' % (name, default))
        elif kind in ('DCTLUI_SLIDER_INT', 'DCTLUI_CHECK_BOX'):
            decls.append('__constant int %s = %s;' % (name, default))
        else:
            decls.append('__constant float %s = %sf;' % (name, default if '.' in default else default + '.0'))
        return ''
    body = re.sub(r'^DEFINE_UI_PARAMS\((.*)\)\s*$', repl, text, flags=re.M)
    kernel = '''
__kernel void glx_kernel(__read_only image2d_t r, __read_only image2d_t g, __read_only image2d_t b, __global float *out)
{
    int x = get_global_id(0), y = get_global_id(1);
    float3 c = transform(1920, 1080, x, y, r, g, b);
    out[(y * 1920 + x) * 3] = c.x;
}
'''
    open(out, 'w').write(SHIM + '\n'.join(decls) + '\n' + body + kernel)

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
