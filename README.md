# DCTL-Gradelux

**CineCore** is a DaVinci Resolve DCTL grading and film-look system, currently
in development.

It is built as a modular color pipeline rather than a LUT replacement: a set
of reusable, independently replaceable functions with a documented processing
order, designed so film looks and grading tools can be added without
restructuring the transform.

## Status

**Phases 1, 2 and 3 of 6 — complete.**

| Group | Controls |
|---|---|
| Setup | Input Encoding, Bypass |
| Primary grade | Exposure, Temperature, Tint, Contrast, Pivot, Saturation, Black Point, White Point |
| Film response | Highlight Shoulder, Highlight Roll Off, Shadow Toe, Shadow Depth |
| Film density and color | Film Density, Density Strength, Subtractive Saturation, Richness, Color Separation |
| Hue density | Red, Orange, Yellow, Green, Cyan, Blue, Magenta Density |
| Split toning | Warm Highlights, Cool Shadows, Split Tone Balance |
| Bleach bypass | Bleach Bypass, Bleach Bypass Mix |
| Film look | Film Look (19 LUT looks + Reference Identity), Look Mix |

Film density, subtractive saturation, richness, color separation, hue
densities, split toning, bleach bypass and the film-look engine arrive in
phases 2–5.

## Working space

Designed for **DaVinci Wide Gamut / DaVinci Intermediate**.

Scene-referred operations (exposure, white balance, tint) run in linear DWG;
tone shaping runs in DI log. No output color-space transform is applied —
input and output are both in the timeline encoding.

Set `Input Encoding` to `DaVinci Linear` if the node is fed linear instead.

## Installation

Copy `CineCore.dctl` into the Resolve LUT directory, then apply it with the
DCTL OpenFX plugin on a node.

- **macOS** `/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT/`
- **Windows** `C:\ProgramData\Blackmagic Design\DaVinci Resolve\Support\LUT\`
- **Linux** `/opt/resolve/LUT/`

Every control is neutral at its default: with the panel untouched the DCTL
returns its input unchanged.

## LUT looks

The film look selector is built into `CineCore.dctl`: a **Film Look** combo box
and a **Look Mix** slider, applied as the last stage before output safety. It
defaults to `None`, which is a bit-exact pass-through.

> **`CineCore.dctl` will not compile unless every LUT it declares is present.**
> A `luts` folder must sit beside it containing all 19 `.cube` files, named
> exactly as listed in [luts/README.md](luts/README.md).

The LUTs must be baked for **DaVinci Intermediate** input — the DCTL performs
no colour space conversion. The supplied looks are Rec.709 and must be
converted once, offline: see **[BAKING.md](BAKING.md)**. Applying a Rec.709 LUT
to DI data gives a broken image, not a look.

`tools/LUTProbe.dctl` is a one-LUT diagnostic. If CineCore fails to build, the
probe tells you whether the cause is the LUT macros or a missing file.

The `.cube` files are not distributed with this project — see
[luts/README.md](luts/README.md).

## Notes

Color-science decisions, labelled approximations and verification results are
recorded in [DEVNOTES.md](DEVNOTES.md).
