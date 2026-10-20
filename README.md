# Gradelux FilmLab

Film emulation and grading engine for DaVinci Resolve Studio, written as a DCTL.

| File | Contents |
|---|---|
| `Gradelux_FilmLab_v2.dctl` | **Version 2 (current):** film emulation engine with 11 Film Stock options and 5 film controls, plus all six Version 1 controls |
| `Gradelux_FilmLab.dctl` | Version 1: the six core controls only |

Grain, halation, bloom and print-specific emulation are planned for later versions.

## Version 2: film emulation

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
