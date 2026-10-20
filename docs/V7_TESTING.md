# Gradelux FilmLab V7: regression and testing checklist

This file covers how V7 was checked against V6.1, how the five new stocks
were checked, and what still has to be checked by eye in DaVinci Resolve.

"Executed" means the test was run on the host (gcc, through the C shim that
compiles the `.dctl` itself) and passed for this commit. Nothing here was
run inside Resolve, and no real footage was graded.

## 1. V6.1 → V7 comparison (the eight approved stocks)

**What changed in the code:**
- Five new stock definitions, appended after Silvergrain 100.
- Five dropdown entries, appended in the same order.
- Header comments.

No shared function, existing stock parameter, print, control or default
changed. `diff Gradelux_FilmLab_v6_1.dctl Gradelux_FilmLab_v7.dctl` shows
only additions, apart from the version line and the appended dropdown line.

**How it was compared:** `tests/math_test_v7.c` compiles V6.1
(`-DDUMP_EIGHT_V61`) and V7 (`-DDUMP_EIGHT_V7`) and feeds both identical
inputs. Every output is printed bit-for-bit (`%a`). The inputs are:
- Off and the eight approved stocks, under all six print options (None and
  the five prints) and all four Stage View modes, on a fixed 9×9×9 DaVinci
  Intermediate grid (including out-of-range values).
- 100,000 random settings of every control: Creative Intensity 0–200%,
  print strength and print controls, film adjustments, V1 grading controls,
  and Global Intensity.

```
cd tests
gcc -O2 -DDUMP_EIGHT_V61 -o e61 math_test_v7.c -lm && ./e61 > v61.txt
gcc -O2 -DDUMP_EIGHT_V7  -o e7  math_test_v7.c -lm && ./e7  > v7.txt
cmp v61.txt v7.txt        # must print nothing
```

**Result (executed):** all 257,464 outputs are identical, with matching
SHA-1. This is exact equality, not equality within a tolerance.

**Sanity check (executed):** swapping Tungsten 500T into the compared set
produces 28,607 differing outputs, so the comparison does detect changes.

## 2. Checklist

| # | Check | How | Status |
|---|---|---|---|
| 1 | Eight approved stocks unchanged vs V6.1 | `DUMP_EIGHT_V61` vs `DUMP_EIGHT_V7`, `cmp` | **Executed: bit-identical** |
| 2 | Prints alone unchanged (vs V4 original) | `DUMP_V4_PRINT` vs `DUMP_V5_PRINT` | **Executed: bit-exact** |
| 3 | V1 grading controls unchanged | `DUMP_V1` vs `DUMP_V4_CORE` | **Executed: bit-exact** |
| 4 | Film adjustments unchanged (vs V3) | `DUMP_V3_ADJ` vs `DUMP_V4_ADJ` | **Executed: bit-exact** |
| 5 | Verona / Verdant / Midnight vs V5 diagnostic build | `DUMP_DIAG_LOOKS` vs `DUMP_V6_LOOKS` | **Executed: bit-exact** |
| 6 | Identity: Look Off + Print None, and Global Intensity 0 | `./math_test_v7` | **Executed: pass** |
| 7 | All 13 stocks × 6 print options: no NaN/Inf, no negatives from in-gamut colours, monotonic grey ramps, no hue-circle jumps | `./math_test_v7` | **Executed: pass** |
| 8 | Prints stay neutral on greys; Silvergrain stays exactly neutral with every print | `./math_test_v7` | **Executed: pass** |
| 9 | Hue map never folds at 100% and 200%, Separation −/0/+ | `./math_test_v7` | **Executed: pass** |
| 10 | Slider extremes (200% intensity, ±100 controls), all stock × print combinations | `./math_test_v7` | **Executed: pass** |
| 11 | Skin hue per stock, and over every stock × print pair | `./math_test_v7` | **Executed:** worst 6.2°, mean 2.9° |
| 12 | Developer Stage View: Full = Normal, Off identity, Tone Only keeps hue | `./math_test_v7` | **Executed: pass** |
| 13 | Synthetic scenarios (below), 13 stocks × 6 print options at 100% | `tools/scenario_check.c` | **Executed: pass** |
| 14 | Same scenarios at 200% (informational) | `scenario_check 200` | **Executed:** see notes |
| 15 | Distinctness: each new stock vs all 12 others, alone and over Cinechrome 2383 | `tools/look_visibility.c` | **Executed:** see section 4 |
| 16 | LUT export / validation for the new stocks (look only and with print) | `tools/gradelux_lut.c validate` | **Executed:** errors in the same range as approved stocks |
| 17 | DCTL compiles in DaVinci Resolve Studio (GPU compiler) | Resolve | **Requires Resolve** |
| 18 | Dropdown shows 13 stocks + Off in the right order, defaults unchanged | Resolve | **Requires Resolve** |
| 19 | Visual review of each new stock on real footage (section 3 list) | Resolve | **Requires Resolve** |
| 20 | Every print with every new stock on real footage | Resolve | **Requires Resolve** |
| 21 | Real-time playback performance | Resolve | **Requires Resolve** |
| 22 | Exported 33/65-point LUTs match the DCTL in Resolve | Resolve | **Requires Resolve** |
| 23 | Eight approved stocks look the same as V6.1 on a saved grade | Resolve (A/B a still) | **Requires Resolve** (numerically proven by #1) |

## 3. Synthetic scenario sets (executed) and real-footage checks (not executed)

`tools/scenario_check.c` builds synthetic stand-ins for the requested
footage types. It runs every stock × every print through the DCTL maths at
100% Creative Intensity and checks for NaN/Inf, negative outputs,
brightness reversals along exposure ladders, abrupt hue changes, skin hue
and added display clipping.

**Result (executed):** at 100%, zero failures for all 13 stocks under all
6 print options, and no stock adds display clipping compared with Off.

| Scenario | Synthetic content | Look for in Resolve (not yet done) |
|---|---|---|
| Portrait | 6 skin tones × −4..+4 stops, hair, lips, eyes | Skin hue and smoothness, no blotching; Halcyon not blue on skin; Pastelle pink/peach, not magenta |
| Daylight exteriors | Sky gradients, very bright clouds, grass, concrete, blue shade | Sky gradation, cloud roll-off, no banding in skies |
| Green foliage | Greens 60–170° hue, three saturations, −4..+1 stops | Halcyon cool, Pastelle mint, Tungsten natural; no neon greens |
| Warm interiors | Tungsten-lit scene, skin under tungsten, lamps up to +5 stops | Tungsten keeps practicals amber and skin natural; lamp roll-off smooth |
| Nighttime city | Ambient −9..−4 stops; amber, red and sodium practicals and neon at −1..+4 stops | Nocturne: blue-black shadows with detail, rich amber/red lights, controlled neon; Midnight vs Nocturne clearly different |
| Bright saturated colours | Rec.2020 primaries and secondaries, full-saturation sweeps | No clipping or posterising, no hue flips |
| High-contrast scenes | −10..+10 stop ramps (neutral and coloured), speculars | Argent blacks dense but not crushed, highlights silvery not clipped |

**Notes:**
- **200% Creative Intensity:** some stocks show steep hue changes in
  saturated neon sweeps. Counting only clearly coloured samples (above 15%
  chroma): Halcyon has 25 and Nocturne 8. The approved Midnight 800T already
  has 55 at 200%, so the new stocks are within the existing range. At 100%
  there are none.
- **DWG-edge column:** reports pure DWG primaries, which lie outside the
  spectral locus (imaginary colours). Approved stocks, and Off through the
  prints, show the same small reversals and negatives there. It is
  informational only.
- **Synthetic sets are not real images.** They test the maths over the
  colours and exposures those scenes contain, not how a grade looks on
  actual footage.

## 4. Distinctness of the new stocks (executed)

Mean CIELAB ΔE through a viewing transform, over a colour checker, skin set,
hue sweeps at three exposures and a grey ramp. About 2 is barely visible;
10+ is obvious.

| New stock | Closest stocks, alone | Visible change vs Off (colours) |
|---|---|---|
| Tungsten 500T | Verdant 8.0, Chroma 8.1, Verona 8.2 | 10.2 |
| Halcyon 400 | Verdant 8.7, Moss 9.3, Midnight 10.6 | 15.4 |
| Argent 200 | Aurelia 14.2, Moss 14.5, Silvergrain 14.5 | 23.8 |
| Nocturne 800 | Midnight 9.3, Moss 11.6, Verona 14.3 | 17.5 |
| Pastelle 400 | Aurelia 8.2, Verdant 9.7, Moss 11.6 | 10.5 |

Every new stock is at least ΔE 8.0 from every other stock on its own. The
closest pair in the whole collection is still the approved Verdant / Moss
(7.4). Over Cinechrome 2383, new-stock pairs are 7.65 or more apart.

## 5. Re-running everything

```
cd tests && gcc -O2 -o math_test_v7 math_test_v7.c -lm && ./math_test_v7       # ALL CHECKS PASSED
cd .. && gcc -O2 -o scenario_check tools/scenario_check.c -lm && ./scenario_check   # ALL SCENARIO CHECKS PASSED
gcc -O2 -o look_visibility tools/look_visibility.c -lm && ./look_visibility       # distinctness matrix
gcc -O2 -o gradelux_lut tools/gradelux_lut.c -lm && ./gradelux_lut validate 9 0 dwg 65
```

Run the section 1 dump comparison before every release.
