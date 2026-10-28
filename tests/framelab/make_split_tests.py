#!/usr/bin/env python3
"""Split Gradelux_FrameLab_v2.dctl into three small DCTLs to find which part
Resolve rejects. Each one passes the picture through untouched.

  FrameLab_Test_1_Controls  - only FrameLab's 46 controls
  FrameLab_Test_2_FirstHalf - FrameLab's code up to the dust section
  FrameLab_Test_3_AllCode   - all of FrameLab's code except the main function

Usage: python3 tests/framelab/make_split_tests.py [out_dir]
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = open(os.path.join(ROOT, 'Gradelux_FrameLab_v2.dctl')).read()
OUT = sys.argv[1] if len(sys.argv) > 1 else HERE

lines = SRC.split('\n')
ui = [l for l in lines if l.startswith('DEFINE_UI_PARAMS')]
first_code = next(i for i, l in enumerate(lines) if l.startswith('//  Fixed settings')) - 1
cut = next(i for i, l in enumerate(lines) if l.startswith('//  Dust, hairs')) - 1
main = next(i for i, l in enumerate(lines) if l.startswith('//  MAIN')) - 1
code_half = '\n'.join(lines[first_code:cut])
code_all = '\n'.join(lines[first_code:main])

PASS = '''
__DEVICE__ float3 transform(int p_Width, int p_Height, int p_X, int p_Y, __TEXTURE__ p_TexR, __TEXTURE__ p_TexG, __TEXTURE__ p_TexB)
{
    float3 c = make_float3(_tex2D(p_TexR, p_X, p_Y), _tex2D(p_TexG, p_X, p_Y), _tex2D(p_TexB, p_X, p_Y));
%s
    return c;
}
'''
SLIDER = 'DEFINE_UI_PARAMS(ui_testAmount, Test Amount, DCTLUI_SLIDER_FLOAT, 0.0, 0.0, 1.0, 0.01)'
USE_SLIDER = '    c = make_float3(c.x + ui_testAmount * 0.0f, c.y, c.z);'

tests = {
    'FrameLab_Test_1_Controls': (
        '// FrameLab test 1: only the controls. Passes the picture through.\n\n' + '\n'.join(ui),
        '    if (ui_animSource == AS_TIME) c = make_float3(c.x, c.y, c.z + ui_frameSize * 0.0f);'),
    'FrameLab_Test_2_FirstHalf': (
        '// FrameLab test 2: the first half of the code. Passes the picture through.\n\n' + SLIDER + '\n' + code_half,
        USE_SLIDER + '\n    c = make_float3(c.x + glx_hash1((float)p_X) * 0.0f, c.y, c.z);'),
    'FrameLab_Test_3_AllCode': (
        '// FrameLab test 3: all of the code except the main function. Passes the picture through.\n\n' + SLIDER + '\n' + code_all,
        USE_SLIDER + '\n    GlxGauge g = glx_gauge(1);\n    c = make_float3(c.x + g.pitch * 0.0f, c.y, c.z);'),
}
for name, (top, use) in tests.items():
    path = os.path.join(OUT, name + '.dctl')
    open(path, 'w').write(top + '\n' + PASS % use)
    print('wrote', path)
