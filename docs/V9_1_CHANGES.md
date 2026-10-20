# FilmLab V9.1: simplified interface

`Gradelux_FilmLab_v9_1.dctl` is FilmLab V9 with a smaller interface. The creative engine was not
rebuilt.
- **Unchanged:** every film look, secondary profile and print, and the pipeline order.
- **Verified:** V9.1 output equals V9 bit for bit at the same settings (section 4).

## 1. Interface

Three dropdowns and twelve sliders, in this order:

| # | Control | Range | Default |
|---|---|---|---|
| | Exposure | −5 … +5 stops (0.1) | 0 |
| | Temperature | −100 … +100 | 0 |
| | Tint | −100 … +100 | 0 |
| ▾ | **Film Look** (18 looks + Off) | | Off |
| | Film Strength *(formerly Creative Intensity; same behaviour)* | 0 … 200% | 100% |
| ▾ | **Secondary Film Look** (16 Cine16 profiles + Off) | | Off |
| | Secondary Strength | 0 … 1.5 (unchanged) | 1.00 |
| ▾ | **Film Print** (5 prints + None) | | None |
| | Print Strength | 0 … 100% | 100% |
| | Film Contrast | −100 … +100 | 0 |
| | Film Density | −100 … +100 | 0 |
| | Subtractive Saturation | −100 … +100 | 0 |
| | Highlight Roll-Off | 0 … 100 | 0 |
| | Shadow Depth | −100 … +100 | 0 |
| | Global Intensity | 0 … 100% | 100% |

Every retained control keeps its V9 internal name, range, default and processing. Film
Strength is the former Creative Intensity control under a new label.

## 2. Removed controls and how their defaults are preserved

Every removed control is replaced by its exact V9 default, written as a fixed constant in the
DCTL (section FIXED SETTINGS) and passed to the same code that used the slider before.

| Removed control | V9 default | Preserved as | Effect at that default |
|---|---|---|---|
| Secondary Color Separation | 1.00 | `GLX_FIX_SEC_SEP 1.00` → Cine16 engine | Original Cine16 default; still applied |
| Secondary Saturation | 1.00 | `GLX_FIX_SEC_SAT 1.00` → Cine16 engine | still applied |
| Secondary Subtractive Density | 1.00 | `GLX_FIX_SEC_SUBTR 1.00` → Cine16 engine | still applied |
| Secondary Print Bias | 1.00 | `GLX_FIX_SEC_BIAS 1.00` → Cine16 engine | still applied |
| Secondary Profile Tone | 1.00 | `GLX_FIX_SEC_TONE 1.00` → Cine16 engine | still applied |
| Secondary Skin Protection | 1.00 | `GLX_FIX_SEC_SKINPROT 1.00` → Cine16 engine | still applied |
| Print Contrast / Density / Saturation / Highlight Rolloff | 0 | `GLX_FIX_PRINT_*` 0 → `glx_print_controls` | Same call with the same values as V9 |
| Film Color Density | 0 | `GLX_FIX_COLOR_DENSITY 0` | Adjustment stage skipped (exact bypass, as in V9) |
| Film Color Separation | **50** | `GLX_FIX_SEPARATION 50` (computes to exactly 0) | as above |
| Film Highlight Warmth | 0 | `GLX_FIX_HL_WARMTH 0` | as above |
| Film Shadow Tone | 0 | `GLX_FIX_SHADOW_TONE 0` | as above |
| Shadow / Midtone / Highlight Hue | 220 / 35 / 45 | — | No effect while every saturation is 0 |
| Shadow / Midtone / Highlight Saturation | 0 | stage removed | Split toning never ran at these defaults (exact bypass) |
| Split Tone Balance / Strength | 0 / 100 | stage removed | as above |
| Dev - Stage View | Normal | branch removed | Normal = full processing |
| Dev - Secondary Skin Mask | off | branch removed (`c16_apply` called with 0) | off = normal rendering |

**Code removed:**
- the split-toning engine (`GlxSplit`, `glx_split_make`, `glx_split_tone`)
- the Stage View branch of the entry point
- the skin-mask return

**Code kept:** every film-engine function, including:
- the film-adjustment code inside `glx_process_post`, which now runs with its fixed neutral
  values
- `glx_look_apply_parts`, which the middle-grey anchor uses
- the Cine16 engine, unchanged; it still takes all seven original controls

**Performance:** each pixel runs the same film-engine work as V9 at the same settings, minus
the removed branch tests.

## 3. Unchanged

- **Looks and prints:** all 18 primary looks, 16 Cine16 profiles and 5 prints (names, order,
  internal IDs and data), plus the middle-grey anchoring.
- **Pipeline:** Exposure → Temperature / Tint → primary look → Cine16 → print → grading →
  Global Intensity. The primary, secondary and print selections remain independent.
- **Data format:** DaVinci Wide Gamut / Intermediate input and output; RGB-only,
  LUT-compatible processing.

## 4. Regression tests

### Executed on the host (C shim compiling both .dctl files; all passed)

| Test | Result |
|---|---|
| **V9 vs V9.1, identical inputs and identical settings of every retained control** (`tests/v9_v91_equivalence.c`) | **358,754 outputs bit-identical** |
| — every primary look alone (18) | 14,418 identical |
| — every secondary profile alone (16) | 12,816 identical |
| — every print alone (5) | 4,005 identical |
| — every primary × secondary pair (288) | 36,000 identical |
| — every primary × print pair (90) | 11,250 identical |
| — every primary × secondary × print triple incl. Off (1,938) | 52,326 identical |
| — engines disabled (Off / Off / None, Film Strength 0, Print Strength 0) | 2,403 identical |
| — Exposure −5 … +5 in 0.5-stop steps with every look | 25,536 identical |
| — 200,000 random settings of all 12 sliders and 3 dropdowns, extended-range input DI −0.6 … 2.6 | 200,000 identical |
| Sanity: the comparison detects a change (V9 with Secondary Profile Tone 0.95, or Film Color Separation 55) | 257,180 / 325,672 outputs differ, so the test is sensitive |
| Identity: every engine disabled, strengths 0, or Global Intensity 0 | 800,000 extended-range samples return the input exactly |
| Cine16: V9.1 vs the original Cine16 DCTL (`FILMLAB=9.1 python3 cine16_regression.py`) | Bit-identical, 328,432 samples (all 16 profiles, all input categories, Secondary Strength varied); chaining Primary → Cine16 → Print bit-identical; with Film Look Off, V9.1 = V8 |
| Interface | 15 `DEFINE_UI_PARAMS`: 3 dropdowns + 12 sliders; no reference to any removed control remains |
| Tools | LUT, preview, distinctness and scenario tools build against V9.1; LUT files and preview images byte-identical to V9 |

V9 itself passed the full suite (`tests/math_test_v9.c`). Because V9.1 is bit-identical to V9
for every retained setting, those results carry over.

Commands:
```
cd tests
gcc -O2 -DSRC_V9 -o eq9 v9_v91_equivalence.c -lm && ./eq9 > v9.txt
gcc -O2 -o eq91 v9_v91_equivalence.c -lm && ./eq91 > v91.txt && cmp v9.txt v91.txt
FILMLAB=9.1 python3 cine16_regression.py
```

### Still required in DaVinci Resolve (not performed)

- [ ] V9.1 compiles in Resolve Studio; the panel shows exactly the three dropdowns and twelve
      sliders above.
- [ ] A/B against V9 on the same clip with matching settings: a difference matte should be
      black for several looks, secondaries and prints.
- [ ] Saved V9 grades: retained controls keep their internal names, so their values should
      carry over (Film Strength is the former Creative Intensity). Values of removed controls
      are ignored; a grade that used non-default values for them will render differently and
      needs regrading.
- [ ] Playback performance (expected the same as V9).
