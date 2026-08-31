#!/usr/bin/env python3
"""Generate the inline-LUT proof of concept from CineCore.dctl.

Replaces the external DEFINE_LUT block and the 20-entry look list with two
INLINE LUTs carrying real data, so the build has no external file dependency
of any kind. CineCore.dctl remains the single source of truth for the engine.

The LUT values are emitted verbatim from the source .cube - same numbers, same
precision, no resampling and no modification of the creative data.
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bake_luts import load_cube

BAKED = sys.argv[1]
TEST_LOOK = sys.argv[2] if len(sys.argv) > 2 else 'Koda'

def inline_block(macro, size, data):
    """Inline CUBE LUT, per the DCTL documentation:

        DEFINE_CUBE_LUT([lutName]) {
        [LUT_Content]
        }

    Verified against the target Resolve build: the opening brace must be on the
    same line as the macro, the content is the standard CUBE format -
    LUT_3D_SIZE then RGB triplets with the red index varying fastest - and
    there is no terminator keyword.
    """
    lines = [f"DEFINE_CUBE_LUT({macro}) {{", f"LUT_3D_SIZE {size}"]
    lines += [f"{c[0]:.8f} {c[1]:.8f} {c[2]:.8f}" for c in data]
    lines.append("}")
    return "\n".join(lines)

src = open('CineCore.dctl', encoding='utf-8').read()
out = src

# --- 1. two-entry look list plus None -------------------------------------
out = re.sub(r'DEFINE_UI_PARAMS\(gLook, Film Look.*?\n',
             'DEFINE_UI_PARAMS(gLook, Film Look, DCTLUI_COMBO_BOX, 0, '
             '{CC_LOOK_REFERENCE, CC_LOOK_TEST}, '
             '{Reference Identity, Test Look})\n', out)

# --- 2. external declarations -> inline data ------------------------------
ref_s, ref_d = load_cube(os.path.join(BAKED, '_Reference_Identity.cube'))
tst_s, tst_d = load_cube(os.path.join(BAKED, TEST_LOOK + '.cube'))

block = (
"//  The LUT data below is inline: this DCTL has NO external file dependency of\n"
"//  any kind and cannot be broken by a missing or misplaced LUT file.\n"
"//\n"
"//  Two LUTs are embedded rather than one, deliberately. Whether MULTIPLE inline\n"
"//  LUTs can coexist in a single DCTL is the thing that decides whether all 19\n"
"//  can be embedded later, so the proof of concept has to test it.\n"
"//\n"
f"//  Values are emitted verbatim from the original LUT data at 8 decimals -\n"
f"//  same numbers, no resampling, no modification of the creative data.\n"
f"//    Reference Identity   {ref_s}^3, {ref_s**3} entries\n"
f"//    Test Look ({TEST_LOOK})       {tst_s}^3, {tst_s**3} entries\n"
+ inline_block('CC_LUT_REFERENCE', ref_s, ref_d) + "\n\n"
+ inline_block('CC_LUT_TEST', tst_s, tst_d) + "\n")

out = re.sub(r'// ---- LUT declarations -+\n(?:.*\n)*?DEFINE_LUT\(CC_LUT_VISTA[^\n]*\n',
             '', out)
# Inline LUT data goes at the very end, after the main entry function. The
# documentation permits either side, and keeping 2 MB of numbers out of the
# readable part of the file is worth doing.
out = out.rstrip() + "\n\n\n" + block

# --- 3. lookup switch ------------------------------------------------------
# APPLY_LUT is assigned to a float3 and that variable returned. Returning
# APPLY_LUT directly makes Resolve reject the file with "main DCTL function's
# return value must be float3" - confirmed on the target build.
out = re.sub(r'    if \(look == CC_LOOK_REFERENCE\).*?\n    return v;\n',
             '    float3 result;\n\n'
             '    if (look == CC_LOOK_TEST) result = APPLY_LUT(v.x, v.y, v.z, CC_LUT_TEST);\n'
             '    else                      result = APPLY_LUT(v.x, v.y, v.z, CC_LUT_REFERENCE);\n\n'
             '    return result;\n', out, flags=re.S)
# with only two entries there is no None, so the look stage always applies one
out = out.replace("    if (look == CC_LOOK_NONE || mix <= 0.0f) return v;",
                  "    if (mix <= 0.0f) return v;")

# --- 4. retitle ------------------------------------------------------------
out = out.replace('//  CineCore\n', '//  CineCore  -  INLINE LUT PROOF OF CONCEPT\n', 1)
out = re.sub(r'//  ---------------------------------------------------------------------------\n'
             r'//  REQUIRES A LUT FOLDER\n'
             r'//  ---------------------------------------------------------------------------\n'
             r'(?://.*\n)*?(?=// =)',
"""//  ---------------------------------------------------------------------------
//  NO EXTERNAL FILES
//  ---------------------------------------------------------------------------
//  Both LUTs are embedded inline. Nothing outside this file is required, and no
//  missing or misplaced LUT file can disable the grading engine.
//
//  This build carries the complete Phase 1-3 engine and every control, plus a
//  two-entry Film Look menu:
//     Reference Identity the architecture check - should look unchanged
//     Test Look          one real 33-point look, embedded verbatim
//
//  Reference Identity is the default. Note it is only NEARLY a no-op: like any
//  LUT it carries the 33-cubed interpolation error and clamps above diffuse
//  white. Look Mix at 0 is the exact pass-through in this build.
//  ---------------------------------------------------------------------------
""", out)

# Strict external-dependency audit. The build must be incapable of looking
# for a file on disk, so these are assertions, not comments.
import re as _re
assert 'DEFINE_LUT(' not in out, "an external LUT declaration survived"
assert 'luts/' not in out, "an external path survived"
assert 'END_CUBE_LUT' not in out, "invented terminator survived"
_files = _re.findall(r'[\w./\\-]+\.cube', out)
assert not _files, f"a .cube filename survived: {_files}"
assert out.count('DEFINE_CUBE_LUT') == 2, "expected exactly two inline definitions"
# every name APPLY_LUT uses must be defined inline in this file
for _n in set(_re.findall(r'APPLY_LUT\([^)]*,\s*(\w+)\)', out)):
    assert f'DEFINE_CUBE_LUT({_n})' in out, f"{_n} is applied but never defined inline"
open('CineCore_PoC.dctl', 'w', encoding='utf-8').write(out)
print(f"CineCore_PoC.dctl  {len(out)/1e6:.2f} MB  {len(out.splitlines())} lines")
print(f"  inline: Reference Identity {ref_s}^3 + Test Look '{TEST_LOOK}' {tst_s}^3")
