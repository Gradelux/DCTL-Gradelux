# Gradelux FilmLab

Film look and print emulation engine for DaVinci Resolve Studio, written as a DCTL.

| File | Contents |
|---|---|
| `Gradelux_FilmLab_v4.dctl` | **Version 4 (current):** 23 refined Gradelux looks, 5 film print emulations with print controls, the film adjustment controls and all Version 1 grading controls |
| `Gradelux_FilmLab_v3.dctl` | Version 3: 23 looks, no print stage |
| `Gradelux_FilmLab_v2.dctl` | Version 2: 10 film-stock profiles |
| `Gradelux_FilmLab.dctl` | Version 1: the six core controls only |
| `tools/gradelux_lut.c` | Reference `.cube` generator and LUT accuracy validator (look only, print only, look + print) |
| `tools/look_preview.c`, `tools/look_preview.py` | Renders a test scene through any look / print combination into a contact sheet |
| `docs/LUT_PLAN.md` | Plan for the standalone LUTs (DWG and Rec.709) |
| `docs/FilmLab_v4_looks.png`, `docs/FilmLab_v4_prints.png` | Contact sheets of the v4 looks and of prints and look + print combinations |
| `docs/FilmLab_v3_look_preview.png` | The v3 looks, for comparison |

Grain, halation, bloom, diffusion and film damage are intentionally not included. They will be
built separately from the LUT-compatible colour engine.

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
