# Gradelux FilmLab

A film-style colour grading DCTL for DaVinci Resolve. Version 1 is the foundation:
accurate colour management and six core film controls. Film stocks, grain,
halation and bloom will come later.

File: `Gradelux_FilmLab.dctl`

## Input colour spaces

| Option | Signal | Decoded to |
|---|---|---|
| DaVinci Wide Gamut / Intermediate | Resolve's scene-referred log | scene-linear |
| Rec.709 / Gamma 2.4 (Display) | display-referred video (BT.1886) | display-linear (not treated as scene-linear) |
| Rec.709 (Scene) / BT.709 OETF | camera-encoded Rec.709 | scene-linear |

All modes are processed in **linear DaVinci Wide Gamut** and converted back to the
input space, so the node's output format always matches its input. 18% grey sits
at linear 0.18 in every mode, and every control pivots there, so a given slider
value looks similar whichever mode you use.

## Controls

| Control | Range | Neutral | What it does |
|---|---|---|---|
| Film Density | -1 to 1 | 0 | Darkens saturated colours (up to 1 stop) without changing hue or saturation. Neutrals untouched. Negative = thinner, brighter colour. |
| Subtractive Saturation | -1 to 1 | 0 | Adds saturation the way dye does: weaker channels get denser, the brightest channel is kept. Saturated colours get richer and darker. |
| Film Contrast | 0.5 to 2 | 1 | Log contrast around 18% grey. Black stays at black; the deepest shadows are protected from noise. |
| Highlight Roll-Off | 0 to 1 | 0 | Smooth shoulder starting 1 stop above grey. Compresses highlights without clipping. In Rec.709 Display mode this also lowers peak white. |
| Shadow Depth | -1 to 1 | 0 | Positive deepens shadows, negative lifts them. Grey and black stay fixed. |
| Global Intensity | 0 to 1 | 1 | Scales every control together. 0 = bypass. |

With all controls neutral (or Intensity at 0) the node passes the image through bit-for-bit.

## Install

1. Open Resolve's LUT folder: **Project Settings → Color Management → Lookup Tables → Open LUT Folder**.
   - macOS: `/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT/`
   - Windows: `C:\ProgramData\Blackmagic Design\DaVinci Resolve\Support\LUT\`
   - Linux: `/opt/resolve/LUT/` (or `/home/<user>/.local/share/DaVinciResolve/LUT/`)
2. Optionally make a `Gradelux` subfolder, then copy `Gradelux_FilmLab.dctl` into it.
3. In the same settings page click **Update Lists**, or restart Resolve.
4. On the Color page, add a node, open **Effects → ResolveFX Color → DCTL**, drag it onto
   the node, and pick `Gradelux_FilmLab` from the DCTL List.

DCTL in the free version of Resolve is limited; use Resolve Studio.

## Choosing the input mode

Pick the mode that matches the signal **entering this node**:

- **Resolve Color Managed (RCM)** – match your timeline colour space
  (e.g. DaVinci WG/Intermediate, Rec.709 Gamma 2.4, or Rec.709 (Scene)).
- **Manual / YRGB workflow** – put a Color Space Transform before FilmLab and choose the mode
  matching that CST's output gamut and gamma. Put your output transform (DRT) after it.
- **ACES** – not supported in v1. Convert to DWG/Intermediate with a CST first, then back.

## Testing in Resolve

1. **Identity:** apply the DCTL with defaults and toggle the node (Ctrl/Cmd+D). Nothing should
   change. Check scopes too.
2. **Mode match:** in each mode, set Film Contrast to 1.5. Grey (and pure black) should not
   move; a grey card or the middle of a greyscale ramp should hold its level.
3. **Neutrals:** on a greyscale ramp (Generators → Grey Scale), raise Subtractive Saturation and
   Film Density. The RGB Parade should stay perfectly aligned.
4. **Highlights (DWG):** on footage with bright highlights, raise Highlight Roll-Off and watch the
   waveform: highlights compress smoothly with no flat clip line.
5. **Signed values:** on a DWG source with saturated lights, check that out-of-gamut colours
   don't turn into hard edges or black pixels.
6. **Intensity:** dial a look, then move Global Intensity from 1 to 0; the look should fade
   smoothly to the original.

## Developer test

`tests/math_test.c` compiles the DCTL as C to check identity, round-trip accuracy, grey
preservation, monotonic tone curves and NaN safety:

```
cd tests && gcc -O2 -o math_test math_test.c -lm && ./math_test
```
