# Gradelux FilmLab

Film stock and print emulation engine for DaVinci Resolve Studio, written as a DCTL.

| File | Contents |
|---|---|
| `Gradelux_FilmLab_v9_1.dctl` | **Version 9.1 (current):** Version 9 with a simplified interface (3 dropdowns, 12 sliders); bit-identical to Version 9 at the same settings |
| `Gradelux_FilmLab_v9.dctl` | Version 9: 18 primary looks with middle-grey anchoring, 16 Cine16 secondary profiles, 5 prints, Exposure / Temperature / Tint, Split Toning |
| `Gradelux_FilmLab_v8.dctl` | Version 8: Version 7 plus the 16 Cine16 profiles as an independent Secondary Film Look engine |
| `Gradelux_FilmLab_v7.dctl` | Version 7: 13 film stocks (the 8 approved V6.1 stocks plus 5 new), the 5 prints, all controls, developer Stage View |
| `Gradelux_FilmLab_v6_1.dctl` | Version 6.1: Version 6 with Moss 250D and Chroma 100 refined |
| `Gradelux_FilmLab_v6.dctl` | Version 6: production foundation, 8 film stocks, the 5 prints, all controls, developer Stage View |
| `Gradelux_FilmLab_v5_diagnostic.dctl` | Version 5 diagnostic build: the 3 stocks v6 is built on |
| `Gradelux_FilmLab_v5.dctl` | Version 5 first prototype: 4 stocks (superseded by the diagnostic build) |
| `Gradelux_FilmLab_v4.dctl` | Version 4: the previous 23 looks and the prints under their old names |
| `Gradelux_FilmLab_v3.dctl`, `_v2`, `Gradelux_FilmLab.dctl` | Earlier versions |
| `tools/gradelux_lut.c` | Reference `.cube` generator and LUT accuracy validator (v5 by default; `-DFILMLAB_V4` for v4) |
| `tools/look_preview.c`, `tools/look_preview.py` | Contact-sheet renderer for any stock / print combination |
| `docs/LUT_PLAN.md` | Plan for the standalone LUTs (DWG and Rec.709) |
| `docs/V5_DIAGNOSIS.md`, `docs/FilmLab_v5_diagnostic.png` | Why the first v5 looks were weak, the fixes, and a preview |
| `reference/Cine16_1B_Color_Profile.dctl` | The original Cine16 DCTL, unchanged: source of the secondary engine and reference for its regression test |
| `tools/extract_cine16.py` | Generates the Cine16 secondary engine from the original file (verbatim code, prefixed names) |
| `tests/runner.c`, `tests/cine16_regression.py` | Bit-for-bit comparison of v8 against the original Cine16 DCTL and FilmLab v7, including chained engines |
| `docs/V8_CINE16_INTEGRATION.md` | Cine16 integration: code changes, behaviour notes, regression instructions |
| `docs/V9_1_CHANGES.md` | Version 9.1: removed controls, how their defaults are preserved, regression checklist |
| `tests/v9_v91_equivalence.c` | Bit-for-bit comparison of Version 9 and 9.1 |
| `docs/V9_CHANGES.md` | Version 9 changelog, exposure-anchoring analysis, new controls, Cine16 notes, regression tests and Resolve checklist |
| `tools/filmlab_shim.h` | Shared host shim (DCTL built-ins, dropdown IDs, every control at its default) for tools and tests |
| `tests/math_test_v9.c`, `tests/v8_v9_report.py` | Version 9 test suite; documented V8 -> V9 differences and dropdown mapping check |
| `tools/scenario_check.c` | Synthetic scenario checks (portrait, daylight, foliage, interior, night, saturated, high contrast) for every stock × print |
| `docs/V7_TESTING.md` | V7 regression checklist: what was executed and what needs Resolve |
| `tools/look_visibility.c` | Measures visible strength of each stock and a distinctness matrix between all stocks |
| `docs/FilmLab_v6_looks.png`, `docs/FilmLab_v6_prints.png` | The 8 v6 stocks, and stock + print pairs |
| `docs/FilmLab_v5_prototype.png` | The first 4 prototype stocks |
| `docs/FilmLab_v4_*.png`, `docs/FilmLab_v3_look_preview.png` | Earlier versions, for comparison |

Grain, halation, bloom, diffusion and film damage are intentionally not included.

## Version 9.1: simplified interface

`Gradelux_FilmLab_v9_1.dctl` keeps the three dropdowns (Film Look, Secondary Film Look, Film
Print) and twelve sliders:
- **Corrections:** Exposure, Temperature, Tint.
- **Strengths:** Film Strength (formerly Creative Intensity), Secondary Strength, Print
  Strength.
- **Grading:** Film Contrast, Film Density, Subtractive Saturation, Highlight Roll-Off, Shadow
  Depth.
- **Master:** Global Intensity.

The other 24 controls were removed:
- **Kept as fixed defaults:** the six Cine16 detail sliders (1.00) and four print-shaping
  sliders (0) are fixed internally at their V9 defaults, so their effect is unchanged.
- **Removed outright:** film adjustments, split toning and the developer controls. At V9
  defaults these were exact bypasses.

**Result:** V9.1 matches V9 bit for bit on 358,754 test outputs, covering every look,
secondary, print and combination, plus 200,000 random slider settings. The Cine16 profiles
still match the original Cine16 DCTL bit for bit. Details and the Resolve checklist:
[`docs/V9_1_CHANGES.md`](docs/V9_1_CHANGES.md).

## Version 9: exposure anchoring, controls and new looks

`Gradelux_FilmLab_v9.dctl` builds on Version 8. Full details:
[`docs/V9_CHANGES.md`](docs/V9_CHANGES.md).

- **Middle-grey anchoring.** Every primary look maps 18% grey to 18% grey at neutral
  settings.
  - Cause: a stock's former print-down density shifted the whole image (Nocturne by
    −0.71 stops, Midnight −0.40, Pastelle +0.20).
  - Fix: it is now a black density that acts only in the shadows, plus an exact neutral
    grey anchor.
  - Kept: contrast, toe, shoulder and colour.
  - Changed for the 11 unchanged approved looks: midtones shift by at most 0.40 stops, with
    hue change under 0.5°.
- **Argent:** redesigned as the most contrast-heavy look, with deep blacks, bright defined
  highlights and restrained metallic colour.
- **Nocturne:** redesigned with no global darkening, an open toe, cool desaturated ambience
  and warm practicals.
- **New looks:** Patina, Opaline, Glacier, Terracotta, Velour (dropdown positions 14–18),
  each at least ΔE 7.3 from every other look.
- **Names without stock numbers** (Verona, …, Night, Daylight, Portrait, Chrome, Emerald,
  Vivid). Dropdown positions are unchanged.
- **Exposure** (±5 stops, scene-linear) and **Temperature / Tint** (chromatic adaptation)
  run before the film engines. **Split Toning** (shadow / midtone / highlight hue and
  saturation, Balance, Strength) runs after grading, before Global Intensity. All are exact
  bypasses at their defaults.
- **Unchanged:** Cine16 secondary profiles (still bit-identical to the original Cine16 DCTL),
  prints, and existing controls. With Film Look Off, V9 matches V8 bit for bit.

Tests executed on the host:
- `tests/math_test_v9.c`
- `tests/cine16_regression.py`
- `tests/v8_v9_report.py`
- `tools/scenario_check.c`
- older regressions

All pass. Not yet done: compiling in Resolve and checking the looks on real footage (checklist
in the changelog). Keep the V8 file as a backup until V9 has been checked in Resolve.

Previews: `docs/FilmLab_v9_looks.png`, `docs/FilmLab_v9_new_and_refined.png`,
`docs/FilmLab_v9_controls.png`.

## Version 8: Cine16 Secondary Film Look

`Gradelux_FilmLab_v8.dctl` adds the sixteen Gradelux Cine16 film colour profiles as an independent
**Secondary Film Look** engine, using the original Cine16 colour science unchanged.

**Processing order:**
1. Input (DWG / DaVinci Intermediate)
2. Primary Film Look
3. **Secondary Film Look**
4. Film Print
5. Film adjustments and grading
6. Output (DWG / DaVinci Intermediate)
7. Global Intensity

**Secondary Film Look dropdown:** Off, Neutral, Warm, Cool, Pastel, Rich, Golden, Faded, Reversal,
Night 500T, Daylight 250D, Portrait 400, Chrome 64, Emerald 400, Steel Chrome, Vivid 50, Cinema E.

**Controls:** the original Cine16 sliders with their original ranges and defaults:
- Secondary Strength
- Secondary Color Separation
- Secondary Saturation
- Secondary Subtractive Density
- Secondary Print Bias
- Secondary Profile Tone
- Secondary Skin Protection

The original Show Skin Mask is in the developer section.

### Verification

Executed on the host, bit for bit; details in
[`docs/V8_CINE16_INTEGRATION.md`](docs/V8_CINE16_INTEGRATION.md).
- **Cine16 match:** each profile in FilmLab (Primary Off, Print None) is **identical to the
  original Cine16 DCTL** over 786,272 samples:
  - every profile, at defaults, random and extreme slider settings, and with Show Skin Mask
  - inputs: RGB grid, grey ramp, skin, saturated and extended-range values, random pixels
- **Secondary Off:** identical to Version 7, including with every FilmLab control randomised.
- **Chaining:** Primary + Secondary + Print is identical to running v7's look, the original
  Cine16 and v7's print as separate programs.
- **Older regressions:** all still bit-exact.
- **Safety suite (`tests/math_test_v8.c`):** passes for all 14 × 17 × 6
  primary × secondary × print combinations.

Not yet done: compiling in Resolve, and an A/B difference matte against the original Cine16 node.

**Notes:**
- **Neutral is not Off:** Neutral is a gentle look of its own. Use Off for no secondary processing.
- **Range limit:** Cine16's original output clamp ([−0.5, 2.0] DI) is kept. It never engaged on
  normal-range input in testing.

## Version 7: thirteen film stocks

`Gradelux_FilmLab_v7.dctl` adds five stocks after Silvergrain 100, so existing dropdown
positions don't move:
- **09** Tungsten 500T
- **10** Halcyon 400
- **11** Argent 200
- **12** Nocturne 800
- **13** Pastelle 400

The eight approved stocks are untouched. So are the prints, every control and default, the
developer Stage View, and every shared function. Only new stock definitions were added.

### Changelog

- Five new stock definitions and five appended dropdown entries. No other code change.
- `tests/math_test_v7.c`:
  - V6.1 vs V7 bit-for-bit comparison of the eight approved stocks.
  - Monochrome stocks detected by definition, not dropdown position.
- `tools/scenario_check.c`: synthetic scenario checks.
- Tools (`look_visibility`, `look_preview`, `gradelux_lut`) list 13 stocks and default to V7.
- Previews: `docs/FilmLab_v7_looks.png`, `docs/FilmLab_v7_new_with_prints.png`.

### The five new stocks

Measured at 100%, no print. Columns:
- **Mid contrast:** slope at grey.
- **Blacks:** shift 4 stops under grey.
- **Highlights:** shift 4 stops over grey.
- **Colour:** display colourfulness vs the source.

| Stock | Mid contrast | Blacks | Highlights | Colour | Identity |
|---|---|---|---|---|---|
| **Tungsten 500T** | 1.31 | −1.10 | −0.48 (long shoulder) | 1.08× | Balanced tungsten negative. Neutral midtones, cool-neutral ambient shadows, warm amber highlights and practicals. Strong dye density: deep reds and rich deep blues; yellows lean amber, reds slightly orange. Natural greens and skin. Lighter and more natural than Midnight. |
| **Halcyon 400** | 1.48 | −1.77 | −0.37 (clean, bright) | 0.84× | Cool modern stock with a steel-blue identity: blue shadows, cool mids, neutral highlights. Blues and cyans carry the colour; greens muted and cool; yellows and oranges restrained; crisp contrast. Skin keeps a natural hue (+1°) but reads slightly paler. |
| **Argent 200** | 1.53 | −1.98 (wide toe keeps detail) | −1.39 (silvery) | 0.40× | Bleach-bypass-inspired: strong contrast and midtone separation, dense open blacks, restrained silvery highlights. Colour heavily reduced but present; warm hues reduced more than cool, so it reads metallic and slightly cool. |
| **Nocturne 800** | 1.50 | −2.44 (deepest) | −1.26 | 0.87× | Dark, dense night film: printed down further than Midnight. Shadows go to near-colourless blue-black; mids lean warm; amber and red practicals stay rich (amber chroma 60 vs 61 Off, red 78 vs 70) while blue and magenta neon are held down (33 vs 38, 66 vs 82). Midnight stays the blue/cyan night look. |
| **Pastelle 400** | 1.28 | −0.47 (soft but not milky) | −0.35 (long, creamy) | 0.87× | Airy, 400H-inspired: printed slightly up. The pastel comes from hue and density, not low saturation: mint greens lightened by density, pale skies, pink/peach skin (up to −7° toward pink on the lightest skin), cool shadows, cream highlights. Global saturation near normal. |

**Distinctness:** every new stock is at least ΔE 8.0 from every other stock on its own:
- Tungsten: Verdant 8.0, Chroma 8.1
- Halcyon: Verdant 8.7
- Argent: 14.2
- Nocturne: Midnight 9.3
- Pastelle: Aurelia 8.2

Visible change vs Off ranges from ΔE 10.2 (Tungsten) to 23.8 (Argent). Each new stock differs in
contrast shape, print-down density, zone width, shadow/mid/highlight colour, per-hue
rotation/saturation/density and dye coupling, not just in saturation and contrast.

### Verification

All of the following were run on the host through the C shim, not in Resolve.
- **Eight approved stocks:** bit-identical to V6.1 over 257,464 outputs (fixed grid ×
  6 print options × 4 Stage Views, plus 100,000 random settings of every control).
- **Older regressions:** V1, film adjustment, print and V5-diagnostic regressions are
  bit-exact.
- **Full test (`./math_test_v7`):** passes for all 13 stocks × 6 print options.
- **Synthetic scenario checks:** pass at 100%.

Compiling in Resolve and judging the stocks on real footage still need to be done; see
[`docs/V7_TESTING.md`](docs/V7_TESTING.md) for the checklist.

## Version 6.1: Moss 250D and Chroma 100 refinement

`Gradelux_FilmLab_v6_1.dctl` changes **only the stock definitions of Moss 250D and Chroma 100**.
- No shared function, control, default, dropdown entry or print changed.
- The other six stocks produce bit-identical output to Version 6 (verification below).

### Changelog

**Moss 250D** (was too flat and washed out)

Contrast and tone:
- Midtone contrast 1.21 → 1.29.
- Deeper toe: blacks at −4 stops −0.86 → −1.36; print-down density −0.04 → −0.10.
- Clearer shadow separation.
- Longer, more open shoulder for a smooth highlight roll-off.

Colour:
- Colourfulness 0.57× → 0.81× of the source (display-referred).
- Skin 0.55× → 0.82×, natural hue.
- Built from dye density, not saturation: colour density 0.25 → 0.60, stronger per-hue
  density (greens −0.35, cyans −0.28). Colours are darker and denser at restrained chroma,
  giving deep forest greens and dense cyans.
- The dye "impurity" that softened colour was removed.
- Global saturation 0.78 → 1.04, with per-hue saturation lowered on greens, cyans, blues and
  magentas so skin gains the most.
- Cool blue-cyan shadows against neutral, faintly warm highlights.

Kept apart from Verdant: Verdant stays lighter, more saturated and emerald/teal; Moss is
denser and quieter, with a different tone shape (ΔE 7.4 between them).

**Chroma 100** (was far too saturated)

Colourfulness changes (display-referred, relative to the source):

| | V6 | V6.1 |
|---|---|---|
| Overall | 1.56× | 1.27× (Verona 1.20×, Aurelia 0.75×) |
| Reds | 1.91× | 1.49× |
| Oranges | 1.70× | 1.44× |
| Blues | 1.31× | 1.27× |
| Foliage | 1.64× | 1.36× |
| Skin | 1.30× | 1.09× |
| Peak display chroma (C*) | 117 | 108 |
| Samples above C* 80 | 24 | 5 |

How:
- Per-layer response (kappa) 0.45 → 0.22. This was the main driver of runaway reds.
- Red-layer masking lightened.
- Red, yellow, blue and magenta zone saturation 1.25/1.08/1.30/1.02 → 0.85/0.90/0.96/0.96.
- Subtractive saturation 0.30 → 0.18.
- Colour density 0.35 → 0.45, and red / green / blue per-hue density deepened, so richness
  comes from density.

Kept: global saturation (1.32) and the slide-like tone (slope 1.48, hard toe, early shoulder,
shadows that hold colour). Greys stay neutral.

### Verification of the six approved stocks

`tests/math_test_v6_1.c` with `-DDUMP_SIX_V6` (compiles Version 6) and `-DDUMP_SIX_V61`
(compiles Version 6.1) drives both files with identical inputs and prints every output
bit-for-bit (`%a`):
- **Grid:** a fixed 9×9×9 DaVinci Intermediate grid (including out-of-range values) for Off,
  Verona, Verdant, Midnight, Aurelia, Solara and Silvergrain × all 6 print options × all
  4 Stage Views.
- **Random settings:** 100,000 random settings of every control.

Result: all 222,472 outputs identical (`cmp` and SHA-1 match). As a check that the comparison
detects changes, the same harness with Moss and Chroma in the set reports 55,612 differing
outputs.

The full test (`./math_test_v6_1`) passes, and the V1, film adjustment, print and Version 5
diagnostic regressions remain bit-exact. Skin across all stock × print pairs: worst 6.1°
(was 6.6°).

Preview: `docs/FilmLab_v6_1_before_after.png`, `docs/FilmLab_v6_1_looks.png`.

## Version 6: production foundation (8 film stocks)

`Gradelux_FilmLab_v6.dctl` is built on the Version 5 diagnostic build.

- **Original three stocks:** Verona 250D, Verdant 250D and Midnight 800T are unchanged
  (verified bit-for-bit against the diagnostic build over 30,000 random settings, including
  prints, controls and stage views).
- **New stocks:** Aurelia 400, Solara 200, Moss 250D, Chroma 100 and Silvergrain 100.
- **Unchanged:** prints, print controls, film adjustments, grading controls, DWG/Intermediate
  in and out, and exact bypass. Regressions against v1, v3 and v4 are bit-exact.

**Film Look dropdown:** Off, Verona 250D, Verdant 250D, Midnight 800T, Aurelia 400,
Solara 200, Moss 250D, Chroma 100, Silvergrain 100.

**Developer section:** "Dev - Stage View" is the last control (Normal, Film Color Only, Film
Tone Only, Full Film Look). At Normal, its default, it has no effect, and LUTs sample Normal.
For monochrome stocks, Color Only shows the spectral mix and Tone Only shows the curve on the
colour image.

### How the eight stocks differ

Measured at 100%, no print. Columns:
- **Mid contrast:** slope at grey.
- **Blacks:** shift 4 stops under grey.
- **Highlights:** shift 4 stops over grey.
- **Whites:** red–blue lean, in stops.
- **Skin:** hue shift / saturation.

| Stock | Mid contrast | Blacks | Highlights | Whites | Skin | Identity |
|---|---|---|---|---|---|---|
| **Verona 250D** | 1.37 | −1.41 | −0.82 | +0.23 warm | −2.6° / 1.22× | Rich Kodak-inspired daylight negative: strong photographic contrast printed slightly down, dense shadows, creamy shoulder, warm golden mids and skin, dense reds, signature yellow-green (greens to yellow-olive, yellows to gold). |
| **Verdant 250D** | 1.31 | −1.11 | −0.73 | −0.04 neutral-cool | +0.5° / 0.91× | Fuji-inspired cinema: strong natural contrast, cyan-green shadows, cool mids, rich emerald/teal greens, controlled crimson reds, deep cyans and blues. |
| **Midnight 800T** | 1.44 | −1.91 | −1.10 | +0.31 tungsten | −2.3° / 1.08× | Dramatic night film: deepest blacks, printed down, blue-cyan shadows against warm tungsten light, neon kept in check, teal greens, sodium-orange yellows. |
| **Aurelia 400** | 1.17 | −0.39 | **−1.34** | +0.12 cream | +4.6° / 0.91× | Portra-inspired portrait film: gentle, finished contrast, the earliest and creamiest shoulder (highlights turn to cream), warm pastel mids that skin receives, strong highlight desaturation, coral reds, muted olive greens, soft teal blues. |
| **Solara 200** | 1.15 | +0.10 (lifted) | −0.96 | **+0.36 golden** | +3.7° / 1.21× | Gold-inspired consumer film: the warmest stock, golden from shadows to highlights (warm shadows, unlike Verona), a faint warm lift in the blacks, softer contrast, bright rich yellows, orange-leaning reds, yellow-green foliage, muted blues. |
| **Moss 250D** | 1.21 | −0.86 | −0.53 (longest shoulder) | −0.04 neutral-cool | −0.4° / **0.66×** | Eterna-inspired, restrained and elegant: the least saturated colour stock (softened by dye impurity, not masking), cool cyan-green relationships kept subtle, smooth contrast with a deep toe, natural skin. |
| **Chroma 100** | **1.48** | −1.63 | −1.22 | neutral | −3.1° / **1.29×** | Ektar / slide-inspired: a different tonal family (steepest mids, hard toe, early shoulder), shadows that *gain* colour (reversal-film behaviour), strong masking and density, pure deep reds and blues, punchy greens, cyans toward blue, neutral greys. |
| **Silvergrain 100** | 1.11 | +0.16 (lifted, rich) | −1.23 | neutral (exact) | — | Soft vintage black-and-white, not a desaturation. Blue-leaning early-panchromatic sensitivity: blues render up to +0.95 stop over luminance and skies lighter, while reds, deep skin and foliage render 0.2–0.4 stop deeper. Delicate mids, smooth early shoulder, lifted but rich blacks. |

Visible difference between stocks (mean CIELAB ΔE through a display transform; about 2 is
barely visible, 10+ obvious): every new stock is at least **ΔE 9.1** from every other stock.
The closest pair overall is Verdant vs Midnight (8.7), two of the original three. Pairs stay
distinct over a print (8.8–51). Run `tools/look_visibility.c` for the full matrix.

### What keeps the looks differentiated

- **Different families, not one curve with small tweaks.**
  - The colour negatives differ in contrast (1.15–1.44), print-down density, toe depth,
    shoulder position (2.8–4.2 stops), zone width and skin share.
  - Chroma 100 uses reversal-film behaviour: shadows gain chroma (newly allowed by the
    engine), with the steepest curve and hard toe.
  - Silvergrain uses the monochrome engine with its own spectral mix and filter response.
- **Each stock owns its colour axes.**
  - Greens: yellow-olive (Verona, Solara), emerald/teal (Verdant), muted olive (Aurelia),
    subtle cyan-green (Moss), punchy (Chroma).
  - Shadows: neutral-warm (Verona), cyan-green (Verdant, Moss), blue-cyan (Midnight),
    golden (Solara), clean (Chroma).
  - Highlights: cream (Aurelia), golden (Solara), tungsten (Midnight), clean (Chroma).
- **Separation was measured and enforced.** Close pairs were pushed apart along their intended
  identities, not by adding saturation:
  - Verona vs Solara: ΔE 6.8 → 9.6. Solara became softer, golden and lifted; Verona stays
    dense and contrasty.
  - Aurelia vs Moss: ΔE 6.6 → 9.1. Aurelia keeps mid colour and goes warm and creamy; Moss
    goes cooler and quieter.
- **Prints don't erase identity.** Stocks remain ΔE 8.8–51 apart over Cinechrome 2383, and
  prints act only on colour, never on greys.
- **Engine extensions (no effect on existing stocks):**
  - shadow saturation may be negative (reversal film)
  - monochrome stocks honour print-down density
  - the developer view handles monochrome stocks

### Tests

`tests/math_test_v6.c` runs on all 8 stocks × 6 print options. It checks:
- identity and bypass
- prints neutral on greys and B&W
- Silvergrain exactly neutral with every print
- monotonic, continuous ramps; no NaN/Inf; no negatives from in-range colours
- the hue map; slider extremes
- skin per stock (≤ 4.6°) and across all stock × print pairs (worst 6.6°, mean 3.1°)
- developer view behaviour

Regression dumps:
- original three stocks = v5 diagnostic build
- Version 1 controls = v1
- film adjustments = v3
- prints = v4

## Version 5 diagnostic build (reference)

`Gradelux_FilmLab_v5_diagnostic.dctl` is a temporary look-development build.

- **Film Look:** Off, Verona 250D, Verdant 250D, Midnight 800T, redesigned to be strong and
  clearly distinct.
- **Diagnostic View (new dropdown):**

  | Option | What you see |
  |---|---|
  | Normal | Full product behaviour. This is what LUTs sample. |
  | Film Color Only | The stock's colour stages only. |
  | Film Tone Only | The stock's characteristic curve and print density, applied without any hue change. |
  | Full Film Look | The stock alone, with print, film adjustments, grading controls and Global Intensity bypassed. |

- **Unchanged:** all controls and the five prints. Bypass with everything off is still exact.
- **Regressions:** v1 controls, v3 film adjustments and v4 prints match bit-for-bit.

The diagnosis of why the first prototype was too subtle, and the before/after measurements,
are in `docs/V5_DIAGNOSIS.md`. A preview is in `docs/FilmLab_v5_diagnostic.png`.

| Stock | Character |
|---|---|
| Verona 250D | Rich daylight colour negative (Kodak-inspired): strong photographic contrast printed slightly down, dense shadows, creamy shoulder, warm golden mids and skin, dense reds, and a signature yellow-green rendering (greens toward yellow-olive, yellows toward gold). |
| Verdant 250D | Cool cinematic colour negative (Fuji-inspired): strong but natural contrast, cyan-green shadow density, slightly cool mids, emerald/teal greens kept rich, controlled crimson-leaning reds, lemon yellows, deep cyans and blues. |
| Midnight 800T | Dramatic night film (tungsten-film-inspired): deep blacks with a long toe, punchy contrast printed down, blue-cyan shadows against rich warm tungsten highlights, neon held in check, teal greens, sodium-orange yellows. |

These are original Gradelux creative interpretations, not measured reproductions of Kodak,
Fujifilm or any other manufacturer's films.

`tests/math_test_v5_diagnostic.c` runs the full suite on every stock × print combination,
plus checks of the Diagnostic View. `tools/look_visibility.c` reports the visible strength and
distinctness of the stocks.

## Version 5 prototype: Gradelux film stocks

All 23 looks are being renamed to original Gradelux film-stock names, and each is being
redesigned to have its own colour and tonal response. As requested, this prototype contains
only the first four stocks, for testing and approval. The other 19 come after sign-off.

**Film Look dropdown (prototype):** Off, Verona 250D, Aurelia 400, Verdant 250D, Midnight 800T.

**Planned full list:**
1. Verona 250D
2. Tungsten 500T
3. Halcyon 400
4. Aurelia 400
5. Argent 200
6. Solara 200
7. Nocturne 800
8. Pastelle 400
9. Patina 400
10. Chroma 100
11. Opaline 160
12. Glacier 400
13. Terracotta 200
14. Velour 400
15. Ashen 500
16. Lucent 50D
17. Nostalgia 200
18. Verdant 250D
19. Spectra 100
20. Midnight 800T
21. Panchro 400
22. Monolith 800
23. Silvergrain 100

**Film Print dropdown:** None, Cinechrome 2383, Velachrome 2393, Lumachrome 3513, Retrochrome
3510, Silkchrome 2383. These are the v4 prints renamed. Their colour science, tone and controls
are unchanged (verified bit-for-bit against v4 across all print control settings).

All stocks and prints are original Gradelux creative interpretations inspired by the character
of well-known films. They are not measured reproductions of any Kodak, Fujifilm or other
manufacturer's product.

### New in the colour science: spectral sensitivity overlap

A film's three layers don't see cleanly separated red, green and blue. Their spectral
sensitivities overlap, and how much is a large part of a stock's colour signature. v5 models
this per stock, as the first step of the film-stock stage:

```
c' = c + Σ s_cj (j − c)        (linear light, s_cj ≥ 0)
```

- **Neutrals:** greys are untouched exactly.
- **Range:** in-range colours stay in range and never go negative.
- **Hue order:** never changes.
- **Skin:** keeps 60% of its original rendering through a wide, soft hue window.

Overlap blends colours in a stock-specific, hue-specific way. The stock's dye masking (v4)
then restores purity through density, which is the same balance real film strikes. Together
with each stock's own curve, channel response, tints, per-hue response and dye coupling,
this gives every stock a distinct colour and tonal signature.

### The four prototype stocks

Measured at Creative Intensity 100%, no print:
- **Slope:** mid contrast.
- **Black:** shift 4 stops under grey.
- **Whites:** red–blue lean of highlights, in stops.

| Stock | Inspired by | Slope | Black | Whites | Character |
|---|---|---|---|---|---|
| Verona 250D | Clean cinema negative (Vision3-style daylight) | 1.17 | −0.60 | +0.11 | Long latitude and a late, gentle shoulder; firm, natural mids; dense detailed blacks. Moderate layer overlap balanced by strong dye masking: clean, faithful colour that still reads as film. Near-neutral greys with a whisper of cool shadow and warm light. |
| Aurelia 400 | Portrait colour negative (Portra-style) | 1.08 | −0.19 | +0.16 | Broad, overlapping sensitivities and light masking: soft, blended colour. Low per-layer contrast for smooth skin; early creamy shoulder for pastel, forgiving highlights. Warm-golden mids, coral reds, muted olive greens, quiet teal-leaning blues, skin hue consolidation. Gentle but finished contrast. |
| Verdant 250D | Restrained cinema negative (Eterna-style) | 1.12 | −0.48 | +0.03 | Strong green/blue overlap and almost no masking: a quiet, low-saturation palette (skin saturation ×0.75). Cyan-green shadow density, crimson-leaning reds, green-leaning yellows, teal greens. Soft contrast with real depth. |
| Midnight 800T | Tungsten night film (high-speed tungsten / neon style) | 1.20 | −0.72 | +0.22 | Tight layer separation and strong masking keep neon and practicals vivid. Cool blue shadows against warm light; the red layer shoulders later than blue, so bright sources roll off warm (tonal only, no halation). Teal greens, sodium-orange yellows, violet-leaning magentas, punchy mids, deep blacks. |

Midnight 800T keeps 18% grey neutral, like every stock, so it fits a normal colour-managed
workflow. Its tungsten character is in how it renders colour and light, not a global blue
cast. For the daylight-balanced-tungsten-film look, cool the image with Film Highlight
Warmth or Film Shadow Tone (or the white balance before FilmLab).

**Skin hue shift:** at most 3.2° for any stock alone, 6.0° worst across all 20 stock × print
pairs (mean 3.0°).

**LUT accuracy at 65³ (DWG, real colours):** mean error 0.0002–0.0003 code values for the
stocks alone and 0.0005 for Midnight 800T + Velachrome 2393 (the strongest pair). The LUT tool
now targets v5; file names use the new stock and print names.

### Prototype review checklist

For each of the 4 stocks, alone and with each of the 5 prints:
- [ ] The stock's identity is recognisable on real footage (daylight, interior tungsten, night / neon, portraits).
- [ ] Skin is natural on light, medium and deep skin, especially Aurelia 400 on portraits.
- [ ] Verdant 250D reads quiet and cinematic, not washed out.
- [ ] Midnight 800T: practicals and neon roll off warm and vivid without clipping; shadows are cool but not crushed.
- [ ] Whites read as white (Midnight and Aurelia lean warm by design, gently).
- [ ] Creative Intensity 0–200% and every print control behave smoothly.
- [ ] The dropdowns show Off + 4 stocks and None + 5 prints with the new names.

Then confirm, or send changes, before the remaining 19 stocks are built.

### Tests (v5)

`tests/math_test_v5.c` runs the full v4 suite on every stock × print combination. It covers:
- identity and bypasses
- neutral prints
- monotonic, continuous ramps
- no NaN/Inf; no negatives from in-range colours
- the hue map, including the new overlap stage
- slider extremes
- skin hue, per stock and across combinations
- distinctness

Regression dumps confirm:
- the Version 1 controls match v1 bit-for-bit
- the film adjustments match v3 bit-for-bit
- the renamed prints match v4 bit-for-bit

**Known limit:** at the most extreme combined settings (a stock at 200% under Velachrome 2393
with every print slider at +100), one channel can drift by about 0.0001 code value across a
highlight stretch. That's invisible, and defaults are strictly monotonic.

## Version 4: film looks and print emulation

### Colour space

- **Input and output:** DaVinci Wide Gamut / DaVinci Intermediate only.
- There is no input selector and no Rec.709 conversion.
- Internally the image is decoded to linear light and processed in Rec.2020 primaries
  (an exact rotation, undone before re-encoding).
- Negative and extended-range values pass through with no 0–1 clamp.

**Important when judging the looks:** FilmLab outputs scene-referred DWG/Intermediate. Always
view it through an output transform: a CST to Rec.709 / Gamma 2.4 with DaVinci tone mapping,
a DRT, or RCM's output space. Viewed directly, any DWG/Intermediate image, graded or not,
looks like flat log footage.

### Controls (Resolve shows them in this order)

| Control | Range | Default | What it does |
|---|---|---|---|
| Film Look | dropdown (24) | Off | The 20 colour looks and 3 monochrome looks (the "negative" stage). Off bypasses it. |
| Creative Intensity | 0–200% | 100% | Scales the look. 0% bypasses it exactly, 100% is the reference look, 200% is exaggerated but bounded. |
| Film Print | dropdown (6) | None | None, Print 2383, Print 2393, Print 3513, Print 3510, Print 2383 Soft. Applied on top of the look. None bypasses it. |
| Print Strength | 0–100% | 100% | Scales the print. 0% bypasses it exactly. |
| Print Contrast | -100 to 100 | 0 | Multiplies the print's mid contrast by 0.78×–1.27× around 18% grey. The shoulder and toe keep their smooth shape. |
| Print Density | -100 to 100 | 0 | Scales the print's colour density (saturation-driven and per-hue density) from 0× to 2×, and its dye coupling from 0.5× to 1.5×. This is density, not saturation. |
| Print Saturation | -100 to 100 | 0 | Scales the print's overall chroma (0.71×–1.41×) while keeping its per-hue character, density and dye behaviour. |
| Print Highlight Rolloff | -100 to 100 | 0 | Moves the print's shoulder. + rolls highlights off earlier and softer, − later and longer. |
| Film Color Density | -100 to 100 | 0 | Adds to or removes the look's own colour density response. |
| Film Color Separation | 0–100 | 50 | Hue clustering (above 50) or blending (below 50). Never folds hues. |
| Film Highlight Warmth | -100 to 100 | 0 | Highlight colour temperature, brightness-neutral. |
| Film Shadow Tone | -100 to 100 | 0 | Shadow colour (+ warm, − teal), brightness-neutral. On monochrome this acts as split toning. |
| Film Density … Global Intensity | | | The six Version 1 controls, unchanged (bit-exact with v1). |

The print controls do nothing while Film Print is None.

### Processing order

```
DaVinci Intermediate -> linear
  -> FILM LOOK  (scaled by Creative Intensity)
  -> FILM PRINT (scaled by Print Strength, shaped by the four print controls)
  -> film adjustments (Color Density, Separation, Highlight Warmth, Shadow Tone)
  -> Version 1 controls (Film Density, Subtractive Saturation, Film Contrast, Shadow Depth, Highlight Roll-Off)
  -> DaVinci Intermediate -> Global Intensity mix
```

- The print builds on whatever the look produced, so any of the 24 looks (including Off)
  combines with any of the 6 print options.
- Look and print are separate functions (`glx_look_apply`, `glx_print_apply`). Each is a
  deterministic RGB → RGB transform.
- Global Intensity mixes after re-encoding, so the fade is perceptually even in DaVinci
  Intermediate.

### What changed in the colour science (v3 → v4)

v3 already worked on linear light, but its looks behaved like well-made filters: the tone
curve flattened on both sides of grey, soft looks got softness from lifted blacks, and
colour came mostly from saturation scaling. v4 changes the underlying models:

1. **New sensitometric characteristic curve.**
   - **Shadows:** below grey, contrast gives real black density (shadows sit up to
     (γ−1)·W stops deeper), then the slope returns to 1. Detail survives instead of being
     crushed or lifted.
   - **Highlights:** above grey, a sigmoid shoulder `tanh(γe/U)·U` rolls highlights smoothly
     toward U stops over grey, like a film shoulder.
   - **Join and shape:** the two halves meet with matching slope and curvature at grey. The
     curve never reverses and black stays black.
   - **Soft looks** now get their softness from an earlier shoulder rather than lifted blacks.
     Every look has mid contrast of at least 1.09; v3's softest were 0.81–0.95.
2. **Dye coupling (new).** Colour is mixed in a log-density domain using differences between
   channels, so greys are mathematically unchanged.
   - **Negative couplings** model masking and inter-image effects: purer, denser colour.
   - **Positive couplings** model unwanted dye absorption: softer, vintage colour (Fable,
     Haze, Bloom, Print 3510).
   - **Anchor:** prevents the strongest channel from brightening, so added colour comes from
     density (darker weaker channels), as in subtractive film. It is applied as a gain:
     black stays black and in-range colours never go negative.
3. **Channel-dependent density curves.**
   - **Per-channel response:** each channel can have its own contrast and shoulder, and part of
     the curve ("kappa") is applied to each channel independently. This produces film's
     nonlinear saturation: richer colour through the mids, softer colour in the shoulder.
   - **Crossover order:** zone colour (shadow / midtone / highlight colour, crossover) is now
     applied before the curve, so the shoulder compresses colour along with tone.
4. **Neutral-locked prints.**
   - **Colour only:** a print's per-channel curves and dye coupling act on colour only. Greys,
     whites and black-and-white images stay exactly neutral (tested bit-exact), so prints never
     add a colour cast.
   - **Warmth without a cast:** "warm highlights" in a print means warm colours keep density
     in the highlights while blues compress earlier. Whites stay white.
5. **Natural skin.** Per-channel curves skew hues toward the strongest channel. On skin,
   their share is reduced by 75%, and hue rotation, tints and per-hue colour are also
   reduced. All protections use a wide, soft hue window, so gradients stay smooth and LUTs
   stay accurate.
6. **Cleaner whites.** Warm and cold looks (Ember, Dune, Sienna, Fable, Tundra) were eased so
   whites read as white, with a red–blue difference of 0.17 stop at most.

### The 23 looks (v4)

"Slope" is mid contrast (1.00 = unchanged). "Black" is how much deeper the image sits 4 stops
under grey (negative = denser blacks). Measured values, Creative Intensity 100%, no print.

| # | Look | Slope | Black | Character |
|---|---|---|---|---|
| 01 | Pure | 1.23 | −0.75 | Polished clean film: firm, well-separated mids, dense detailed blacks, long clean shoulder, rich natural colour from dye density, neutral whites, skin hue within 1°. |
| 02 | Ember | 1.23 | −0.85 | Cinematic warmth: amber light, warm mids, deep dense shadows, rich red density, olive-held greens. |
| 03 | Slate | 1.27 | −0.88 | Cool cinema: blue-slate shadows, cool mids, muted teal greens, firm contrast. |
| 04 | Solace | 1.10 | −0.19 | Gentle warmth with substance: creamy early shoulder, warm natural skin, real mid contrast. |
| 05 | Chrome | 1.31 | −0.95 | Crisp and metallic: strong per-channel contrast, cool neutrals, steel blues and cyans. |
| 06 | Dune | 1.16 | −0.54 | Sunlit earth: golden light, earthy shadows with depth, olive greens, terracotta reds. |
| 07 | Noir | 1.37 | −1.29 | Dramatic and restrained: very deep blacks with detail, heavy shadow desaturation, dense muted colour. |
| 08 | Bloom | 1.09 | −0.10 | Soft pastel with structure: delicate early shoulder, pink-tinged highlights, mint greens, light mauve veil. |
| 09 | Haze | 1.09 | +0.11 | Atmospheric: cool veil and milky shoulder, muted colour softened by dye impurity, tonal shape kept. |
| 10 | Vivid | 1.24 | −0.77 | Bold colour carried with density and dye masking: deep reds and blues, clear separation. |
| 11 | Muse | 1.13 | −0.34 | Editorial: balanced contrast with depth, creamy whites, refined consolidated skin. |
| 12 | Tundra | 1.15 | −0.34 | Cold: icy highlights and shadows, desaturated teal greens, crisp separation. |
| 13 | Sienna | 1.23 | −0.80 | Burnt warmth: orange-turned reds, earthy dense colour, deep warm-neutral blacks. |
| 14 | Velvet | 1.19 | −0.86 | Luxurious depth: very dense jewel tones, plum-tinged deep shadows, smooth shoulder. |
| 15 | Cinder | 1.17 | −0.15 | Moody smoke: smoky desaturated shadows with a faint veil, subdued colour, warm highlights. |
| 16 | Lumen | 1.10 | −0.33 | Luminous: radiant mids, lighter colour density so colours glow, clean blacks. |
| 17 | Fable | 1.13 | +0.11 | Vintage print: faint cyan-green veil, warm yellowed mids, nostalgic colour from dye impurity. |
| 18 | Moss | 1.17 | −0.63 | Organic forest: deep dense greens, muted olive yellows, earthy separation. |
| 19 | Prism | 1.19 | −0.62 | Spectrum: strongest hue separation, broad rich colour with dye masking. |
| 20 | Eclipse | 1.31 | −1.15 | Dark drama: very deep toe, powerful contrast, cool shadows against warm protected skin. |
| 21 | Mono | 1.23 | −0.75 | Classic 35mm panchromatic: slightly red-sensitive, rich contrast, dense detailed blacks. |
| 22 | Obsidian | 1.48 | −1.58 | Red-filter drama: dark skies and foliage, bright skin, very deep toe, brilliant whites. |
| 23 | Silver | 1.09 | −0.06 | Soft vintage silver-gelatin: blue-leaning sensitivity, luminous early shoulder, gentle veil. |

The mono looks output exactly R = G = B. Their channel mixes and filter responses are
unchanged from v3; their curves now use the v4 characteristic curve.

### The 5 film prints

Creatively designed, print-film-inspired interpretations. They are not measured reproductions
of real print stocks. Measured with no look (print alone); every print keeps whites and greys
exactly neutral.

| Print | Slope | Black | Highlights (+4 st) | Character |
|---|---|---|---|---|
| Print 2383 | 1.24 | −0.78 | −0.47 | Classic cinema print: pronounced contrast, strong rich blacks, dense highlights with a smooth roll-off. Blue layer shoulders early, so bright colours warm as they roll off. Dye masking gives dense, natural colour. |
| Print 2393 | 1.36 | −1.12 | −0.54 | Punchier: higher contrast, deeper blacks, strongest per-layer response (midtone separation), richer colour density. |
| Print 3513 | 1.10 | −0.35 | −0.61 | Gentle: softer contrast, long smooth shoulder, warm-leaning colour, natural skin, detailed blacks without wash. |
| Print 3510 | 1.08 | −0.36 | −0.78 | Vintage: slightly soft contrast, early gentle shoulder, golden yellows, olive greens, teal-leaning blues, subdued colour from dye impurity, real shadow depth. |
| Print 2383 Soft | 1.15 | −0.52 | −0.21 | 2383 colour response with a softer curve: lower contrast, later and gentler shoulder, lighter toe, controlled density. |

Prints stack with the look's contrast. For example, Pure (1.23) + Print 2383 (1.24) gives about
1.5 at grey: a full print-film contrast. For a gentler combination, use a soft look, a gentle
print (3513, 3510, 2383 Soft), a lower Print Strength, or Print Contrast.

### LUT configurations

The look and print stages are independently callable and RGB-only. Three reference
configurations can be sampled into LUTs:

| Configuration | Settings |
|---|---|
| A. Look only | Film Look = look, Film Print = None |
| B. Print only | Film Look = Off, Film Print = print |
| C. Look + Print | Film Look = look, Film Print = print |

In all three, Creative Intensity = 100%, Print Strength = 100%, the print controls are 0 and
every other control is at its default. See `docs/LUT_PLAN.md` for the generator, domains,
measured accuracy and validation.

### Install (Resolve Studio)

1. In Resolve, open **Project Settings → Color Management → Lookup Tables → Open LUT Folder**.
   - macOS: `/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT/`
   - Windows: `C:\ProgramData\Blackmagic Design\DaVinci Resolve\Support\LUT\`
2. Copy `Gradelux_FilmLab_v4.dctl` into a `Gradelux` folder there, then click **Update Lists**.
3. On a Color page node, add **ResolveFX Color → DCTL** and choose `Gradelux_FilmLab_v4`.
4. Make sure the node receives DWG/Intermediate:
   - **RCM:** set the timeline colour space to DaVinci WG/Intermediate.
   - **Manual:** place it after a CST to DWG/Intermediate and before the output CST / DRT.

The maths has been verified on a host compiler only. Compile and test it in Resolve Studio
on macOS (Metal) and Windows (CUDA/OpenCL) before release.

### Testing checklist (Version 4)

**Bypass and identity**
- [ ] Film Look Off + Film Print None, all defaults: toggling the node changes nothing (scopes).
- [ ] Creative Intensity 0% equals Look Off; Print Strength 0% equals Print None.
- [ ] Print controls do nothing while Film Print is None.
- [ ] Global Intensity 0% returns the input with any settings.
- [ ] Both dropdowns: every entry selects a different result (24 looks, 6 print options).

**Neutrality and monochrome**
- [ ] Grey Scale generator through each print alone: RGB parade channels identical.
- [ ] Mono / Obsidian / Silver with every print: parade identical; each print changes only tone.
- [ ] B&W footage (desaturated in a previous node) through any print: no colour appears.

**Footage and lighting scenarios** (try each with Pure, a warm look, a cool look, a soft look,
and each print)
- [ ] **Daylight exteriors:** blue skies stay smooth (no banding); foliage density looks natural.
- [ ] **Tungsten / practical-lit interiors:** warm sources roll off smoothly and hold colour; whites aren't contaminated.
- [ ] **Mixed lighting:** no hue jumps where warm and cool light meet.
- [ ] **Night and low light:** blacks are dense with detail; no noise boost or crushed floor; LEDs and neon don't clip or posterize.
- [ ] **Overexposed highlights (sky, windows, lamps):** the shoulder is smooth, with no flat clip line and no hue shift as highlights compress.
- [ ] **Underexposed footage (brought up first):** toe detail holds and noise stays neutral.
- [ ] **High-key / white backgrounds:** whites stay white with prints; looks with warm or cool identity tint them only gently.
- [ ] **Skin:** light, medium and deep skin in daylight, tungsten and shade. Natural with every look and print; no magenta, green or grey cast. Check the strongest pairs too (e.g. Velvet + 2393).
- [ ] **Saturated products and costumes:** colours dense, not neon; no channel flattening.
- [ ] **Gradients:** sky and backdrop gradients have no banding at Creative Intensity 100% and 200%.

**Controls**
- [ ] Each print control at −100 and +100 on every print: smooth and stable.
- [ ] Creative Intensity 0 → 200% and Print Strength 0 → 100% sweep smoothly.
- [ ] All existing v3 controls behave as before, with and without a look and print.

**Performance**
- [ ] UHD/4K playback with look + print active vs bypass, on macOS and Windows.
- [ ] Render a clip and confirm it matches the viewer.

**Preview without Resolve**
```
gcc -O2 -o look_preview tools/look_preview.c -lm
./look_preview looks.raw && python3 tools/look_preview.py looks.raw looks.png
./look_preview combos.raw 1:1 2:2 && python3 tools/look_preview.py combos.raw combos.png "Pure + 2383|Ember + 2393" 2
```

## Version 3 reference (looks without print)

### Colour space

- **Input and output:** DaVinci Wide Gamut / DaVinci Intermediate only.
- There is no input selector and no Rec.709 conversion.
- Internally the image is decoded to linear light and the maths runs in Rec.2020 primaries
  (exact rotation, undone before re-encoding).
- Negative and extended-range values pass through with no 0–1 clamp.

### Controls (Resolve shows them in this order)

| Control | Range | Default | What it does |
|---|---|---|---|
| Film Look | dropdown (24) | Off | Off, the 20 colour looks, and the 3 monochrome looks. Off bypasses the look engine completely. |
| Creative Intensity | 0–200% | 100% | Scales the selected look. 0% = bypass, 100% = the reference look (identical to its LUT), 200% = exaggerated but bounded. |
| Film Color Density | -100 to 100 | 0 | Adds to or removes the look's own colour density response. No effect with Off or a monochrome look. |
| Film Color Separation | 0–100 | 50 | Hue clustering. Above 50 colour families separate more distinctly; below 50 they blend. Never folds hues. |
| Film Highlight Warmth | -100 to 100 | 0 | Highlight colour temperature (+ amber, − cool), brightness-neutral. |
| Film Shadow Tone | -100 to 100 | 0 | Shadow colour (+ warm amber, − teal), brightness-neutral. On monochrome looks this acts as split toning. |
| Film Density … Global Intensity | | | The six Version 1 controls, unchanged (see below). |

Film Stock Strength from v2 is replaced by Creative Intensity.

### Processing order

```
DaVinci Intermediate -> linear -> LOOK (scaled by Creative Intensity) -> film adjustments
  -> V1 grading controls -> DaVinci Intermediate -> Global Intensity mix
```

The look engine depends only on the pixel's RGB, the Film Look and Creative Intensity. It
uses no image position, neighbouring pixels, time or randomness. With every other control
at default, the DCTL output is exactly the reference look. That is what the LUTs sample.

Global Intensity mixes after re-encoding. It must mix in DaVinci Intermediate to give an even
perceptual fade, and this is the only change from the suggested processing order.

### The 23 looks

All looks keep 18% grey at the same brightness (within ±0.02 stop), so switching looks
doesn't change exposure. They are original Gradelux designs inspired by film aesthetics, not
reproductions of any photographic product.

"Slope" is mid contrast: 1.00 is unchanged, higher is punchier.

**Colour looks**

| # | Look | Slope | Character |
|---|---|---|---|
| 01 | Pure | 1.17 | Polished clean cinema. Firm contrast, deep but detailed blacks, long clean shoulder, richer natural colour, neutral whites. Skin hue moves under 1°. |
| 02 | Ember | 1.19 | Cinematic warmth. Amber highlights, warm mids, deep shadows, rich dense reds; greens held back toward olive. |
| 03 | Slate | 1.27 | Cool cinema. Blue shadows, cool mids, muted teal greens, firm contrast, restrained saturation. |
| 04 | Solace | 0.90 | Gentle warmth. Soft contrast, creamy compressed highlights, warm natural skin, a whisper of warm fade. |
| 05 | Chrome | 1.29 | Crisp and metallic. High per-channel contrast, cool neutrals, steel blues and cyans, controlled warm colours. |
| 06 | Dune | 1.04 | Sunlit earth. Golden highlights, earthy warm shadows, olive greens, terracotta reds, dusty blues. |
| 07 | Noir | 1.34 | Dramatic and restrained. Deep blacks, strong contrast, heavy shadow desaturation, dense muted colour. |
| 08 | Bloom | 0.85 | Soft pastel. Lifted mauve blacks, pink-tinged highlights, mint greens, lavender blues, light colour density. |
| 09 | Haze | 0.81 | Atmospheric. The most faded blacks (cool), soft contrast, milky shoulder, muted colour throughout. |
| 10 | Vivid | 1.21 | Bold colour. Strong saturation carried with density, deep reds and blues, clear separation, vibrant contrast. |
| 11 | Muse | 1.07 | Editorial. Balanced contrast, creamy whites, refined consolidated skin, softened greens and blues. |
| 12 | Tundra | 1.10 | Cold. Icy blue highlights and shadows, desaturated teal greens, pale cool fade. |
| 13 | Sienna | 1.19 | Burnt warmth. Orange-turned reds, earthy dense colour, warm mids, rich contrast, olive greens. |
| 14 | Velvet | 1.15 | Luxurious depth. Very dense jewel tones (subtractive saturation), plum-tinged deep shadows, smooth long shoulder. |
| 15 | Cinder | 1.10 | Moody smoke. Grey, desaturated shadows with a soft fade, subdued colour, subtle warm highlights. |
| 16 | Lumen | 0.95 | Luminous and airy. Open shadows, late shoulder, lighter colour density so colours glow, radiant mids. |
| 17 | Fable | 1.02 | Vintage print. Faded cyan-green blacks, warm yellowed mids and highlights, nostalgic softened colour. |
| 18 | Moss | 1.11 | Organic forest. Deep dense greens, muted olive yellows, green-tinged shadows, earthy separation. |
| 19 | Prism | 1.14 | Spectrum. The strongest hue separation, broad rich saturation across every hue, clean contrast. |
| 20 | Eclipse | 1.29 | Dark drama. Deep toe, powerful contrast, strong cool shadows against warm protected skin, dense teal greens. |

**Monochrome looks.** Each has its own channel mix, colour-filter response and tone curve.
Output is exactly neutral (R = G = B).

| # | Look | Slope | Character |
|---|---|---|---|
| 21 | Mono | 1.17 | Classic 35mm panchromatic. Slightly red-sensitive (skin about 0.2 stop brighter than luminance), rich contrast, smooth shoulder, detailed blacks. |
| 22 | Obsidian | 1.43 | Red-filter drama. Skies and foliage go dark (blue −1.7 stops), skin and reds bright (+0.6 / +1.1), very deep toe, brilliant whites with a short shoulder. |
| 23 | Silver | 0.91 | Soft vintage silver-gelatin. Blue-leaning sensitivity (luminous skies, slightly deeper skin), low contrast, lifted toe, long luminous shoulder, gentle fade. |

**Monochrome and Creative Intensity:** output = mono rendering + (1 − intensity) × the
original colour detail.
- At 0% the image is untouched.
- At 50% it is half-desaturated over a half-strength mono rendering.
- From 100% it is fully monochrome. Above 100% the channel mix and curve are exaggerated,
  and the image stays neutral.

### How the looks are built

Every colour look is a set of about 60 numbers fed into one engine:

1. **Colour response.** Hue-dependent rotation, saturation and density across six smoothly
   blended hue zones. On top of that:
   - overall saturation, with separate saturation in shadows, midtones and highlights
   - saturation-driven density and hue separation
   - a soft gamut limit, so in-range colours never go negative or clip
   - skin protection (reduced rotation, saturation and density changes on skin)
2. **Subtractive saturation.** The Version 1 hue-preserving operator, driven by the look.
3. **Tone.** The look's contrast, toe and shoulder, partly applied per channel for film-like
   colour response, plus channel crossover and shadow / midtone / highlight colour.
   The colour shifts are brightness-neutral and reduced on skin.
4. **Fade.** A smooth, optionally tinted black lift for faded or milky blacks.

Creative Intensity scales all of these numbers together. It then keeps each one inside a
range where the maths is proven smooth and order-preserving. Tone curves stay monotonic,
and the hue map can't fold, even at 200%.

### Install (Resolve Studio)

1. In Resolve, open **Project Settings → Color Management → Lookup Tables → Open LUT Folder**.
   - macOS: `/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT/`
   - Windows: `C:\ProgramData\Blackmagic Design\DaVinci Resolve\Support\LUT\`
2. Create a `Gradelux` folder there and copy `Gradelux_FilmLab_v3.dctl` into it.
3. Click **Update Lists** (or restart Resolve).
4. On the Color page, add **ResolveFX Color → DCTL** to a node and choose `Gradelux_FilmLab_v3`.
5. Make sure the node receives DWG/Intermediate:
   - **RCM:** set the timeline colour space to DaVinci WG/Intermediate.
   - **Manual:** place the node after a CST to DWG/Intermediate and before your output CST/DRT.

The maths has been verified on a host compiler only. Compile and test it in Resolve Studio
on macOS (Metal) and Windows (CUDA/OpenCL) before release.

### Testing checklist (Version 3)

**Identity and bypass**
- [ ] Film Look Off with every control at default: toggling the node changes nothing (check scopes).
- [ ] Creative Intensity 0% with any look: identical to Off.
- [ ] Global Intensity 0%: identical to the input with any settings.
- [ ] The dropdown shows exactly 24 entries, and each selects a different look.

**Each look (use a colour chart, a grey ramp, skin tones and real footage)**
- [ ] Pure on/off shows a clear improvement in contrast, richness and separation without an obvious cast.
- [ ] 18% grey stays at the same level when switching looks (waveform).
- [ ] Grey ramp: smooth, no banding, kinks or reversals at the toe or shoulder.
- [ ] Skin: natural on light, medium and deep skin, with no green, grey or magenta cast.
- [ ] Highlights: smooth shoulder; bright saturated lights don't flatten or posterize.
- [ ] Shadows: lifts and fades are smooth, with no crushed floor or noise boost.
- [ ] Mono / Obsidian / Silver: RGB parade channels identical; each renders skies, skin and foliage differently.

**Creative Intensity**
- [ ] Sweep 0 → 200% on each look: smooth, no sudden jumps.
- [ ] At 200%, gradients and skin still hold together (strong, not broken).
- [ ] Monochrome looks: 0 → 100% fades colour out smoothly; fully neutral from 100%.

**Controls with every look**
- [ ] Film Color Density, Separation, Highlight Warmth and Shadow Tone at both extremes: smooth and stable.
- [ ] The Version 1 controls behave as in v1 and v2.
- [ ] Out-of-gamut sources (LEDs, neon): no black pixels or speckles.

**Performance**
- [ ] UHD/4K playback at timeline frame rate with a look active vs bypass, on both platforms.
- [ ] Render a clip and confirm it matches the viewer.

**Preview without Resolve**
```
gcc -O2 -o look_preview tools/look_preview.c -lm && ./look_preview looks.raw
python3 tools/look_preview.py looks.raw preview.png
```

### LUTs

See `docs/LUT_PLAN.md` for the plan to produce the 46 standalone LUTs. It covers the sampling
domain, transfer functions, matrices, clipping behaviour, measured accuracy, and validation.

## Version 2 reference (film stocks)

### Controls (Resolve shows them in this order)

| Control | Range | Default | What it does |
|---|---|---|---|
| Film Stock | dropdown | None / Clean | Selects the film profile (below). |
| Film Stock Strength | 0–100% | 100% | Moves every parameter of the selected stock from Clean (0%) to the full profile (100%). At 0% the stock is bypassed exactly. |
| Film Color Density | -100 to 100 | 0 | Scales the stock's colour density (its per-hue density plus saturation-driven darkening). -100 removes it, +100 doubles it. Has no effect with None / Clean. |
| Film Color Separation | 0–100 | 50 | 50 = the stock as designed. Higher values draw hues toward six hue axes (one sits on skin tone) so colour families read as more distinct, with a little more chroma. Lower values spread hues for a blended palette. The hue map is mathematically guaranteed never to fold, so there are no harsh transitions. |
| Film Highlight Warmth | -100 to 100 | 0 | Highlight colour temperature: + amber, - cool. Only acts above mid grey, fading in over 4 stops. Brightness-neutral. |
| Film Shadow Tone | -100 to 100 | 0 | Shadow colour bias: + warm amber, - teal. Only acts below mid grey, fading in over 4 stops. Brightness-neutral. |

The six Version 1 controls follow, with the same ranges, defaults and behaviour.

### Processing order

```
DaVinci Intermediate -> linear -> film colour -> film tone -> V1 colour -> V1 tone -> DaVinci Intermediate -> Global Intensity mix
```

- **Film colour:** per-stock hue rotation, saturation and density across six smoothly blended hue zones, plus Film Color Separation and Film Color Density.
- **Film tone:** per-stock characteristic curves and highlight/shadow colour, plus Highlight Warmth and Shadow Tone.

Both film passes run on linear light in the same internal space as Version 1, so there are
no extra colour conversions.

### Film profiles

These are original, film-inspired interpretations. They are not measured reproductions of
any manufacturer's stock. Every stock keeps 18% grey at exactly the same level, so you can
switch stocks without re-exposing.

| Stock | Character |
|---|---|
| None / Clean | No film response. The film controls still work. |
| Vision3 500T Inspired | Soft tungsten negative look: lower contrast (0.92), open lifted shadows, long gentle shoulder. Cool blue shadows against warm highlights, teal-leaning greens, slightly muted yellows. |
| Vision3 250D Inspired | Natural daylight negative: neutral contrast and clean greys. Subtle warmth in highlights and a hint of cyan in shadows. Balanced, faithful colour with slightly softened greens. |
| Vision3 50D Inspired | The cleanest stock: slightly firmer mids (1.05), nearly neutral tints, high colour fidelity, small extra saturation and light density. |
| Kodak 2383 Print Inspired | Print-film punch: high contrast (1.26), deep toe, firm shoulder and strong per-channel contrast for rich colour. Cyan-blue shadows, warm highlights, dense reds and blues, greens toward teal, highlights desaturate. |
| Portra 400 Inspired | Soft portrait negative: low contrast (0.86), lifted toe, very soft shoulder. Warm pastel highlights, muted yellow-leaning greens, gentle coral reds, kind to skin. |
| Kodak Gold 200 Inspired | Warm consumer negative: golden yellows, saturated reds, yellow-leaning greens, warm shadows and highlights, moderate punch (1.08). |
| Fujifilm Eterna Inspired | Restrained cinema palette: low contrast and low saturation, teal-green shadows, cool greens, crimson-leaning reds, extra colour density. |
| Fujifilm 400H Inspired | Airy pastel: lifted toe, soft contrast, mint/cyan greens, slightly cool highlights, light ("thin") colour density. |
| CineStill 800T Inspired | Night tungsten look: strong cool-shadow / warm-red-highlight split, teal greens, intense reds and cyans, punchy mids. |
| Modern Cinema Negative | Contemporary cinema look: punchy clean mids (1.12), late smooth shoulder, clear teal-shadow / warm-highlight split, controlled teal greens, dense clean primaries. |

### How a stock is built

Each stock is a set of about 40 numbers fed into two shared operators.

1. **Colour response.** The colour is split into a neutral part and a chroma vector. The
   chroma's hue picks a blend of six overlapping hue zones (raised-cosine weights that always
   sum to 1, so there are no seams). Each zone has its own hue rotation, saturation and
   density. On top of that there is overall saturation, saturation loss in highlights and
   shadows, and saturation-driven density. Saturation and rotation are limited by a soft
   gamut boundary, so in-range colours never go negative or clip. Hue rotation is reduced
   by 60% on skin tones.
2. **Tone response.** Each stock has a tone curve with mid contrast, toe and shoulder.
   Part of the curve (the stock's "kappa", 6–30%) is applied to each channel separately,
   the way film dye layers respond on their own. This gives film-like saturation through
   the mids and softening in the shoulder. Each channel also gets an exposure-driven
   deviation (crossover, shadow tint, highlight tint). The deviations are zero at mid grey
   and carry no brightness. Stock tints are reduced by 60% on skin.

### Film profile testing checklist

**Basics**
- [ ] All controls at default (Film Stock = None / Clean): toggling the node changes nothing.
- [ ] Each of the 11 dropdown options gives a different look on a colour chart.
- [ ] Film Stock Strength 0% with any stock looks exactly like None / Clean, with V1 controls still active.
- [ ] Global Intensity 0% returns the original image with any stock selected.

**Exposure and neutrals**
- [ ] Grey Scale generator: switching stocks never moves the middle patch (DaVinci Intermediate ~0.336).
- [ ] The grey ramp stays smooth and monotonic on the waveform for every stock, with no kinks at the toe or shoulder.
- [ ] Shadow and highlight tints on the RGB parade match each stock's description (e.g. CineStill: blue low end, red high end).

**Colour**
- [ ] Vectorscope with a colour chart: per-stock hue shifts are smooth. Sweep a hue gradient (e.g. a rainbow generator) and look for bands or jumps.
- [ ] Saturated sources (neon, LEDs, saturated flowers): no clipping, black pixels or channel flattening with any stock at 100% plus Film Color Separation at 100.
- [ ] Film Color Separation 0 → 100 on a hue gradient: colours group or spread smoothly with no hard edges.
- [ ] Film Color Density -100 / +100: colours lighten or deepen; greys unchanged.

**Skin tones**
- [ ] Test light, medium and deep skin under every stock: no green, grey or magenta cast. Portra, 250D and 50D should look the most natural.
- [ ] Highlight Warmth ±100 and Shadow Tone ±100 on a face: changes stay in their tonal zones and transition smoothly.

**Highlights and shadows**
- [ ] Bright highlights (sky, practicals) on 2383 and Modern: smooth shoulder, no hue shift as highlights roll off.
- [ ] Deep shadows on 500T, Portra, 400H (lifted) and 2383 (deep): no noise boost, no crushed floor.

**Performance and render**
- [ ] UHD/4K playback with a stock active vs bypass at timeline frame rate.
- [ ] Render a clip and confirm it matches the viewer.
- [ ] Repeat on macOS (Metal) and Windows (CUDA/OpenCL).

## Version 1 reference

## Colour space

- **Input:** DaVinci Wide Gamut / DaVinci Intermediate
- **Output:** DaVinci Wide Gamut / DaVinci Intermediate
- No input selector and no Rec.709 conversion.

Inside the node, the DaVinci Intermediate signal is decoded to linear light. The colour
and tone maths then run on linear values rotated into Rec.2020 primaries. DWG's blue
primary is imaginary, so its luminance formula has a negative blue weight; Rec.2020's
real primaries keep saturation and luminance maths stable. The rotation is exactly undone
before the signal is re-encoded to DaVinci Intermediate. Nothing is clipped or
gamut-compressed.

## Controls

| Control | Range | Default | Behaviour |
|---|---|---|---|
| Film Density | -100 to 100 | 0 | Darkens coloured pixels in proportion to their saturation (up to 0.75 stop for pure colours) with one gain on all channels, so hue and saturation are unchanged. Skin tones are protected (about 0.1–0.2 stop at 100). Negative values give thinner, brighter colour. |
| Subtractive Saturation | -100 to 100 | 0 | Adds saturation the way dye does: the weakest channel gets denser while the brightest is kept. Hue is held exactly, no channel can clip or flip sign, and colours that are already saturated change little. 75% of the luminance change is restored, leaving a slight film-like darkening. |
| Film Contrast | -100 to 100 | 0 | S-curve in stops around 18% grey. Slope at grey runs from 0.66 to 1.52 and eases back to 1 about 2.5 stops either side, so the extremes are never clipped. Black stays at black. |
| Shadow Depth | -100 to 100 | 0 | Smooth toe below about 2 stops under grey. Positive deepens shadows by up to 1 stop, negative lifts them. Grey is fixed and nothing is crushed to a hard floor. |
| Highlight Roll-Off | 0 to 100 | 0 | Smooth shoulder starting 1.5 stops above grey. Highlights compress toward a soft ceiling but keep their separation. One gain on all channels, so there are no colour shifts. |
| Global Intensity | 0 to 100% | 100% | Mixes the result with the original in DaVinci Intermediate. 0% returns the input bit-for-bit. |

All tone operations are a single gain per pixel, so they cannot shift hue. With every control
at its default the node returns its input bit-for-bit.

## Install (Resolve Studio)

1. In Resolve open **Project Settings → Color Management → Lookup Tables → Open LUT Folder**.
   - macOS: `/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT/`
   - Windows: `C:\ProgramData\Blackmagic Design\DaVinci Resolve\Support\LUT\`
2. Create a `Gradelux` folder there and copy `Gradelux_FilmLab.dctl` into it.
3. Click **Update Lists** (or restart Resolve).
4. On the Color page add **ResolveFX Color → DCTL** to a node and choose `Gradelux/Gradelux_FilmLab_v2`
   (or `Gradelux_FilmLab` for Version 1).

I have only checked the maths on a host compiler; it has not been compiled by Resolve yet.
The file uses only standard DCTL functions, but test it on both macOS (Metal) and Windows
(CUDA/OpenCL) before selling it.

## Node placement

**Manual (YRGB) workflow**
```
[CST: camera → DWG/Intermediate] → [primary balance] → [Gradelux FilmLab] → [creative nodes] → [CST / DRT: DWG → Rec.709 or other output]
```

**Resolve Color Management**
Set the timeline colour space to **DaVinci WG/Intermediate**. Put FilmLab on any node; the
output transform is applied after the node tree.

Do a basic balance (exposure, white balance) before FilmLab so 18% grey sits near
DaVinci Intermediate 0.336. All controls pivot on that level.

## Testing checklist

**Identity and blending**
- [ ] Defaults: toggle the node on/off; the image and scopes must not change.
- [ ] Global Intensity 0%: identical to bypass, whatever the other sliders are set to.
- [ ] Sweep Intensity 0 → 100%: the look fades in evenly.

**Clipping and range**
- [ ] On a DWG source with bright highlights, push Contrast and Roll-Off. The waveform should show no flat clip line inside the node.
- [ ] Check out-of-gamut sources (LED, neon, saturated blue light) for black pixels, hard edges or speckles.
- [ ] Generators → Grey Scale ramp: the waveform stays smooth with no banding or reversals.

**Hue shifts**
- [ ] With a colour chart, push Contrast, Shadow Depth and Roll-Off one at a time. Vectorscope traces should only move toward or away from the centre, never rotate.
- [ ] Push Subtractive Saturation to ±100: vectorscope traces stretch along their own angle.
- [ ] Grey ramp with all sliders at extremes: the RGB parade stays perfectly aligned.

**Skin tones**
- [ ] Film Density at 100 on a range of skin tones (pale to deep). Skin should barely darken while saturated clothing and backgrounds deepen.
- [ ] Subtractive Saturation at +50 on skin: richer but not orange or red.

**Highlights and shadows**
- [ ] Roll-Off at 100 on a sky or practical light: compression is smooth with no visible knee.
- [ ] Shadow Depth ±100 on a dark scene: no crushed black floor, no noise boost when lifting.

**Performance**
- [ ] Play back 4K or UHD at timeline frame rate with the node active and compare against bypass.
- [ ] Render a test clip and confirm the output matches the viewer.

## Maths tests

`tests/math_test_v4.c` (Version 4) checks:
- identity, and exact bypass of the look, the print and the print controls
- every one of the 144 Look × Print combinations:
  - monotonic, continuous ramps
  - prints keep neutral input exactly neutral
  - mono looks exactly neutral with every print
  - no NaN/Inf; no negatives from in-range colours
  - continuity around the hue circle
- the hue map at 100% and 200% with Separation extremes, and with print controls at their extremes
- slider extremes on every combination
- minimum contrast per look; prints never tint white
- skin hue per look and across all look + print pairs
- all looks and all prints distinct

Regression dumps confirm v4 matches v1 (Version 1 controls) and v3 (film adjustments) bit-for-bit.

`tests/math_test_v3.c` (Version 3) checks:
- identity at defaults, at 0% Global Intensity and at 0% Creative Intensity
- that the look fades continuously to zero
- for all 23 looks at 100% and 200%:
  - monotonic, continuous neutral ramps
  - no NaN/Inf
  - no negatives from in-range colours
  - exact R = G = B for monochrome looks
  - the hue map never folding, and the output staying continuous around the hue circle
- skin hue shifts, a minimum strength per look, and all looks distinct
- extreme slider sweeps with every look

It also builds regression dumps: v3 with Look Off equals v1 bit-for-bit, and v3's film
adjustments match v2. Build commands are at the top of the file.

`tests/math_test_v2.c` (Version 2) checks:
- identity at defaults, at 0% Global Intensity, and the exact bypass at 0% Stock Strength
- grey and black fixed for every stock
- monotonic and continuous ramps
- no NaN/Inf
- no negatives created from in-range colours
- every stock distinct
- skin hue shift under 4°
- hue order preserved under Film Color Separation

It also dumps a regression file showing that v2 with Film Stock = None matches Version 1
exactly. Build and run commands are at the top of the file.

`tests/math_test.c` (Version 1) compiles the DCTL as C and checks:
- identity at defaults and at 0% intensity
- the DaVinci Intermediate round trip
- grey, neutral and black preservation
- monotonic tone curves
- hue preservation
- no NaN/Inf on random signed inputs
- skin protection

```
cd tests && gcc -O2 -o math_test math_test.c -lm && ./math_test
```
