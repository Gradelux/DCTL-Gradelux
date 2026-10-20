# Gradelux FilmLab

Film emulation and grading engine for DaVinci Resolve Studio, written as a DCTL.
Version 1 contains the core engine and six controls. Film profiles, print emulation,
halation, bloom and grain will come in later versions.

File: `Gradelux_FilmLab.dctl`

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
4. On the Color page add **ResolveFX Color → DCTL** to a node and choose `Gradelux/Gradelux_FilmLab`.

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

`tests/math_test.c` compiles the DCTL as C and checks:
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
