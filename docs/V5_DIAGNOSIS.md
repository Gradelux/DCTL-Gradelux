# Why the V5 prototype looks were too subtle, and what changed

## Method

Numbers alone can't say whether a look reads on screen, so every look was measured as seen.

The test content:
- colour checker
- six skin tones
- a hue sweep at three exposures
- a grey ramp

Each image ran through the look, then through a display transform (DWG → Rec.709 with a
filmic curve), then to CIELAB. The metric is mean ΔE against the untouched image: about 2 is
just noticeable, and 10 or more is obvious.

Each suspected weakening stage was then switched off one at a time. The tool is
`tools/look_visibility.c`.

## Findings (first V5 prototype)

| Measurement | Verona | Aurelia | Verdant | Midnight |
|---|---|---|---|---|
| Whole look, colours (ΔE) | 4.8 | 11.6 | 17.4 | 8.3 |
| Whole look, skin | 4.2 | 2.3 | 8.5 | 5.8 |
| **Tone stage only, greys** | **1.9** | **1.9** | **1.7** | **2.5** |
| Look vs look | Verona–Midnight only **5.5** | | | |

Aurelia's and Verdant's large colour numbers were mostly *desaturation*: less colour, not more
character.

### What was weakening or cancelling the looks

1. **The tone curve was nearly invisible.** This was the main problem.
   - **Fixed pivot:** every curve pivoted exactly on 18% grey and had to keep grey's brightness.
     Mid slopes were only 1.08–1.20, so one stop from grey the change was about 0.2 stop.
   - **Shallow shadows:** shadows deepened by at most (γ−1)×W, about 0.4 stop, and only well
     below grey.
   - **Shoulder too high:** it started 4–6 stops above grey. That region is already compressed
     by Resolve's output transform, so the film shoulder was hidden.
   - **Result:** ΔE under 2.5 on greys, which is why the looks didn't read as film.
2. **Colour zones barely engaged.**
   - **Zone width:** shadow, midtone and highlight colour reached full strength 4 stops from
     grey. At ±1 stop, where most of a normally exposed image sits, they ran at about 16%.
   - **Tiny shifts:** the shifts were also tiny (0.01–0.06 stop, 1–4%).
   - **Narrowing the zones alone** raised colour change by 30–40%.
3. **Stages cancelled each other.** The v5 spectral-overlap stage desaturates and blends, and
   the dye masking then spends its effect restoring purity. On Verona, removing the overlap
   made the look *stronger* (ΔE 4.8 → 6.4).
4. **Skin protection was modest but real.** It reduced skin change by up to 2 ΔE, which is
   why Verona's skin didn't turn warm.
5. **Not causes** (measured, no meaningful effect): the soft gamut limit, the chroma fade on
   hue rotation, Creative Intensity scaling at 100%, and print interaction. A stock under
   Print 2383 changes the image as much as the stock alone, so prints were not erasing the
   looks.

## What changed in the diagnostic build

No new mathematics; the fixes target the causes above.

- **Print-down density:** each stock can sit denser at grey (Verona −0.12, Verdant −0.06,
  Midnight −0.28 stop). This is film's "printing density", and it is part of the tone stage.
- **Tone acts inside the visible range:**
  - mid slopes of 1.31–1.44
  - shadow contrast acting over 3 stops
  - deeper toes
  - shoulders starting about +1.5 stops over grey
- **Per-stock colour-zone width:** 2–2.5 stops instead of 4. The film adjustment controls keep
  the original 4 stops, so their behaviour is unchanged.
- **Real colour shifts:** shadow, midtone and highlight colour of 0.05–0.22 stop.
- **Stronger hue relationships:** 8–14° between neighbouring hue zones.
- **No spectral overlap** on these three stocks.
- **Per-stock skin share of zone colour:** Verona gives skin its warmth (25% removed);
  Verdant protects skin from its cool-green cast (80% removed).

## Result (diagnostic build)

| Measurement | Verona 250D | Verdant 250D | Midnight 800T |
|---|---|---|---|
| Whole look, colours (ΔE) | **12.8** (was 4.8) | 9.2 | **10.8** (was 8.3) |
| Whole look, skin | **9.5** (was 4.2) | 5.8 | 9.3 |
| **Tone stage only, greys** | **4.6** (was 1.9) | **3.5** (was 1.7) | **6.1** (was 2.5) |
| Mid contrast (slope) | 1.37 | 1.31 | 1.44 |
| Shadows 4 stops under grey | −1.41 stop | −1.11 | −1.91 |
| Skin hue shift | −2.6° | +0.5° | −2.3° |
| White red/blue lean | +0.23 stop (warm) | −0.04 (neutral-cool) | +0.31 (tungsten) |

Distinctness between stocks:

| Pair | Before | Now |
|---|---|---|
| Verona vs Verdant | — | ΔE 11.4 |
| Verona vs Midnight | 5.5 | ΔE 9.3 |
| Verdant vs Midnight | — | ΔE 8.7 |

The closest pair now differs by 0.53 stop RMS on the test chart, against 0.28 before.

Every stock + print pair keeps skin within 6.1° of hue (mean 2.9°).
