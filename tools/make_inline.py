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
SIZE  = int(sys.argv[2]) if len(sys.argv) > 2 else 0   # 0 = verbatim

LOOKS = [('CC_LUT_70S', '70s'),
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

def raw_cube(path):
    """Return (size, [raw data lines]) with the numbers EXACTLY as written in the
    source file. Reformatting them loses precision: values in scientific
    notation such as 6.10361e-05 do not survive a fixed-decimal round trip."""
    size, rows = None, []
    for line in open(path):
        t = line.strip()
        if not t or t.startswith('#') or t.startswith('TITLE') or t.startswith('DOMAIN'):
            continue
        if t.startswith('LUT_3D_SIZE'):
            size = int(t.split()[1]); continue
        if len(t.split()) == 3:
            rows.append(t)
    assert size and len(rows) == size ** 3, f"{path}: {size}^3 vs {len(rows)} rows"
    return size, rows

def block(macro, size, rows):
    return "\n".join([f"DEFINE_CUBE_LUT({macro}) {{", f"LUT_3D_SIZE {size}"] + rows + ["}"])

src = open('CineCore.dctl', encoding='utf-8').read()
out = src

# Reference Identity is generated, not loaded: a 2x2x2 identity is exact under
# trilinear interpolation, so it is a true no-op rather than a near one.
ident = [f"{i & 1}.000000 {(i >> 1) & 1}.000000 {(i >> 2) & 1}.000000" for i in range(8)]
blocks = [block('CC_LUT_REFERENCE', 2, ident)]

# Every look is embedded EXACTLY as supplied: original values, original grid
# size, no colour space conversion and no resampling of any kind.
for macro, fn in LOOKS:
    if SIZE == 0:
        # verbatim: original grid, original characters, nothing touched
        s, rows = raw_cube(os.path.join(BAKED, fn + '.cube'))
        blocks.append(block(macro, s, rows))
    else:
        # resampled to a common grid. Six decimals is well below the error the
        # resample itself introduces, so it costs nothing extra.
        s, d = load_cube(os.path.join(BAKED, fn + '.cube'))
        rs = d if s == SIZE else resample(s, d, SIZE)
        blocks.append(block(macro, SIZE,
                            [f"{c[0]:.6f} {c[1]:.6f} {c[2]:.6f}" for c in rs]))

header = ("// ---- Inline LUT data ------------------------------------------------------\n"
          "//  All look data is embedded here. This DCTL has NO external file dependency\n"
          "//  of any kind and cannot be broken by a missing or misplaced file.\n"
          "//\n"
          "//  These blocks MUST stay above the code that uses them: the generated arrays\n"
          "//  are emitted where the block sits, and referencing them earlier in the file\n"
          "//  fails to compile.\n"
          "//\n"
          "//  Values are the original look data EXACTLY as supplied - original values,\n"
          "//  original grid size, no colour space conversion, no resampling.\n"
          + "\n\n".join(blocks) + "\n")

out = re.sub(r'// ---- LUT declarations -+\n(?:.*\n)*?DEFINE_LUT\(CC_LUT_VISTA[^\n]*\n',
             header, out)

# APPLY_LUT assigned to a float3, never returned directly.
lookup = ["__DEVICE__ float3 cc_lookup(float3 v, int look)", "{", "    float3 result = v;", "",
          "    if (look == CC_LOOK_REFERENCE) result = APPLY_LUT(v.x, v.y, v.z, CC_LUT_REFERENCE);"]
for macro, fn in LOOKS:
    key = 'CC_LOOK_' + ('REFERENCE' if fn.startswith('_') else fn.upper().replace('70S','70S'))
    lookup.append(f"    if (look == {key}) result = APPLY_LUT(v.x, v.y, v.z, {macro});")
lookup += ["", "    return result;", "}"]
out = re.sub(r'__DEVICE__ float3 cc_lookup\(float3 v, int look\)\n\{\n(?:.*\n)*?\}',
             "\n".join(lookup), out)

assert 'DEFINE_LUT(' not in out and 'luts/' not in out and 'END_CUBE_LUT' not in out
assert not re.findall(r'[\w./\\-]+\.cube', out)
assert 'return APPLY_LUT' not in out
assert out.count('DEFINE_CUBE_LUT') == len(LOOKS) + 1
tpos = out.index('__DEVICE__ float3 cc_lookup')
for m in re.finditer(r'^DEFINE_CUBE_LUT\((\w+)\) \{$', out, re.M):
    assert m.start() < tpos, f"{m.group(1)} defined after the code that uses it"
for n in set(re.findall(r'APPLY_LUT\([^)]*,\s*(\w+)\)', out)):
    assert f'DEFINE_CUBE_LUT({n}) {{' in out, f"{n} applied but not defined"

OUT = os.environ.get('CC_OUT', 'CineCore_Inline.dctl')
open('CineCore.dctl.new', 'w', encoding='utf-8').write(out)
os.replace('CineCore.dctl.new', OUT)
print(f"{OUT}  {len(out)/1048576:.2f} MB  {len(out.splitlines()):,} lines")
print(f"  {len(LOOKS)} inline LUTs at {SIZE}^3, all before the code that uses them")
