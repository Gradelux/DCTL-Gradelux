# FilmLab V8: Cine16 secondary film look integration

`Gradelux_FilmLab_v8.dctl` adds the sixteen Cine16 film colour profiles (from
`Cine16_1B_Color_Profile.dctl` v2.2) to FilmLab as an independent **Secondary
Film Look** engine. It is built on FilmLab V7, the latest approved version: your
uploaded V7 is byte-identical to the repository's V7 apart from line endings.

## 1. What is preserved, and how it was verified

| Claim | Verified by | Result |
|---|---|---|
| Each secondary profile reproduces the original Cine16 DCTL (Primary Off, Print None, other controls neutral) | `tests/cine16_regression.py`, section A | **Bit-identical.** 786,272 samples, 16 profiles: RGB grid, grey ramp, skin, saturated, extended range, random pixels, random and extreme slider settings, Show Skin Mask |
| Secondary Off = FilmLab V7 | `cine16_regression.py` B, and the `math_test_v8.c` dumps `DUMP_ALL_V7` / `DUMP_ALL_V8` | **Bit-identical:** 302,904 samples, plus 265,248 with every FilmLab control and Stage View randomised and the secondary sliders set randomly |
| Primary → Secondary → Print order, each engine unchanged in the chain | `cine16_regression.py` C: v8 vs v7 primary → original Cine16 → v7 print, run as separate programs | **Bit-identical:** 51,408 samples over 63 combinations, including Off/Golden/None, Verona/Golden/None and Verona/Golden/Cinechrome 2383 |
| The comparison can detect a change | `cine16_regression.py` D: each profile vs the wrong profile | Every wrong pairing differs on 992 of 992 samples |
| Eight approved V6.1 stocks, prints, V1 controls, film adjustments | older regression dumps (see section 6) | **Bit-exact** |

"Bit-identical" means every float32 output channel is exactly equal. It is
not a "within tolerance" result.

## 2. How the Cine16 code was integrated

The Cine16 code was not rewritten.
[`tools/extract_cine16.py`](../tools/extract_cine16.py) generates the engine
from the original file (stored unchanged as
`reference/Cine16_1B_Color_Profile.dctl`). It copies the following verbatim:
- the constants
- the four profile tables (`PH`, `PS`, `PL`, `PG`)
- the helper functions
- the whole `transform()` body

It makes only these mechanical changes:

1. **Prefixes.** Every Cine16 identifier gets `c16_` / `C16_`. FilmLab already
   defines `DI_A`, `DI_B`, `DI_C` and `DI_M` (same values), and generic names
   such as `luma`, `sstep`, `PH` and `PS` could collide later.
2. **Controls.** The eight Cine16 UI globals (`p_strength`, `p_sep`, …) become
   fields of a `C16Controls` struct passed in by FilmLab. The argument is
   named `c16c` because the original body already has a local variable `c`.
3. **Entry point.** `transform()` becomes `c16_apply(R, G, B, profile, controls,
   showMask)`. It takes and returns DaVinci Wide Gamut / DaVinci
   Intermediate, exactly like the original.

Kept unchanged: expressions, operation order, the original NaN guard, and the
original final clamp of each channel to [−0.5, 2.0] DI.

## 3. FilmLab code changes

- **UI:** the *Secondary Film Look* dropdown (17 options: Off + 16 profiles in
  the original order) and seven sliders using the original Cine16 ranges,
  defaults and tooltip text:

  | Control | Range | Default |
  |---|---|---|
  | Secondary Strength | 0–1.5 | 1.00 |
  | Secondary Color Separation | 0–2 | 1.00 |
  | Secondary Saturation | 0–2 | 1.00 |
  | Secondary Subtractive Density | 0–2 | 1.00 |
  | Secondary Print Bias | 0–2 | 1.00 |
  | Secondary Profile Tone | 0–1.5 | 1.00 |
  | Secondary Skin Protection | 0–1 | 1.00 |

  These sit directly after Creative Intensity, following the processing order.
  No existing control, range, default or dropdown order changed.
- **Developer section:** the original *Show Skin Mask* is kept as
  *Dev - Secondary Skin Mask*. It shows the Cine16 skin mask of the image
  entering the secondary look, and only when a secondary look is selected.
  It can be removed before a customer release.
- **Engine split (no behaviour change):** V7's `glx_process_linear`, which ran
  look → print → adjustments → grading, is now `glx_process_post` (print →
  adjustments → grading) plus a `glx_process_linear` wrapper that applies the
  look first and then calls it. Operations and their order are identical,
  which the Secondary-Off = V7 dumps prove bit-for-bit. Two helpers,
  `glx_di_to_work` and `glx_work_to_di`, reproduce the entry point's existing
  DaVinci Intermediate ↔ working-space conversion for the secondary path.
- **Entry point:** one new block, entered only when Secondary Film Look is not
  Off:
  1. Primary film look. With no primary look, the input goes to Cine16
     **unconverted**. With one, DI is decoded to linear working, the look is
     applied, and the result is encoded back to DI.
  2. `c16_apply` (DI in, DI out).
  3. If any print, film adjustment or grading control is active: decode → print
     → adjustments → grading → encode.
  4. Global Intensity, using the same rule as V7.

  With Secondary Off this block is skipped and the V7 code runs unchanged:
  a true bypass.

Every engine can be called on its own:
- `glx_look_apply` (primary, linear working)
- `c16_apply` (secondary, DWG/Intermediate)
- `glx_print_apply` (print, linear working)
- `glx_process_post` (print + adjustments + grading)

## 4. Processing order

1. DWG / DaVinci Intermediate in.
2. Primary Film Look (Creative Intensity).
3. Secondary Film Look: the original Cine16 processing.
4. Film Print (Print Strength and print controls).
5. Film adjustments, then the Version 1 grading controls.
6. DWG / DaVinci Intermediate out.
7. Global Intensity: mix with the untouched input.

The only colour-space conversions are those the stages need. Cine16 works in DI
on DWG, the primary looks and prints in linear working space. With the primary
look Off and no print or grading, the secondary look runs on the input
directly with no conversion at all.

## 5. Behaviour notes

- **Neutral is not Off.** The Neutral profile is itself a gentle look: tone
  trim, saturation response and print bias. Use *Secondary Film Look = Off* for
  no secondary processing.
- **Secondary Strength 0 is not a bypass.** It runs the original engine, as the
  original does, which gives a near-identity result that still passes through
  Cine16's log/HSV round trip and clamp. Only Off is an exact bypass.
- **Range limiting (kept from the original):** Cine16 clamps each output
  channel to [−0.5, 2.0] DI, which is linear −0.048 to about 1.3 million
  (about 22.8 stops over 18% grey).
  - On 48,000 random inputs in the normal 0–1 DI range (linear 0–100), the
    clamp never engaged.
  - It acts only on extreme extended-range values: very negative channels from
    out-of-gamut colours, or values far above any camera's range.
  - When a print or grading control follows the secondary look, those stages
    receive the clamped values.
- **NaN guard (kept from the original):** if Cine16 produces NaN or Inf, it
  returns its own input unchanged.
- **Stacking:** primary and secondary looks add together. Verona 250D + Golden +
  Cinechrome 2383 at full strength is dense. Use Creative Intensity or
  Secondary Strength to balance combinations.
- **Stage View** (developer) still shows the primary stock alone, without
  secondary, print or grading.
- **Global Intensity 0** is an exact bypass, also with a secondary look.

## 6. Regression testing instructions

Run all of these from the repository; each must report bit-identical / pass.

```
# A-D: Cine16 match, Secondary Off = V7, chaining, sanity (about 30 s)
cd tests && python3 cine16_regression.py          # ends "ALL CINE16 REGRESSIONS BIT-IDENTICAL"

# FilmLab safety suite incl. secondary checks (NaN/Inf, clamp, Global Intensity 0,
# secondary controls inert when Off, skin mask), all 14 x 17 x 6 combinations
gcc -O2 -o math_test_v8 math_test_v8.c -lm && ./math_test_v8     # ends "ALL CHECKS PASSED"

# Secondary Off = V7 with every FilmLab control randomised
gcc -O2 -DDUMP_ALL_V7 -o a7 math_test_v8.c -lm && ./a7 > v7.txt
gcc -O2 -DDUMP_ALL_V8 -o a8 math_test_v8.c -lm && ./a8 > v8.txt && cmp v7.txt v8.txt

# Older regressions, now run against V8
for p in "DUMP_V1 DUMP_V4_CORE" "DUMP_V3_ADJ DUMP_V4_ADJ" "DUMP_V4_PRINT DUMP_V5_PRINT" "DUMP_DIAG_LOOKS DUMP_V6_LOOKS"; do
  set -- $p; gcc -O2 -D$1 -o ra math_test_v8.c -lm; gcc -O2 -D$2 -o rb math_test_v8.c -lm
  ./ra > ra.txt; ./rb > rb.txt; cmp ra.txt rb.txt && echo "$1 ok"; done
```

**Manual procedure for the requested reference check** (what section A automates):
1. Primary Film Look = Off.
2. Secondary Film Look = the profile under test (start with Golden).
3. Film Print = None; every other FilmLab control at its default.
4. Secondary sliders at the original defaults (1.00), Skin Mask off.
5. Feed identical DWG/Intermediate inputs to FilmLab V8 and to the original
   Cine16 DCTL (with the same profile and default sliders), and compare the
   outputs.

**Checks that require DaVinci Resolve (not executed here):**
- [ ] V8 compiles in Resolve Studio; the UI shows the Secondary section after
      Creative Intensity, with the original ranges and defaults.
- [ ] A/B in Resolve: original Cine16 node (Golden) against FilmLab (Off /
      Golden / None) on the same clip. The images should be identical (a
      difference matte should be black).
- [ ] Repeat for several profiles, and with non-default secondary sliders.
- [ ] Secondary Off on an existing V7 grade: no visible change.
- [ ] Verona 250D + Golden, and + Cinechrome 2383, look as intended on real
      footage.
- [ ] Dev - Secondary Skin Mask matches the original Show Skin Mask.
- [ ] Real-time playback performance with a secondary look active.
- [ ] Exported LUTs (`gradelux_lut secondaries …`, `cube … [secondary]`) match the
      DCTL in Resolve.

**Resolve compiler note:** the host tests compile the code as C. Resolve's
GPU compiler may contract multiply-adds differently from the host, so the
original Cine16 and FilmLab V8 could in principle differ by a last-bit
rounding inside Resolve. The code is the same; only Resolve can confirm this,
via the A/B difference test above.

## 7. LUT export

The secondary engine is a deterministic per-pixel RGB function: no spatial,
temporal or random processing. `tools/gradelux_lut.c` now samples any
combination:

```
gcc -O2 -o gradelux_lut tools/gradelux_lut.c -lm
./gradelux_lut cube 0 0 dwg 65 6 > Golden_DWG_65.cube          # secondary alone (Golden)
./gradelux_lut cube 0 0 rec709 33 6 > Golden_Rec709_33.cube
./gradelux_lut cube 1 1 dwg 65 6 > Verona_Golden_2383_DWG.cube  # primary + secondary + print
./gradelux_lut secondaries dwg 65 <dir>                          # all 16 profiles alone
./gradelux_lut validate 0 0 dwg 33 6                             # LUT vs DCTL error report
```

Secondary sliders stay at the original defaults (1.00) in exported LUTs. LUT
interpolation errors for secondary profiles are in the same range as for the
primary looks.
