# Gradelux LUT Plan (Version 4)

This is the plan for selling the Gradelux looks, and optionally the film prints and look +
print pairs, as standalone `.cube` LUTs in two colour-space versions. No LUTs have been
generated or committed yet. The generator and validator below are ready, and their accuracy
has been measured.

| Set | Count | Pipeline |
|---|---|---|
| DWG | 23 | DaVinci Intermediate (DWG) → Gradelux look → DaVinci Intermediate (DWG) |
| Rec.709 | 23 | Rec.709 Gamma 2.4 → DWG / Intermediate → Gradelux look → Rec.709 Gamma 2.4 |

Both sets come from the same look and print definitions in `Gradelux_FilmLab_v4.dctl`.
The same tools can also produce **print-only** LUTs (5 per colour space) and **look + print**
LUTs (up to 23 × 5 per colour space) if those are wanted as products.

## 1. Single source of truth

There are three reference configurations:

| Config | Film Look | Film Print |
|---|---|---|
| A. Look only | the look | None |
| B. Print only | Off | the print |
| C. Look + Print | the look | the print |

In all three: Creative Intensity 100%, Print Strength 100%, Print Contrast / Density /
Saturation / Highlight Rolloff 0, and every other control at its default (Film Color Density
0, Film Color Separation 50, Highlight Warmth 0, Shadow Tone 0, all Version 1 controls
neutral, Global Intensity 100%).

In that configuration the DCTL runs only the look stage and/or the print stage:
DI decode → DWG → Rec.2020-primaries working space → look → print → back → DI encode. The
output is exactly the reference transform, bit-for-bit, because Global Intensity at 100%
returns the processed value directly.

Both stages are pure per-pixel RGB → RGB functions:
- no image coordinates, neighbouring pixels, time, randomness or resolution
- no grain, halation, bloom or other spatial effects

They can therefore be sampled exactly into a 3D LUT.

`tools/gradelux_lut.c` compiles the DCTL source itself and calls its `transform()` with the
reference configuration. The DCTL and the LUTs cannot drift apart: change a look in the
DCTL, regenerate, and the LUT follows.

## 2. Generator

```
gcc -O2 -o gradelux_lut tools/gradelux_lut.c -lm          # from the repo root

./gradelux_lut list                                         # look and print numbers
./gradelux_lut cube 1 0 dwg 65    > Gradelux_01_Pure_DWG_65.cube          # A: look only
./gradelux_lut cube 0 1 rec709 65 > Gradelux_Print2383_Rec709_65.cube     # B: print only
./gradelux_lut cube 2 1 dwg 65    > Gradelux_02_Ember_Print2383_DWG_65.cube  # C: look + print
./gradelux_lut all 0 dwg 65 out/                            # all 23 looks (A), one set
./gradelux_lut all 1 rec709 65 out/                         # all 23 looks + Print 2383 (C)
./gradelux_lut validate 2 1 dwg 33                          # accuracy report
```

Output format (standard Resolve / Adobe `.cube`):
- `TITLE "Gradelux <Look> (<colour space>)"`, then comment lines
- `LUT_3D_SIZE 33` or `65`
- N³ lines of `R G B` with 6 decimals, red index varying fastest, blue slowest
- no `DOMAIN_MIN` / `DOMAIN_MAX` lines, so the input domain is the default 0..1 per axis

File naming: `Gradelux_<NN>_<Look>[_<Print>]_<DWG|Rec709>_<33|65>.cube`.

## 3. Pipelines, transfer functions and matrices

### DWG LUTs
- **Input:** DaVinci Intermediate code values, sampled on a uniform 0..1 grid.
  - DI 0 = linear 0. DI 1.0 = linear ≈ 100 (about +9.1 stops over 18% grey).
  - 18% grey = DI 0.336.
- **Transform:** the reference configuration (look, print, or both), exactly as the DCTL computes it.
- **Output:** DaVinci Intermediate code values, written unclamped. Values slightly outside
  0..1 can occur for out-of-gamut colours, so the file preserves them.

DaVinci Intermediate (Blackmagic Design white paper):
```
A = 0.0075, B = 7.0, C = 0.07329248, M = 10.44426855
LIN_CUT = 0.00262409, LOG_CUT = 0.02740668
decode: V <= LOG_CUT ? V / M : 2^(V / C - B) - A
encode: L <= LIN_CUT ? L * M : (log2(L + A) + B) * C
```

### Rec.709 LUTs
1. Input: Rec.709 code values 0..1.
2. BT.1886 EOTF with black = 0: `L = V^2.4` (display-linear Rec.709).
3. Linear Rec.709 → linear DWG (3×3, both D65):
   ```
   0.5627674560 0.3235165887 0.1137159553
   0.0777546353 0.7495773462 0.1726680186
   0.0646691999 0.1919986920 0.7433321080
   ```
4. DaVinci Intermediate encode, then the reference transform (identical to the DWG LUT),
   then DaVinci Intermediate decode.
5. Linear DWG → linear Rec.709 (the exact inverse):
   ```
    1.8986148993 -0.7921761834 -0.1064387159
   -0.1689487865  1.4889757541 -0.3200269676
   -0.1215391606 -0.3156758531  1.4372150137
   ```
6. **Gamut and range mapping: hard per-channel clip to 0..1 in linear Rec.709.** This is
   the only clipping in either pipeline, and it is applied identically to all 23 looks.
7. Inverse EOTF: `V = L^(1/2.4)`.

The matrices are derived in double precision from the primaries (DWG R 0.8/0.313,
G 0.1682/0.9877, B 0.079/−0.1155; Rec.709 R 0.64/0.33, G 0.30/0.60, B 0.15/0.06; D65
white). Rows sum to 1, so neutrals stay neutral.

**Why display-linear is the right input for Rec.709 LUTs.** A Rec.709 Gamma 2.4 image decodes
to display light. A mid-grey card in graded Rec.709 sits near code 0.49, and 0.49^2.4 =
0.18. That is the same 18% grey every look pivots on, so the looks behave consistently
without assuming Rec.709 footage is scene-linear or inverting an unknown camera rendering.

**No tone mapping is added.** The Rec.709 LUT does not apply a DRT. It applies the same look
to a signal that is already display-referred, then clips to the legal range. Values the
look pushes above display white are clipped. The looks' own shoulders make this gentle in
practice.

## 4. Sampling domain and sizes

| Size | Points | Use |
|---|---|---|
| 33 | 35,937 | Standard size; fast; universally supported |
| 65 | 274,625 | Premium accuracy; recommended for the DWG set |

The domain is 0..1 per axis for both pipelines.
- **DWG:** out-of-domain inputs (negative DI, or DI above 1.0, i.e. linear above about 100)
  are clamped to the cube edge by the LUT host.
- **Rec.709:** the domain is the full legal signal range.

## 5. Measured accuracy (Version 4)

These are LUT vs exact engine results, using tetrahedral interpolation. Errors are absolute,
in output code values. Coverage:
- **DWG, look only:** all 23 looks.
- **Rec.709, look only:** 5 representative looks.
- **Print only:** all 5 prints.
- **Look + print:** 12 representative pairs, including the strongest (Vivid + 2393,
  Velvet-style density, Obsidian + 2393) and the softest.

Measured with `tools/gradelux_lut.c validate`.

| Config | Pipeline | Size | LUTs | Real colours mean / worst | Grey ramp worst | Skin mean / worst | Hue sweep worst | Full cube worst |
|---|---|---|---|---|---|---|---|---|
| A. Look only | DWG | 33³ | 23 | 0.0007 / 0.016 | 0.002 | 0.0021 / 0.012 | 0.013 | 1.07 |
| A. Look only | DWG | 65³ | 23 | 0.0003 / 0.007 | 0.001 | 0.0007 / 0.006 | 0.006 | 1.06 |
| A. Look only | Rec.709 | 33³ | 5 | 0.0017 / 0.102 | 0.008 | 0.0009 / 0.011 | 0.060 | 0.10 |
| A. Look only | Rec.709 | 65³ | 5 | 0.0006 / 0.069 | 0.004 | 0.0003 / 0.005 | 0.037 | 0.07 |
| B. Print only | DWG | 33³ | 5 | 0.0006 / 0.011 | 0.001 | 0.0023 / 0.011 | 0.006 | 0.13 |
| B. Print only | DWG | 65³ | 5 | 0.0002 / 0.007 | 0.000 | 0.0007 / 0.006 | 0.006 | 0.11 |
| B. Print only | Rec.709 | 33³ | 5 | 0.0014 / 0.096 | 0.007 | 0.0006 / 0.006 | 0.066 | 0.10 |
| B. Print only | Rec.709 | 65³ | 5 | 0.0005 / 0.071 | 0.002 | 0.0002 / 0.004 | 0.041 | 0.07 |
| C. Look + Print | DWG | 33³ | 12 | 0.0010 / 0.032 | 0.003 | 0.0051 / 0.032 | 0.014 | 0.41 |
| C. Look + Print | DWG | 65³ | 12 | 0.0004 / 0.016 | 0.001 | 0.0015 / 0.016 | 0.009 | 0.32 |
| C. Look + Print | Rec.709 | 33³ | 12 | 0.0016 / 0.135 | 0.009 | 0.0014 / 0.065 | 0.070 | 0.14 |
| C. Look + Print | Rec.709 | 65³ | 12 | 0.0006 / 0.091 | 0.005 | 0.0004 / 0.047 | 0.042 | 0.10 |

What the test sets contain:
- **Real colours (DWG):** random colours inside Rec.2020, from −8 to +6 stops around grey,
  converted to DI.
- **Real colours (Rec.709):** random colours over the whole 0..1 cube.
- **Skin tones and hue sweep:** built as the same real colours for both pipelines.

For scale, 0.001 in DaVinci Intermediate is about 0.014 stop. Mean errors on real colours are
far below visibility in every configuration.

**Recommendation:**
- **65³ for all DWG LUTs and for every look + print LUT.**
- 33³ is fine for single looks and single prints.
- Look + print at 33³ has a higher skin-tone error (mean ≈0.005, about 0.07 stop). At 65³
  that drops to ≈0.0015, about 0.02 stop.

The reason: many colour operators depend on saturation, and in a log-encoded lattice
neighbouring 33³ points are about 0.43 stop apart per channel. Stacking a look and a print
compounds this curvature, and 65³ halves the spacing.

**Rec.709 worst cases** happen where the exact output has a channel clipped to 0: very
saturated greens and cyans outside Rec.709. The gamma-2.4 encode is extremely steep there,
so the interpolated code value can read about 0.09–0.13 where the exact value is 0. In light,
that is about 0.004–0.007 linear, under 1% of the pixel's brightest channel. Ways to reduce it
in a later version, if wanted:
- a smooth gamut compression into Rec.709 instead of the hard clip
- a 1D shaper, if the target hosts' support for it is verified first

The **full DWG cube** includes imaginary colours far outside the spectral locus (e.g. DI
(0, 0.76, 0.99): linear R 0, G ≈10, B ≈80). There the engine stays finite and smooth, but the
lattice can't follow its steepness: the worst error reaches about 1.1 for Vivid, with mean
errors still ≤0.002. No camera produces these colours; they are listed only for completeness.

## 6. Validation procedure for each release

1. **Regenerate** both sets at the chosen size from the tagged DCTL version.
2. **Numerical check:** run `gradelux_lut validate <look> <dwg|rec709> <size>` for all 23
   looks and confirm the errors match the table above.
3. **In Resolve, DWG LUTs:**
   - Put the DCTL (reference configuration) and the LUT on parallel nodes with the same input.
   - Compare with a **Difference** composite, or a Layer Mixer set to Difference with gain
     raised, on:
     - a colour chart
     - Generators → Grey Scale and a hue gradient
     - skin tones (light, medium, deep)
     - real footage: daylight, tungsten, night, LEDs and neon
   - Expect differences near zero apart from extreme saturated lights.
4. **In Resolve, Rec.709 LUTs:**
   - Build the same chain manually: CST Rec.709 Gamma 2.4 → DWG/Intermediate (tone mapping
     None, gamut mapping None), then the DCTL, then CST back to Rec.709 Gamma 2.4 (same
     settings).
   - Compare against the LUT alone, as above.
   - Expect a match except where the manual chain keeps out-of-range values that the LUT
     clips.
5. **Verify host behaviour; don't assume it:**
   - Resolve's handling of DWG LUT output values outside 0..1 (kept or clamped).
   - LUT interpolation (tetrahedral vs trilinear).
   - How inputs outside the domain are handled.
   - Repeat the checks in at least one other host the LUTs will be sold for (e.g. Premiere,
     Final Cut), especially for the Rec.709 set.
6. **Visual QA:** check each look at both sizes for banding in sky gradients and skin, and for
   posterization on saturated lights.

## 7. Known limitations

- **Bounded domain (DWG):** inputs above DI 1.0 (about +9 stops) or below 0 are clamped by
  the LUT. The DCTL handles these values; a LUT cannot.
- **Interpolation:** any 3D LUT approximates the engine between lattice points. The 65³
  size halves the error of 33³.
- **Rec.709 clip:** colours the look pushes outside Rec.709, or above display white, are
  hard-clipped per channel. That can slightly change the hue of extremely saturated
  out-of-gamut colours.
- **No user controls:** a LUT bakes the reference configuration only. Creative Intensity,
  Print Strength, the print controls, the film adjustments and the grading controls are
  DCTL-only. To deliver variants, generate extra LUTs with different settings in the
  generator.
- **Monochrome Rec.709 LUTs** (and print-only LUTs on neutral input) are exactly neutral
  before the 709 conversion, and stay neutral because the matrices map neutrals to neutrals.
