# DCTL-Gradelux

**CineCore** is a DaVinci Resolve DCTL grading and film-look system, currently
in development.

It is built as a modular color pipeline rather than a LUT replacement: a set
of reusable, independently replaceable functions with a documented processing
order, designed so film looks and grading tools can be added without
restructuring the transform.

## Status

**Phases 1 and 2 of 6 — complete.**

| Group | Controls |
|---|---|
| Setup | Input Encoding, Bypass |
| Primary grade | Exposure, Temperature, Tint, Contrast, Pivot, Saturation, Black Point, White Point |
| Film response | Highlight Shoulder, Highlight Roll Off, Shadow Toe, Shadow Depth |
| Film density and color | Film Density, Density Strength, Subtractive Saturation, Richness, Color Separation |

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

`CineCoreLook.dctl` is a separate DCTL that applies a selectable `.cube` look
with a mix control. It is kept separate from `CineCore.dctl` on purpose: the
supplied LUTs are display-referred **Rec.709**, while CineCore works in DaVinci
Wide Gamut / DaVinci Intermediate, so the look node must sit after a colour
space transform:

```
[ CineCore.dctl ] -> [ CST: DaVinci WG/Intermediate -> Rec.709 ] -> [ CineCoreLook.dctl ]
```

Applying it directly to DaVinci Intermediate data gives a broken image, not a
look. A DCTL also fails to compile if a LUT it references is missing, so
keeping the stage separate means `CineCore.dctl` still builds without any LUTs
present.

The `.cube` files are not distributed with this project — see
[luts/README.md](luts/README.md).

## Notes

Color-science decisions, labelled approximations and verification results are
recorded in [DEVNOTES.md](DEVNOTES.md).
