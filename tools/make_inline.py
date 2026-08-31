#!/usr/bin/env python3
"""Generate the shipping CineCore with all looks embedded inline.

Confirmed rules, each established on the target Resolve build:
  * DEFINE_CUBE_LUT(NAME) {   - opening brace on the SAME line
  * content is standard CUBE - LUT_3D_SIZE then RGB triplets, red fastest
  * no terminator keyword
  * APPLY_LUT must be assigned to a float3, never returned directly
  * blocks must appear BEFORE the code that uses them
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bake_luts import load_cube

BAKED = sys.argv[1]
SIZE  = int(sys.argv[2]) if len(sys.argv) > 2 else 33

LOOKS = [('CC_LUT_REFERENCE', '_Reference_Identity'), ('CC_LUT_70S', '70s'),
         ('CC_LUT_ALPINE','Alpine'), ('CC_LUT_CHROME','Chrome'), ('CC_LUT_CLEAN','Clean'),
         ('CC_LUT_COAST','Coast'), ('CC_LUT_DAZE','Daze'), ('CC_LUT_DUSK','Dusk'),
         ('CC_LUT_KODA','Koda'), ('CC_LUT_LAGUNA','Laguna'), ('CC_LUT_LUCID','Lucid'),
         ('CC_LUT_LUX','Lux'), ('CC_LUT_MIST','Mist'), ('CC_LUT_MONACO','Monaco'),
         ('CC_LUT_NORDIC','Nordic'), ('CC_LUT_OBSIDIAN','Obsidian'), ('CC_LUT_PALMA','Palma'),
         ('CC_LUT_SILVER','Silver'), ('CC_LUT_STERLING','Sterling'), ('CC_LUT_VISTA','Vista')]

def resample(src_size, src_data, n):
    if n == src_size: return src_data
    import math
    def sample(r, g, b):
        def ax(v):
            x = min(max(v,0.0),1.0)*(src_size-1); i=int(min(math.floor(x),src_size-2)); return i, x-i
        ir,fr=ax(r); ig,fg=ax(g); ib,fb=ax(b); out=[0.0,0.0,0.0]
        for dz in (0,1):
            wz = fb if dz else 1-fb
            if wz==0: continue
            for dy in (0,1):
                wy = fg if dy else 1-fg
                if wy==0: continue
                for dx in (0,1):
                    w=(fr if dx else 1-fr)*wy*wz
                    if w==0: continue
                    c=src_data[(ir+dx)+(ig+dy)*src_size+(ib+dz)*src_size*src_size]
                    for k in range(3): out[k]+=w*c[k]
        return tuple(out)
    return [sample(r/(n-1), g/(n-1), b/(n-1))
            for b in range(n) for g in range(n) for r in range(n)]

def block(macro, size, data):
    return "\n".join([f"DEFINE_CUBE_LUT({macro}) {{", f"LUT_3D_SIZE {size}"]
                     + [f"{c[0]:.8f} {c[1]:.8f} {c[2]:.8f}" for c in data] + ["}"])

src = open('CineCore.dctl', encoding='utf-8').read()
out = src

blocks = []
for macro, fn in LOOKS:
    s, d = load_cube(os.path.join(BAKED, fn + '.cube'))
    blocks.append(block(macro, SIZE, resample(s, d, SIZE)))

header = ("// ---- Inline LUT data ------------------------------------------------------\n"
          "//  All look data is embedded here. This DCTL has NO external file dependency\n"
          "//  of any kind and cannot be broken by a missing or misplaced file.\n"
          "//\n"
          "//  These blocks MUST stay above the code that uses them: the generated arrays\n"
          "//  are emitted where the block sits, and referencing them earlier in the file\n"
          "//  fails to compile.\n"
          "//\n"
          "//  Values are the original look data, emitted verbatim at 8 decimals.\n"
          + "\n\n".join(blocks) + "\n")

out = re.sub(r'// ---- LUT declarations -+\n(?:.*\n)*?DEFINE_LUT\(CC_LUT_VISTA[^\n]*\n',
             header, out)

# APPLY_LUT assigned to a float3, never returned directly.
lookup = ["__DEVICE__ float3 cc_lookup(float3 v, int look)", "{", "    float3 result = v;", ""]
for macro, fn in LOOKS:
    key = 'CC_LOOK_' + ('REFERENCE' if fn.startswith('_') else fn.upper().replace('70S','70S'))
    lookup.append(f"    if (look == {key}) result = APPLY_LUT(v.x, v.y, v.z, {macro});")
lookup += ["", "    return result;", "}"]
out = re.sub(r'__DEVICE__ float3 cc_lookup\(float3 v, int look\)\n\{\n(?:.*\n)*?\}',
             "\n".join(lookup), out)

assert 'DEFINE_LUT(' not in out and 'luts/' not in out and 'END_CUBE_LUT' not in out
assert not re.findall(r'[\w./\\-]+\.cube', out)
assert 'return APPLY_LUT' not in out
assert out.count('DEFINE_CUBE_LUT') == len(LOOKS)
tpos = out.index('__DEVICE__ float3 cc_lookup')
for m in re.finditer(r'^DEFINE_CUBE_LUT\((\w+)\) \{$', out, re.M):
    assert m.start() < tpos, f"{m.group(1)} defined after the code that uses it"
for n in set(re.findall(r'APPLY_LUT\([^)]*,\s*(\w+)\)', out)):
    assert f'DEFINE_CUBE_LUT({n}) {{' in out, f"{n} applied but not defined"

open('CineCore.dctl.new', 'w', encoding='utf-8').write(out)
os.replace('CineCore.dctl.new', 'CineCore_Inline.dctl')
print(f"CineCore_Inline.dctl  {len(out)/1e6:.2f} MB  {len(out.splitlines()):,} lines")
print(f"  {len(LOOKS)} inline LUTs at {SIZE}^3, all before the code that uses them")
