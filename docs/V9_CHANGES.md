# FilmLab V9: colour science, exposure control and new film looks

`Gradelux_FilmLab_v9.dctl` is built on FilmLab V8 (the version in this repository; no newer V8
upload was attached). Nothing was rebuilt from scratch.

- **Unchanged:** the Cine16 engine, the five prints, every existing control, and the
  DWG / DaVinci Intermediate colour management.
- **Primary film looks:** these changed in two deliberate ways: middle-grey anchoring, and
  the Argent / Nocturne redesigns.

## 1. Changelog

| Area | Change |
|---|---|
| **Exposure anchoring** | Every primary look now maps 18% grey to 18% grey at neutral settings (section 2). |
| **Argent** | Redesigned as a dramatic, contrast-heavy bleach-bypass look (section 3). |
| **Nocturne** | Redesigned: no global darkening, open toe, cool ambience, warm practicals (section 3). |
| **Names** | Stock numbers removed from both dropdowns (section 4). Internal IDs and dropdown positions unchanged. |
| **New looks** | Patina, Opaline, Glacier, Terracotta, Velour (dropdown positions 14–18). |
| **Exposure** | −5 … +5 stops, scene-linear gain, before the film engines. |
| **Temperature / Tint** | −100 … +100, creative white balance by chromatic adaptation, before the film engines. |
| **Split Toning** | Shadow / midtone / highlight hue and saturation, Balance, Strength; after grading, before Global Intensity. |
| **Processing order** | Exposure → Temperature / Tint → primary look → Cine16 → print → film adjustments → grading → split toning → Global Intensity, identical in the primary-only and secondary-enabled paths. |
| **UI order** | Primary Corrections → Primary Film Look → Secondary Film Look → Film Print → Film Adjustments → grading → Split Toning → Global Intensity → developer controls. No range or default of an existing control changed. |
| **Tools** | One shared C shim (`tools/filmlab_shim.h`); LUT, preview, distinctness and scenario tools target V9. The LUT tool supports 18 looks: its configuration packing was widened, because the old packing silently mapped looks 16–18 onto other configurations. The preview tool accepts control settings per tile. |
| **Tests** | `tests/math_test_v9.c`, `tests/cine16_regression.py` (now V9), `tests/v8_v9_report.py`. |

## 2. Exposure anchoring

**Diagnosis.**
- Every grey shift between primary looks came from one parameter: the stock's print-down
  density (`midDensity`, set through `glx_set_render`).
- It was applied as one uniform gain, then multiplied by the curve's slope at grey.
- V8 middle grey therefore moved by:
  - Nocturne −0.71 stops
  - Midnight −0.40
  - Pastelle +0.20
  - Halcyon −0.19
  - Verona −0.16
  - Moss −0.13
  - Chroma −0.12
  - Tungsten +0.08
  - Verdant −0.08
  - Aurelia −0.04
- These were already anchored at grey:
  - the characteristic curves (toe, shoulder, per-channel response)
  - zone tints, hue density and dye coupling
  - the prints (0.00 at grey)

**Fix.**
1. **Black density.** The print-down density is now a black density: the same neutral gain
   at full strength below about −4 stops, fading smoothly (C1) to exactly zero at 18% grey
   and above. Each look keeps its black depth, but grey and highlights are no longer pulled
   down. The extra slope it adds below grey is at most 0.375 × |density|, so the curve stays
   strictly increasing.
2. **Exact anchor.** Each stock is evaluated once on a neutral 18% grey pixel. Its result
   gets the single neutral gain that returns that grey to 18% luminance. That gain is at
   most ±0.04 stops; it removes the second-order effect of a stock's midtone tint and
   per-channel response on grey.

**Result (executed).**
- Every primary look maps grey to 18% within 0.000001 stops, at 50%, 100% and 200%
  Creative Intensity (`math_test_v9.c`, section 8a).
- Contrast at grey, toe shape, shoulder and colour are unchanged.
- Prints are untouched: they never used the parameter, and their outputs are bit-identical
  to V8.

**Expected differences for the 11 approved looks whose parameters did not change**
(`tests/v8_v9_report.py`, identical inputs; grey change in stops, v9 − v8):

| Look | −6 | −4 | −2 | 0 | +2 | +4 | +6 | colour dE | dL* | dC* | hue |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Verona | −0.00 | +0.01 | +0.10 | +0.16 | +0.10 | +0.02 | +0.00 | 0.77 | +0.33 | +0.07 | 0.22° |
| Verdant | −0.00 | +0.01 | +0.05 | +0.08 | +0.05 | +0.01 | +0.00 | 0.39 | +0.17 | −0.06 | 0.10° |
| Midnight | −0.01 | +0.03 | +0.24 | +0.40 | +0.20 | +0.03 | −0.00 | 1.41 | +0.64 | +0.30 | 0.34° |
| Aurelia | −0.00 | −0.00 | +0.02 | +0.04 | +0.02 | +0.00 | −0.00 | 0.19 | +0.07 | +0.00 | 0.05° |
| Solara | −0.01 | −0.01 | −0.01 | −0.01 | −0.01 | −0.01 | −0.01 | 0.09 | −0.05 | −0.01 | 0.03° |
| Moss | −0.00 | +0.01 | +0.09 | +0.13 | +0.09 | +0.04 | +0.01 | 0.77 | +0.38 | −0.05 | 0.19° |
| Chroma | +0.00 | +0.01 | +0.07 | +0.12 | +0.05 | +0.01 | +0.00 | 0.36 | +0.19 | +0.14 | 0.07° |
| Silvergrain | −0.01 | −0.01 | −0.01 | −0.01 | −0.01 | −0.01 | −0.01 | 0.06 | −0.06 | −0.00 | mono |
| Tungsten | −0.00 | −0.01 | −0.05 | −0.08 | −0.05 | −0.02 | −0.01 | 0.36 | −0.18 | −0.08 | 0.09° |
| Halcyon | −0.02 | +0.00 | +0.11 | +0.19 | +0.11 | +0.02 | −0.01 | 0.70 | +0.34 | +0.10 | 0.20° |
| Pastelle | −0.01 | −0.02 | −0.12 | −0.20 | −0.14 | −0.06 | −0.02 | 1.06 | −0.48 | +0.07 | 0.41° |

How to read the table:
- **Tonal only.** The change peaks at grey, fades out in the deep shadows and the highlight
  shoulder, and is almost entirely lightness: chroma moves less than 0.5 C* and hue less
  than 0.5°.
- **Mapping kept.** Every V9 look is still closest to the same-numbered V8 look.
- **Middle-pulled looks lighten.** Looks that were printed down (Midnight, Halcyon, Verona,
  Moss, Chroma) render the middle of the image lighter.
- **Pastelle and Tungsten darken slightly.** They had been printed up.

**Consequence for distinctness.** Midnight was partly distinct by being darker overall. After
anchoring, the closest approved pairs are:
- Midnight / Verdant: ΔE 6.4 (V8: 8.7)
- Tungsten / Midnight: ΔE 6.6
- Moss / Verdant: ΔE 6.9 (V8: 7.4)

Their colour identities are unchanged. Re-separating them would mean changing approved
looks beyond the anchoring, so it was not done.

## 3. Argent and Nocturne

Measured at 100%, no print; colour = display colourfulness vs source.

| | Mid contrast | −4 stops | +2 stops | +4 stops | Colour |
|---|---|---|---|---|---|
| Argent V8 | 1.53 | −1.98 | +0.13 | −1.39 | 0.40× |
| **Argent V9** | **1.72** (highest) | −2.62 | **+0.64** | **−0.66** | **0.49×** |
| Nocturne V8 | 1.50 | −2.44 (grey −0.71) | −0.11 | −1.26 | 0.87× |
| **Nocturne V9** | **1.50** | **−1.72** (grey 0.00) | +0.39 | −0.86 | 0.77× |

**Argent**
- **Tone:** the steepest midtone slope of the collection, deep blacks, and brighter, better
  defined highlights on a later, smooth shoulder.
- **Colour:** restrained colour carried by dye density, with warm hues reduced more than cool
  ones and steel-cyan blues. Only a light per-layer response, so contrast rises in
  luminance.

**Nocturne**
- **Tone:** anchored at grey with an open toe: rich blacks, visible lower mids, about 0.7
  stops less crush.
- **Colour:**
  - Shadows are blue and heavily desaturated.
  - Greens, cyans and blues turn quiet and teal.
  - Reds and ambers stay rich.
  - Above about +1 stop the colour warms, so practicals glow amber against the night.
  - Neon is held down.
- **Engineering:** the warm highlight tint saturates (stops growing) above the highlight
  zone. An earlier draft let it keep growing, which made green and blue fall slightly in
  very bright highlights at 200% Creative Intensity; that was found by the tests and fixed.
- **Distinctness:** distinct from Midnight (cyan-blue stylised night).

## 4. Names (labels only; positions and processing unchanged)

| Primary Film Look | Secondary Film Look |
|---|---|
| 0 Off · 1 Verona · 2 Verdant · 3 Midnight · 4 Aurelia · 5 Solara · 6 Moss · 7 Chroma · 8 Silvergrain · 9 Tungsten · 10 Halcyon · 11 Argent · 12 Nocturne · 13 Pastelle · **14 Patina · 15 Opaline · 16 Glacier · 17 Terracotta · 18 Velour** | Off · Neutral · Warm · Cool · Pastel · Rich · Golden · Faded · Reversal · **Night** · **Daylight** · **Portrait** · **Chrome** · **Emerald** · Steel Chrome · **Vivid** · Cinema E |

The internal identifiers (`GLX_LOOK_VERONA_250D`, `GLX_SEC_NIGHT_500T`, …) are kept, so
dropdown indices cannot shift. The five print names are unchanged.

## 5. New primary looks

Columns as in section 3; nearest is the closest other stock (mean ΔE). Every new look keeps
middle grey exactly at 18%.

| Look | Mid contrast | −4 / +4 stops | Colour | Identity | Nearest |
|---|---|---|---|---|---|
| **Patina** | 1.37 | −1.63 / −0.94 | 0.76× | Vintage colour negative: green-cast shadows, yellow-green mids, warm highlights; reds turn reddish-brown, yellows lean green, greens olive, blues faded teal; rich blacks. Not golden, blacks not lifted (unlike Solara). | Moss 7.3 |
| **Opaline** | 1.42 | −1.66 / −0.62 | 0.63× | Luxury editorial: fully protected natural skin against refined pastels (cool sage greens, soft lavender blues, all lightened by density); pearl-neutral creamy highlights; clean shadows. | Aurelia 7.5 |
| **Glacier** | 1.37 | −1.46 / −0.44 | 0.65× | Cold cinematic: icy cyan shadows and lower mids, cool-neutral highlights on a long bright shoulder, saturated luminous cyans, greens pushed cyan and held quiet, warm colours strongly restrained. | Halcyon 7.7 |
| **Terracotta** | 1.39 | −1.68 / −0.80 | 0.96× | Warm desert: burnt-orange and earthy red (reds and yellows both turn orange, dense), golden highlights, deep warm-brown shadows, muted olive greens, sunlit skin. | Verona 7.6 |
| **Velour** | 1.40 | −1.83 / −0.39 | 0.92× | Rich negative: crimson, emerald and cobalt rendered dark and dense at restrained saturation, plum (velvet) shadow tone, luminous neutral highlights, strong midtone separation. | Tungsten 7.6 |

All are at least ΔE 7.2 from every other stock, also over Cinechrome 2383. Skin hue shift
per look is ≤ 5.2°; over every stock × print pair the worst is 6.7° (Velour + Velachrome
2393) and the mean 3.1°.

## 6. New controls

**Exposure** (−5 … +5 stops, default 0, step 0.1)
- One gain of 2^stops on all channels in scene-linear light (linear working space), before
  the film engines.
- Colour ratios are kept exactly; negative values keep their sign; there is no clipping.
- 0 is an exact bypass.

**Temperature / Tint** (−100 … +100, default 0)
- Chromatic adaptation (von Kries scaling in CAT02 cone space; one 3×3 matrix in linear
  light) from D65 to a target white:
  - Temperature moves the target along the CIE daylight locus in mired steps (about
    15,700 K at −100, 4,100 K at +100), measured relative to D65, so 0 is exactly D65.
  - Tint moves it perpendicular to the locus by up to 0.025 Duv.
- White keeps its luminance; neutrals take on the target colour.
- Positive = warmer / more magenta.
- These are creative controls, not calibrated Kelvin or camera white balance.
- 0 / 0 is an exact bypass.

**Split Toning** (after grading, before Global Intensity)
- **Controls:** Shadow / Midtone / Highlight Hue (0–360°, defaults 220 / 35 / 45) and
  Saturation (0–100, default 0), Balance (−100 … +100), Strength (0–100, default 100).
- **Masks:** from the exposure of the tone norm. Highlights start at +0.5 stops and are
  full at +3.5; shadows are the mirror image; midtones take the rest. The masks are smooth
  and sum to 1; Balance shifts them by up to ±2 stops.
- **Tints:** applied as per-channel gains in log space with the luminance component removed,
  then one neutral gain restores each pixel's exact luminance.
- **Skin:** receives half the tint.
- **Safety and bypass:** no clipping, black stays black. All saturations 0, or Strength 0,
  is an exact bypass.
- **Monochrome:** it tints Silvergrain (split-toned black and white). Exposure /
  Temperature / Tint keep Silvergrain exactly neutral.

**Notes**
- **Developer Stage View** shows the primary stock alone, without corrections, secondary
  look, print, grading or split toning.
- **Effect on the Cine16 path.** Exposure and white balance now act before Cine16.
  - Cine16's tonal bands are fixed in DaVinci Intermediate, so a non-zero Exposure changes
    which band a pixel falls into, exactly as feeding it brighter or darker footage would.
  - With all corrections at 0 and Primary Off, Cine16 still receives the input unconverted
    and matches the original DCTL bit for bit.

## 7. Cine16 secondary engine

The Cine16 code and data are unchanged. Primary Off / Secondary X / Print None matches the
original `Cine16_1B_Color_Profile.dctl` bit for bit for all 16 profiles (section 9). Its grey
behaviour is documented here, not altered:

| Profile | −6 | −4 | −2 | 0 (grey) | +2 | +4 | +6 |
|---|---|---|---|---|---|---|---|
| Neutral | +0.05 | −0.02 | −0.01 | +0.02 | +0.02 | +0.05 | +0.06 |
| Warm | +0.27 | +0.02 | +0.01 | +0.07 | +0.07 | +0.15 | +0.14 |
| Cool | −0.18 | −0.16 | −0.13 | −0.03 | +0.05 | +0.10 | +0.15 |
| Pastel | +0.63 | +0.27 | +0.13 | +0.01 | −0.08 | −0.17 | −0.26 |
| Rich | +0.03 | −0.11 | −0.10 | +0.02 | +0.11 | +0.25 | +0.34 |
| Golden | +0.34 | −0.09 | −0.09 | +0.12 | +0.21 | +0.45 | +0.52 |
| Faded | +1.46 | +0.78 | +0.42 | +0.07 | −0.18 | −0.25 | −0.37 |
| Reversal | −0.32 | −0.36 | −0.31 | −0.00 | +0.28 | +0.59 | +0.87 |
| Night | +0.05 | −0.24 | −0.22 | +0.04 | +0.22 | +0.57 | +0.72 |
| Daylight | +0.36 | −0.07 | −0.11 | +0.11 | +0.29 | +0.57 | +0.75 |
| Portrait | +0.63 | +0.25 | +0.17 | +0.07 | −0.06 | −0.09 | −0.17 |
| Chrome | −0.22 | −0.49 | −0.42 | +0.05 | +0.43 | +0.86 | +1.22 |
| Emerald | +0.51 | +0.15 | +0.06 | +0.06 | +0.09 | +0.10 | +0.14 |
| Steel Chrome | −0.08 | −0.29 | −0.29 | +0.03 | +0.35 | +0.75 | +1.07 |
| Vivid | −0.64 | −0.49 | −0.40 | −0.04 | +0.28 | +0.61 | +0.93 |
| Cinema E | +0.61 | +0.32 | +0.21 | +0.02 | −0.14 | −0.27 | −0.41 |

(Grey luminance change in stops at exposure, Primary Off, Print None.)
- **Middle grey:** stays within −0.04 … +0.12 stops (largest: Golden +0.12, Daylight +0.11).
- **Main effect:** the profiles mostly reshape shadows and highlights; for example Chrome,
  Reversal and Steel Chrome brighten highlights by +0.6 … +0.9 stops at +4.

**Hard output range (kept unchanged).** Cine16 clamps every output channel to
[−0.5, 2.0] DaVinci Intermediate, i.e. linear −0.048 … about 1.3 million (22.8 stops over
grey).
- **Normal footage:** on 48,000 random inputs in the normal 0–1 DI range (linear 0–100) the
  clamp never engaged.
- **Where it acts:**
  - strongly out-of-gamut colours whose channels go below linear −0.048 (possible with
    saturated DWG data, e.g. some blues and neon lights);
  - values far beyond any camera's range.
- **Downstream:** when a print or grading follows, those stages receive the clamped values.

**Recommendation (not implemented).** A future optional Cine16 extended-range mode could
replace the hard lower limit with a soft one (asymptotic, like the primary engine's gamut
limit), so very negative out-of-gamut channels roll off instead of stopping abruptly. It
would have to remain off by default so the original appearance and the regression match are
kept.

## 8. LUT export

All new engines are deterministic per-pixel RGB functions: no spatial, temporal or random
processing.
- **Defaults stay out of LUTs.** `tools/gradelux_lut.c` builds every LUT with all controls at
  their defaults, so Exposure, Temperature, Tint and Split Toning are not baked into
  standard exports.
- **Configurations:** it exports primary looks, secondary profiles, prints and combinations,
  as DWG / Intermediate or Rec.709 Gamma 2.4, at 33 or 65 points (`validate` reports LUT vs
  DCTL error).
- **Custom variants** would need a deliberate tool extension.

## 9. Regression tests

### Executed on the host (C shim compiling the .dctl; all passed)

| Test | Result |
|---|---|
| 18 primary dropdown positions map to the intended stocks | Labels and internal IDs checked; each of the 13 V8 looks is closest to its own V8 counterpart (`v8_v9_report.py`); the LUT / preview tools list 18 looks |
| Cine16 mapping and appearance | All 16 profiles bit-identical to the original Cine16 DCTL (786,272 samples, incl. random / extreme sliders and skin mask); a deliberately wrong mapping is detected for every profile |
| Five prints functional and unchanged | Prints alone bit-exact vs V4 / V8; prints with every look checked for NaN, neutrality, monotonic ramps, hue continuity |
| Identity with every control neutral | Look Off + Print None + all neutral = input exactly; Global Intensity 0 = input exactly, with every control random |
| Look Off: V9 = V8 | Bit-identical over 184,986 outputs (every print, secondary profile, Stage View, and random settings of every existing control) |
| Secondary controls inert when Off | Bit-identical over 411,084 samples (19 look options × 6 prints) |
| Chaining (Primary → Cine16 → Print) | V9 equals its own stages run as separate programs (51,408 samples), incl. Off / Golden / None, Verona / Golden / None, Verona / Golden / Cinechrome 2383 |
| Middle-grey anchoring | Worst grey shift 0.000001 stops (18 looks × Creative Intensity 50 / 100 / 200%) |
| Exposure | Exact 2^stops gain (max relative error 1.1e-6), no sign flips, ±5 stops |
| Temperature / Tint | Grey luminance change < 1e-6 stops, correct direction, monotonic, continuous at 0 |
| Split toning | Neutral settings are exact bypasses (20,000 random cases); grey luminance change 0.0000 stops; correct tint directions; the shadow tint does not reach +4.5 stops |
| Monochrome | Silvergrain exactly neutral with Exposure / Temperature / Tint and every print; split toning tints it by design |
| Extended range | NaN / Inf 0 with every control random, DI −0.6 … 2.6 input |
| All 18 looks × 6 print options | No NaN, no negatives from in-gamut colours, monotonic ramps, no hue-map folds at 100 / 200%, slider extremes stable (incl. 200% with every print control at its extreme), Stage View correct |
| Skin | Per look ≤ 5.2°; worst stock × print 6.7° |
| Older regressions | V1 controls, film adjustments, prints: bit-exact |
| Synthetic scenarios (portrait, daylight, foliage, interior, night, saturated, high contrast) | All 18 looks × 6 print options pass at 100%. At 200% (informational) some looks show steep hue changes in saturated neon sweeps, as approved Midnight / Halcyon already did in V8 |
| LUT tool | All 18 looks, 16 secondaries and combinations generate and validate |

Commands:
```
cd tests
gcc -O2 -o math_test_v9 math_test_v9.c -lm && ./math_test_v9        # ALL CHECKS PASSED
python3 cine16_regression.py                                          # ALL CINE16 REGRESSIONS BIT-IDENTICAL
python3 v8_v9_report.py                                               # difference report + mapping check
gcc -O2 -DDUMP_LOOKOFF_V8 -o o8 math_test_v9.c -lm && ./o8 > v8.txt
gcc -O2 -DDUMP_LOOKOFF_V9 -o o9 math_test_v9.c -lm && ./o9 > v9.txt && cmp v8.txt v9.txt
cd .. && gcc -O2 -o scenario_check tools/scenario_check.c -lm && ./scenario_check
```

### Still required in DaVinci Resolve (not performed)

- [ ] V9 compiles in Resolve Studio (GPU compiler); the UI shows the new section order, names
      and default values.
- [ ] Saved V8 grades reopen with the intended settings: dropdown indices are unchanged, but
      the panel order changed.
- [ ] Visual check that the anchored approved looks still feel right on real footage, above
      all Midnight, Halcyon, Verona and Pastelle, whose midtones moved most.
- [ ] Argent, Nocturne and the five new looks on real footage (portrait, daylight, foliage,
      interiors, night city, saturated colours, high contrast), with every print.
- [ ] Exposure, Temperature, Tint and Split Toning on real footage; compare Temperature / Tint
      against Resolve's own white balance for feel (not expected to match numerically).
- [ ] A/B: original Cine16 node vs FilmLab V9 (Primary Off / Secondary X / Print None); a
      difference matte should be black.
- [ ] Real-time playback performance. V9 evaluates each primary stock twice per pixel (once
      for the grey anchor), so check this on your system.
- [ ] Exported LUTs match the DCTL in Resolve.

Keep the approved V8 file as a backup until V9 has been checked in Resolve.
