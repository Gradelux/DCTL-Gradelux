# FilmLab V10: no film prints, 11 sliders

`Gradelux_FilmLab_v10.dctl` is FilmLab V9.1 with the film print engine removed:
- **Removed:** the Film Print dropdown, Print Strength, the five prints and their code.
- **Panel:** two dropdowns and eleven sliders.
- **Unchanged:** the 18 film looks, the 16 Cine16 profiles and every retained control. With
  Film Print = None, V10 output is bit-identical to V9.1 (section 4).

The three original LUT looks (Vista, Lux, Laguna) were added in an earlier V10 build and then
removed at your request. Neither the DCTL nor the repository contains any LUT data.

## 1. Interface

| Control | Range | Default |
|---|---|---|
| Exposure | −5 … +5 stops | 0 |
| Temperature | −100 … +100 | 0 |
| Tint | −100 … +100 | 0 |
| **Film Look** (dropdown, 19 options) | Off + Verona, Verdant, Midnight, Aurelia, Solara, Moss, Chroma, Silvergrain, Tungsten, Halcyon, Argent, Nocturne, Pastelle, Patina, Opaline, Glacier, Terracotta, Velour | Off |
| Film Strength | 0 … 200% | 100% |
| **Secondary Film Look** (dropdown, 17 options) | Off + the 16 original Cine16 profiles | Off |
| Secondary Strength | 0 … 1.5 | 1.00 |
| Film Contrast | −100 … +100 | 0 |
| Film Density | −100 … +100 | 0 |
| Subtractive Saturation | −100 … +100 | 0 |
| Highlight Roll-Off | 0 … 100 | 0 |
| Shadow Depth | −100 … +100 | 0 |
| Global Intensity | 0 … 100% | 100% |

- **Saved grades:** every control keeps its V9.1 internal name, range and default. Dropdown IDs
  are unchanged, so saved V9.1 grades keep their values.
- **Pipeline:** Exposure → Temperature / Tint → Film Look → Secondary Film Look (Cine16) →
  grading sliders → Global Intensity.
- **Data format:** DaVinci Wide Gamut / Intermediate in and out.

## 2. Removed

| Removed | How existing looks are preserved |
|---|---|
| Film Print dropdown and its five prints (Cinechrome 2383, Velachrome 2393, Lumachrome 3513, Retrochrome 3510, Silkchrome 2383) | Every look renders exactly as it did in V9.1 with Film Print = None (section 4) |
| Print Strength slider | — |
| Fixed print controls (`GLX_FIX_PRINT_*`) | Only used by the print engine |
| Print engine code (`glx_print_get`, `glx_print_controls`, `glx_print_apply`, the neutral-locked print variant of the tone stage, the `isPrint` field) | These paths never ran for looks, so the arithmetic of every look is unchanged |

The non-neutral V9 defaults fixed in V9.1 are still applied internally (the FIXED SETTINGS
section):
- the six Cine16 detail settings at 1.00;
- the film adjustments at their bypass values.

**Old grades:** a saved V9.1 grade that used a film print now renders without the print.

## 3. Unchanged

- **Looks:** all 18 film looks (data, IDs, names, middle-grey anchoring) and all 16 Cine16
  profiles. The Cine16 code is verbatim from the original DCTL and still takes all seven
  original controls.
- **LUT export:** every stage is a deterministic RGB → RGB function, so any configuration can
  still be exported as a LUT. Use Resolve's Generate LUT on a FilmLab node, or
  `tools/gradelux_lut.c`, which uses V9.1 (identical with Print None).

## 4. Tests executed on the host (all passed)

These compile the DCTL's own code as C.

| Test | Result |
|---|---|
| **V9.1 (Film Print = None) vs V10, identical settings** (`tests/v91_v10_equivalence.c`) | **319,067 outputs bit-identical** |
| — each look alone | 14,418 identical |
| — each Cine16 profile alone | 12,816 identical |
| — every look × secondary pair, including Off | 40,375 identical |
| — engines disabled | 1,602 identical |
| — Exposure −5 … +5 with every look | 25,536 identical |
| — each grading slider at ±50 / ±100 with every look | 24,320 identical |
| — 200,000 random settings of all 11 sliders and both dropdowns, extended-range input | 200,000 identical |
| Sensitivity: the same comparison with a deliberately tiny change | 294,054 of 319,067 differ, so the test detects changes |
| **Cine16 vs the original Cine16 DCTL** (`FILMLAB=10 python3 tests/cine16_regression.py`) | **328,432 samples bit-identical**; secondary inert when Off (68,514); Film Look Off = V8 (30,651); chained engines (51,408) |
| Interface (`tests/v10_interface_check.py`) | 2 dropdowns and 11 sliders in order; 19 Film Look options; no print, split-toning, diagnostic or extra film controls; no LUT looks or LUT data |

Because V10 equals V9.1 bit for bit and V9.1 equals V9, the V9 suite
(`tests/math_test_v9.c`) carries over.

**Commands:**
```
cd tests
gcc -O2 -DSRC_V91 -o eq91 v91_v10_equivalence.c -lm && ./eq91 > v91.txt
gcc -O2           -o eq10 v91_v10_equivalence.c -lm && ./eq10 > v10.txt && cmp v91.txt v10.txt
FILMLAB=10 python3 cine16_regression.py
python3 v10_interface_check.py
```

## 5. Resolve testing checklist (not performed: no Resolve here)

- [ ] V10 compiles in Resolve Studio. The panel shows exactly the 2 dropdowns and 11 sliders
      of section 1, and Film Look has 19 entries.
- [ ] A/B against V9.1 on the same clip, with V9.1 Film Print = None and matching settings:
      the difference matte should be black for several looks and secondaries.
- [ ] Old grades keep their values. Grades that used a film print now render without it.
- [ ] Playback performance, expected the same as V9.1 or slightly better.
- [ ] A render or delivery of a short clip matches the viewer.
