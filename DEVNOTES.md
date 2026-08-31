# CineCore — Development Notes

Running record of color-science decisions, approximations and verification.
Phases 1, 2 and 3 are complete and internally consistent: no placeholder maths.

---

## 1. Color-science decisions

### 1.1 Luminance weighting — **implementation choice, not a derivation**

**Decision.** CineCore borrows the ITU-R BT.709 luma coefficients
(0.2126 / 0.7152 / 0.0722) as a positive weighting applied directly to the
working RGB values. They live in `SECTION 2.2` and nowhere else.

> **These are NOT DaVinci Wide Gamut luminance coefficients and must not be
> treated as a DWG specification.** They are an explicitly chosen stand-in,
> documented in one place and replaceable in one place. Borrowing the numbers
> says nothing about the working gamut and confers no Rec.709 property on it.

**Phase 2 added no further dependence on them.** Film density, subtractive
saturation, richness and color separation are built entirely from channel
maxima, minima and medians, and use no luminance weighting at all. After
Phase 2 the only consumers of these three defines remain the Phase 1 tone
norms, the white-balance normalization and the primary saturation.

**Why not DaVinci Wide Gamut's own coefficients.** Official DWG luminance
coefficients are not present in the documentation available to this project.
None are asserted, derived or fitted anywhere in the code.

**Why not simply compute them from the primaries.** Beyond the rule against
inventing values, there is a real technical reason not to want them here.
DWG's blue primary sits at a negative *y*, so the space's true colorimetric
luminance row carries a **negative blue coefficient**. Every luma-preserving
operation built on such a row *inverts* for strongly saturated blues — a
saturated blue would carry negative luminance, and saturation and tone-norm
maths would run backwards on it. That is an image-quality defect, not a
subtlety. A positive weighting summing to 1 is worth more here than
colorimetric exactness.

**This is not a Rec.709 transform.** No matrix, no primary conversion, no
gamma, no gamut change. Three numbers used as weights, which is exactly what
Resolve's own luminance mixer does by default regardless of working space.

**How much does it actually affect the image?**

| | Effect |
|---|---|
| Neutrals (R=G=B) | **None, ever.** Any weighting summing to 1 leaves neutrals identical. Exposure, contrast, pivot, black/white point and the whole tone curve are untouched by this choice. |
| Saturated color | How much a saturated color appears to darken under luma-preserving operations, and where the shoulder/toe knees fall for strongly colored pixels. Visible, but a **character** difference. |
| Failure mode avoided | A negative coefficient would invert luma-preserving maths on saturated blues. That one *is* a correctness issue. |

Verdict: **largely a design choice, not a material image-quality issue** — with
the single exception that the sign of the coefficients genuinely matters.

To change it, edit only the three defines in Section 2.2. Equal weights
(0.33333333 each) are gamut-agnostic and equally stable, at the cost of a
poorer perceptual match.

### 1.2 DaVinci Intermediate transfer function

**Confirmed by the project owner against Blackmagic Design's official DaVinci
Wide Gamut / DaVinci Intermediate documentation. Locked — do not modify.**

They are also self-checking against the encoding's documented landmarks, which
were verified numerically before use:

| linear | code value |
|---|---|
| 0.00 | 0.000000 |
| 0.18 | 0.336043 |
| 1.00 | 0.513837 |
| 10.0 | 0.756599 |
| 100.0 | 1.000000 |

The two segments meet continuously at the cut point and the pair round-trips
to float precision.

An important consequence that drove the tuning: **code value 1.0 is linear
100, not white.** Diffuse white sits at 0.5138. Highlight shoulder knees
therefore belong around 0.42–0.78, not near 1.0, and an output limiter with a
knee at 1.0 would be operating well inside the legal signal range.

### 1.3 Where each operation runs

- **Linear DWG:** exposure, white balance, tint. Scaling light is only
  physically meaningful in linear; a gain on log code values bends the
  transfer curve instead of changing exposure.
- **DaVinci Intermediate log:** contrast, pivot, shoulder, toe, depth, black
  and white point, saturation. Log is near perceptually uniform, so a slope
  through a pivot is the classic film gamma and the knees land where the eye
  expects them.
- At most **one** linear↔log round trip per pixel, and it is skipped entirely
  when exposure and white balance are neutral, so an untouched panel is a
  bit-exact pass-through rather than a round-trip approximation.
- **No output color-space transform.** Input and output are both in the
  timeline encoding.

### 1.4 Shadow toe — design change made during Phase 1

The first implementation mirrored the highlight shoulder's exponential
soft-clip downwards. Numerical testing showed this was **wrong in direction**:
a mirrored soft-clip compresses shadows *upwards* towards the knee, so DI
0.0497 came out at 0.0842 and black was lifted off zero into a milky print
toe — the opposite of a control meant to deepen shadows.

Replaced with a cubic determined by four boundary conditions:

```
f(0) = 0     black stays exactly black
f'(0) = s0   shadow slope at black, driven by the strength control
f(1) = 1     the knee is pinned, mids untouched
f'(1) = 1    slope matches at the knee, no visible seam
  =>  f(t) = (s0-1)t^3 + 2(1-s0)t^2 + s0*t
```

A gamma cannot satisfy `f'(1) = 1` without collapsing to the identity. Below
zero the cubic turns non-monotonic, so negatives take the linear extension
`y = s0*x`, which is C1-continuous with the cubic at zero and keeps
scene-referred negatives negative. Reducing the slope at black rather than
clamping is what separates *compression* from *crushing*: near-black detail is
squeezed but every distinct input still maps to a distinct output.

### 1.5 Shadow depth — bump weight, not a shadow ramp

The weight is `16u²(1-u)²`, zero **and flat** at both ends. Zero at the pivot
keeps mids clean; zero at black stops the control pushing legal blacks
negative. A weight peaking at black would not be depth, it would be clipping.
`CC_DEPTH_AMOUNT` is capped at 0.045 so the steepest slope the control can
subtract is 0.92 at the smallest legal pivot — provably below 1, so the curve
cannot fold back on itself.

### 1.6 Output stage is a validity net, not a limiter

Knees sit at code value 2.0 and −1.0, far outside anything a real image can
reach (1.0 is already linear 100). Ordinary imagery, out-of-gamut negatives
included, passes through untouched; only genuinely runaway arithmetic is
bounded, and smoothly. Deliberately **not** a clamp to [0,1]: that would
destroy highlight roll-off and remove the negatives later stages need to
recover color. Values are sanitised once on input and once on output only —
never between stages, since every function is total.

---

## 1.7 Phase 2 — the four film-character operators

The design problem in Phase 2 is that four controls all change how colorful an
image looks, and they have to feel like four different tools rather than four
copies of one. They are separated by **what each one anchors on**, which is
what determines the character of the result:

| Operator | Anchor | What moves | Net effect on brightness |
|---|---|---|---|
| Saturation (Phase 1) | luminance | all channels, symmetric | preserved exactly |
| Film Density | none — uniform offset | all channels equally | darkens saturated color |
| Subtractive Saturation | max channel | only channels below the max | darkens |
| Richness | median channel | top up, bottom down, middle fixed | roughly neutral |
| Color Separation | min and max pinned | only the median channel | none by construction |

Three of the five are structurally incapable of raising a channel, so they
cannot introduce clipping at all. Only richness can raise the top channel, and
it is bounded by a midtone window, a gain ceiling and a chroma brake.

### 1.7.1 Film Density

Photographic density is defined as the negative logarithm of transmittance, so
adding density is *literally* a subtraction in a log domain — which is where
this stage already runs. The amount subtracted is scaled by a selectivity curve
on chroma and applied **equally to all three channels**. A uniform log offset
is an exact exposure pull on that pixel, so channel ratios survive untouched:
hue and chroma are provably unchanged (verified bit-exact), the color only
gains weight.

Rejected: a saturation-style operator. It scales chroma, which is exactly what
this control is specified *not* to do, and scaling chroma is what clips.

Two controls because amount and selectivity are different needs. **Density
Strength** interpolates the chroma exponent from a cube law (only the purest
colors gain density) to near-linear (mid-chroma colors join in).

**Limitation:** darkens only, so it cannot recover a color that is already too
dark. Skin protection is statistical, not semantic — see 1.7.5.

### 1.7.2 Subtractive Saturation

Additive saturation expands about luminance, pushing the brightest channel
*up*, which is what makes digital saturation clip and glow. Subtractive color
works the other way: adding dye removes light from the channels that are not
the color. So this anchors on the **maximum** channel and widens each channel's
gap below it. The brightest channel has zero gap and never moves — which is why
this cannot introduce highlight clipping however hard it is pushed — while the
others fall, so the color saturates and darkens in one move.

**A bug caught in testing.** The first version widened the gap and then passed
it through the highlight shoulder's soft ceiling. For gaps above ~0.8 the
ceiling returned a value *below the original gap*, so the control quietly
desaturated the most saturated colors in the frame — 9,904 of 20,000 random
pixels. Reworked to bound the **increase** rather than the widened value:

```
increase' = headroom * (1 - exp(-increase / headroom))     headroom = limit - gap
```

The increase is non-negative by construction, so the gap can never shrink,
while the result still approaches the ceiling asymptotically. Now verified:
never raises the max channel, never reduces chroma, monotonic in gap.

**Limitation:** the image darkens as it saturates. That is inherent to
subtractive behavior, not a defect. Near the ceiling, large gaps receive
proportionally less increase than small ones, so extremely saturated colors
take a small hue shift — the price of the guard.

### 1.7.3 Richness

Anchored on the **median** channel, which makes it the only symmetric operator
of the three: top channel up, bottom channel down, middle fixed (verified
exactly). Widening the spread between channels *is* channel separation, which
is what reads as depth, and because the move is symmetric overall brightness
barely shifts — so it does not behave like the saturation control next to it.

Weighted by a smooth bump on the median channel, zero and flat at both ends, so
shadows and highlights are excluded and the midtones carry the effect. Using
the median as the tone estimate is deliberate: already computed, exact for
neutrals, and **needs no luminance coefficients**, so this stage adds no
dependence on the Section 2.2 approximation.

**Limitation:** the one Phase 2 stage that can raise a channel. Bounded, and
highlights are excluded by the weighting, but not structurally impossible.

### 1.7.4 Color Separation

Separation is a change in *how fast hue varies*, not a rotation, so this is a
contrast curve on hue position rather than an offset.

Sorting the channels into min / median / max identifies the 60° hue sector; the
median's position inside it,

```
p = (median - min) / (max - min)          in 0..1
```

is where the hue sits within that sector. Applying a smoothstep to `p` is a
contrast curve on hue: **p = 0 and p = 1 are exact fixed points, so the six pure
hue axes cannot move at all**, while hues mid-sector are pushed apart at up to
1.5× the original rate.

Only the median channel moves. Min and max are preserved *exactly* (verified),
so chroma is unchanged to the last bit and this stage cannot alter saturation,
brightness or density. It is a pure hue operation — which is what "keep
luminance-dependent operations isolated from hue-dependent operations" asks
for, taken literally.

Rejected: an `atan2` hue angle followed by a rotation. It is ill-conditioned
near neutral, exactly where stability matters most, and a rotation moves colors
bodily around the wheel — the aggressive hue shifting this control is meant to
avoid.

Reconstruction uses a monotone piecewise-linear remap pinned at min, median and
max rather than an offset applied to "the median channel", because that handles
ties between equal channels without double-counting them.

**Limitations:** hue position is defined in log RGB, not a perceptual hue space,
so equal changes in `p` are not perceptually equal around the circle — an
approximation, labelled. And since the average slope of any fixed-endpoint
mapping is 1, extra separation mid-sector is necessarily paid for with mild
compression near the sector ends. Deliberate: it is what gives cleaner
primaries. The blend is capped at `CC_SEP_MAX = 0.6` so the slope stays ≥ 0.4
everywhere — hues near a primary compress towards it but never collapse onto it.

### 1.7.5 How skin is protected, and what that does not cover

There is **no hue detection anywhere in Phase 2**, by design. Skin protection is
a consequence of the chroma selectivity curve: chroma is measured as the log
spread between the largest and smallest channel — exactly zero for a neutral,
no square root, no arc tangent, nothing to become ill-conditioned near grey —
and the effect scales as that spread raised to a power above 1. Skin's spread is
roughly a quarter of a saturated primary's, so squaring leaves it at a few
percent of the effect. Measured, with density, richness and separation all at
maximum:

| patch | max channel shift | hue position |
|---|---|---|
| shadow skin | 0.107 stop | 0.472 → 0.472 |
| mid skin | 0.174 stop | 0.465 → 0.464 |
| highlight skin | 0.153 stop | 0.465 → 0.465 |
| **saturated red** | **2.212 stop** | — |

A 12.7× selectivity ratio between saturated red and mid skin, with skin hue
essentially frozen.

**What this does not cover:** protection is statistical, not semantic. An
unusually saturated skin tone — strong colored light, heavy makeup, a costume
in a skin-adjacent hue — has high chroma and *will* be treated as a saturated
color. There is no hue-aware skin qualifier in Phase 2. Phase 3's hue density
controls will make it possible to pull the orange band back independently.

Subtractive saturation is excluded from the skin figures above, deliberately:
it is a saturation control and is *expected* to saturate skin. Measured
separately it moves mid skin 0.853 stop at maximum, bounded and hue-stable.

## 1.8 LUT looks — findings and the open decision

A set of 19 `.cube` look LUTs was supplied (15 x 33³, 4 x 65³). Two properties
of that set determine how they can be used, and neither is a code problem.

### 1.8.1 They are display-referred Rec.709 LUTs

Eleven state `# Input: Rec.709` in their own headers. The other eight, baked in
Resolve, state nothing, but all 19 have a 0..1 domain and a 0..1 output range,
which is the signature of a display-referred LUT.

CineCore works in DaVinci Wide Gamut / DaVinci Intermediate. **These are not
interchangeable.** DI mid-grey is code value 0.336 and DI 1.0 is linear 100, so
a Rec.709 LUT reads a DI mid-grey as something close to black, and the gamuts do
not match either. Applying one directly to DI data produces a broken image, not
a look.

Resolving this inside the DCTL requires a DaVinci WG/Intermediate -> Rec.709
conversion, which collides with three standing project rules: no Rec.709
transform baked in, no output CST unless requested, keep the pipeline
scene-referred.

**DECIDED: route A.** The LUTs are re-baked offline into DI-native looks, so no
conversion happens inside the DCTL and every project rule is preserved. The
procedure is `BAKING.md`. The three routes as costed:

| Route | What it needs | Consequence |
|---|---|---|
| **A. Re-bake the LUTs for DI input** | Resolve node tree `CST DI->709 / LUT / CST 709->DI`, then Generate 3D LUT | Best fit. LUTs become native to the working space, no conversion in the DCTL, every project rule preserved, and the stage can merge straight into CineCore. Costs LUT resolution across DI's very wide range |
| **B. Keep the look stage on a separate node after a CST** | Nothing — works today | Zero risk, standard Resolve practice, no rule changes. Costs the single-node integration |
| **C. Add a DWG->Rec.709 conversion inside the DCTL** | Lifting the three rules, plus an official DWG matrix | Single node, full integration. But the matrix is not in the documentation available here and must not be derived, so this route is blocked on a documented matrix being supplied |

The look stage now lives **inside `CineCore.dctl`** (Section 10), implementing
route A: it expects DaVinci Intermediate and applies no conversion whatsoever.

The trade accepted in merging it: **`CineCore.dctl` no longer compiles without
the full LUT set.** A DCTL that references a missing `.cube` fails to build, so
the grading engine and the LUT folder are now a package. `tools/LUTProbe.dctl`
exists to disambiguate build failures — it declares one LUT and nothing else,
so it separates "the macros are wrong" from "a file is missing".

The look is applied at step 16, after color separation and before output
safety, matching the project's processing order. Phase 3's hue density shaping
goes before it; split toning, bleach bypass and the highlight / shadow color
shaping go after.

### 1.8.1a Highlights above diffuse white — a correction

An earlier note implied the above-white clipping was a cost of route A. **It is
not.** A display-referred LUT has a 0..1 domain; diffuse white is DI 0.5138, so
everything brighter saturates during the lookup no matter how the conversion is
arranged. Route B clips in exactly the same place, because the LUT node clamps
to its own domain either way.

| DI code | linear | Rec.709 encoded | in domain? |
|---|---|---|---|
| 0.3360 | 0.18 | 0.4894 | yes, mid grey |
| 0.5138 | 1.00 | 0.9999 | yes, diffuse white |
| 0.6000 | 2.27 | 1.4067 | no |
| 0.8000 | 15.08 | 3.0973 | no |

The mitigation is CineCore's Highlight Shoulder, which exists for this: roll
speculars below diffuse white before the look stage and nothing is lost. This
is also how the real process works — highlights are graded into range before a
print emulation, not after.

### 1.8.1c Resolve's Generate 3D LUT did not capture the Color Space Transform

The first re-bake of all 19 looks came back **functionally identical to the
originals**. Verified two ways:

- **Neutral axis prediction.** On the neutral axis a white-point-preserving
  gamut matrix cancels, so a correct bake is fully predictable from the DI
  transfer function and the Rec.709 curve alone. Six hypotheses were tested
  against the delivered files. "No colour space conversion at all" matched at
  RMS 0.0001 DI; every hypothesis involving a real conversion was off by
  0.16–0.20 DI, roughly two stops.
- **Full-volume comparison.** Sampling each original at every grid point of its
  re-baked counterpart, including fully saturated corners where a gamut
  conversion would be unmissable, the worst difference anywhere in any of the
  19 cubes was 0.00006 — against a 33³ neighbour-to-neighbour step of 0.02,
  which is 300x below the quantisation floor.

Cause: Color Space Transform is a ResolveFX plugin, and the generated LUT
appears to capture only the primary grade and LUT nodes. The viewer showed the
conversion working the whole time, which is why this is invisible by eye.

**This also exposed a flaw in the original BAKING.md verification.** The
"disable the LUT node and check the image is unchanged" test validates the
*viewer*, and passes just as happily when the CSTs are being dropped from LUT
generation entirely. It has been replaced with a decisive test: generate a LUT
from a single CST node and check whether the file is an identity ramp.

The reliable route is to bake in software from the documented DWG matrix,
which removes Resolve's LUT generator from the path entirely. Blocked on that
matrix being supplied from documentation — it must not be derived.

### 1.8.1b Why the bake must have tone and gamut mapping disabled

Neither is invertible. With either enabled on the CSTs, the round trip is not a
clean conversion and every baked LUT silently inherits the error. `BAKING.md`
includes a ten-second self-check for this: disabling the LUT node must leave the
image completely unchanged, since the two CSTs should cancel exactly.

### 1.8.2 Licensing

Eleven of the LUTs are Dehancer-generated and carry an explicit notice in their
headers: property of Dehancer Ltd, usable by the plugin licence owner only,
copying and sharing prohibited.

Consequences applied:

- The `.cube` files are **not** committed. `.gitignore` excludes `*.cube` so
  they cannot be added by accident.
- `CineCoreLook.dctl` contains no LUT data. It references files by name, so it
  is distributable even though its LUT set is not.
- **If CineCore is ever distributed commercially, this LUT set cannot go with
  it.** The parametric look engine in Phase 4 is the route to shippable looks;
  the LUT selector is best treated as a personal-use convenience layer.

### 1.8.3 Syntax confidence

`DEFINE_LUT` / `APPLY_LUT` are used as documented DCTL features. Confidence is
high but **unverified in this environment** — no Resolve compiler is available
here, and no DCTL documentation was supplied to the project to check against.
The C-level structure is verified by the same shim method used for CineCore.
`CineCoreLook.dctl` is deliberately small so it doubles as the syntax test: if
the macros are wrong, it fails alone and `CineCore.dctl` keeps building.

This is also why the LUT stage is not inside `CineCore.dctl`: a DCTL that
references a missing LUT file fails to compile, so merging the stage would make
the main grading tool unbuildable for anyone without this exact LUT set.

### 1.8.2 Software bake — the working method

Blackmagic's published DWG matrices were supplied, so the bake was done in
software (`tools/bake_luts.py`), with Resolve's Generate 3D LUT out of the path
entirely. The supplied matrices were checked before use: inverse pair to
1.2e-08, white point and all three primary chromaticities matching the stated
values to ~1e-09.

Rec.709 enters only because the sources are Rec.709 LUTs. It is confined to the
offline baker and appears nowhere in the DCTL.

**Gamma choice.** The BT.709 OETF, matching the LUTs' own "Rec.709" label.
Still the one unresolved judgement call — the files do not state whether they
mean the OETF or a 2.4 power function. Worth knowing: for the look to land
exactly as authored, the encoding used in the bake should also match the
project's output transform, since the two then cancel. One flag in the baker
re-bakes the set as `gamma24`.

**Results.** Round trip exact (0.00e+00) at every grid point below diffuse
white, including in-gamut colour; independent forward recomputation agrees to
4.6e-09; output correctly ceilings at DI 0.513837.

**Two measured 33³ characteristics, neither a bake error.** Grid points are
uniform in DI but exponential in linear, so interpolating the curved round trip
between them is imperfect:

| | 33³ | 65³ |
|---|---|---|
| Interpolation error at the clamp knee | 0.18 stop | 0.05 stop |
| Worst diagonal non-monotonicity | 0.055 stop | 0.018 stop |

The knee error rounds off the corner where Rec.709 saturates at diffuse white,
which softens the clip rather than damaging it. The diagonal dip is the less
welcome one — the source LUTs show zero — but at 0.055 stop it is small, and
65³ halves rather than removes it. 33-point was delivered as specified; 65 is
an `out_size` change away.

**A verification lesson worth keeping.** Four checks failed on the first run and
all four were badly specified, not real defects: identity and forward-agreement
were compared against *interpolated* values rather than at grid points, where
the answer is exact; "neutrals stay neutral" was applied to creative looks,
which are entitled to tint greys and mostly do; and monotonicity was measured on
the trilinear diagonal, which mixes in off-diagonal corners. Testing a numeric
pipeline at the points it actually defines is the difference between measuring
the pipeline and measuring the sampler.

## 1.9 Phase 3 — hue density, split toning, bleach bypass

### 1.9.1 Two spec items resolved rather than built twice

The control list names **Warm Highlights / Cool Shadows** under Color Character
and **Highlight Warmth / Shadow Coolness** under Effects, and the processing
order lists *split toning* at 17 and *highlight / shadow color shaping* at
19–20. These describe one operation. It is implemented **once**, as split
toning, rather than shipped as two identical control pairs. Flagged rather than
silently chosen: if the Effects pair was meant to be something different — a
second, independent toning axis, say — it is a small addition.

**Fade** and **Soft Highlight Compression** are in the Effects group but not in
the Phase 3 list, so they are not built. Deferred.

### 1.9.2 Hue-specific density shaping

Exactly the Phase 2 film-density operator — a uniform log offset, so hue and
chroma are preserved bit-exactly (verified) — scaled by a smooth weight on hue
as well as chroma.

Hue is measured without trigonometry, using the standard sextant coordinate in
60° units from the sorted channels: Red 0, Yellow 1, Green 2, Cyan 3, Blue 4,
Magenta 5, with Orange at 0.5. Each band is a smoothstep lobe wrapping across
the 0/6 seam.

**The stability argument that makes this safe.** Hue is ill-conditioned as
chroma approaches zero — numerator and denominator both vanish. Two things
neutralise that: pixels below the chroma floor return untouched, and above it
the effect scales with chroma itself, so the weight vanishes exactly as fast as
the hue estimate becomes unreliable. A noisy hue on a near-neutral pixel is
multiplied by nearly nothing. Verified: near-neutral pixels move < 0.002 DI with
all seven bands at full opposing deflection.

**This closes the semantic skin gap left open in 1.7.5.** Measured, all three
skin patches:

| patch | hue | Orange band weight | +1 Orange | +1 Green | +1 Blue |
|---|---|---|---|---|---|
| shadow skin | 0.472 | 0.998 | −0.231 stop | 0.000 | 0.000 |
| mid skin | 0.465 | 0.996 | −0.238 stop | 0.000 | 0.000 |
| highlight skin | 0.465 | 0.996 | −0.195 stop | 0.000 | 0.000 |
| saturated red | 0.187 | 0.767 | −0.617 stop | 0.000 | 0.000 |

Skin sits dead centre of the Orange band, and the Green and Blue bands do not
reach it at all.

**Limitations.** The bands overlap by design — non-overlapping bands would band
visibly at the boundaries — so Red, Orange and Yellow reach into each other and
setting all three stacks. The chroma gate here is **linear**, not the quadratic
used by film density, because a hue control aimed at skin has to actually reach
skin. That makes these bands less neutral-safe than the Phase 2 operators, which
is the right trade for a targeted tool.

### 1.9.3 Split toning

A single warm/cool axis in log, applied with opposite tone weights at the two
ends. The axis is made **luminance-neutral by subtracting its own luminance**
before use, so toning moves colour without moving brightness at all — verified
exact. A toning control that quietly changes exposure is one of the easiest ways
to make a grade drift.

The two tone weights are smoothsteps that both reach exactly zero at the
crossover and never overlap, so midtones at the crossover are untouched
(verified) and the two ends cannot fight. Split Tone Balance slides the
crossover.

**Limitations.** This deliberately tints neutrals — that is what toning is, and
it is the one place in CineCore where a neutral is intentionally moved. The axis
is a fixed warm/cool direction rather than a free hue, keeping the control count
low at the cost of not being able to tone towards, say, green shadows.
Luminance neutrality is exact only under the Section 2.2 weighting.

### 1.9.4 Bleach bypass

Two coupled moves from one strength control, because in the real process they
are one thing.

1. **Desaturation towards luminance.** *Labelled approximation.* A literal
   density sum would be a uniform log offset, which leaves log chroma untouched
   and so would not look desaturated at all. What is modelled is the perceptual
   result of a heavy neutral silver image sitting over the dye — reduced
   colorfulness — not the arithmetic of density addition.
2. **A log gamma pinned at black and at diffuse white.** Silver density is
   highest where the image is already dark, so shadows deepen while white holds.
   Pinning both ends means the effect cannot shift exposure or blow highlights.
   Above diffuse white the curve continues linearly at the slope it had reached,
   so the pieces meet with matching value *and* slope (verified C1). Below zero
   it is mirrored, staying monotonic through black rather than folding.

**The two controls are genuinely different, not a strength and a copy.** Bleach
Bypass sets how strong the effect is; Bleach Bypass Mix blends that result
against the untouched image. A strong effect at half mix keeps the deep blacks
and restores midtone colour; a half-strength effect scales everything down
together. Verified to differ.

**Limitation.** This is the one Phase 3 operator that deliberately reduces
shadow separation. The curve is a smooth power function, not a clamp, so it
stays strictly increasing — but blocked-up blacks are the intended look and
pushing it hard will produce them.

**Why not an overlay blend**, the usual shortcut: it is not invertible, has no
defined behaviour outside 0–1, and would clip the wide-gamut negatives later
stages need.

## 1.10 Inline LUT embedding

### 1.10.1 Corrections to an earlier assessment

Two things in the previous analysis were wrong and are withdrawn:

- **Ownership.** The looks were assumed to be third-party assets on the basis of
  text in the source files' headers. They are the project owner's original
  LUTs, distributable commercially. No licensing constraint applies, and the
  conclusion built on that assumption — that these looks could not ship — is
  void. Ownership is not inferable from file headers and should not have been
  treated as evidence.
- **Evidence quality.** gcc timings were cited as a proxy for Resolve's GPU
  compilation, and CUDA `__constant__` memory was named as the likely storage
  for inline LUTs. Neither is supported: gcc is an AOT CPU compiler, and no
  documentation of Resolve's inline LUT storage was available. Both are
  withdrawn as evidence. The only reliable measurement of Resolve's behaviour
  is Resolve.

What survives from the measurement is the source-size arithmetic, which is
independent of any of that: a 33³ LUT is 1.13 MB of source text and 35,937
entries; twenty is roughly 23.7 MB.

### 1.10.2 Architecture: inline, via DEFINE_CUBE_LUT

`DEFINE_CUBE_LUT` carries LUT data inline, so the DCTL has no external file
dependency and no missing file can disable the engine. This is the direction
being tested, and it is strictly better than the external-file design on every
requirement the project has: self-contained, unbreakable by file placement, and
preserving the creative data exactly rather than approximating it.

**Syntax status.** The exact macro form is the one element that cannot be
verified in this project — no Resolve compiler, no DCTL documentation supplied.
The proof of concept uses a block form mirroring the .cube layout:

```
DEFINE_CUBE_LUT( NAME )
LUT_3D_SIZE 33
<r> <g> <b>
...
END_CUBE_LUT
```

The data is machine-generated by `tools/make_poc.py`, so if the real signature
differs it is a re-emit, not a redesign.

### 1.10.3 Proof of concept

`CineCore_PoC.dctl`, 2.33 MB, generated from `CineCore.dctl` so the engine
stays a single source of truth. Carries the complete Phase 1–3 engine, every
control, and a three-entry Film Look menu.

**Two LUTs are embedded, not one, deliberately.** Whether multiple inline LUTs
can coexist in a single DCTL is what decides whether all 19 can be embedded, so
the proof of concept has to test it rather than assume it.

`None` is included alongside Reference Identity because every CineCore control
is neutral at its default, and Reference Identity is only *nearly* a no-op — it
carries the 33³ interpolation error and clamps above diffuse white, as any LUT
does.

**Verified before hand-off:** both embedded LUTs are bit-identical to their
source `.cube` files, all 35,937 entries each, max difference 0.0e+00, with red
index fastest as the format requires. No `DEFINE_LUT` and no `luts/` path
survives anywhere in the file. The engine compiles clean under
`-Wall -Wextra -Wshadow` with the inline blocks stubbed, and defaults remain a
bit-exact pass-through.

What only Resolve can confirm: that the macro form is right, that two inline
LUTs coexist, and what the compile actually costs.

## 2. Labelled approximations

| Approximation | Nature | Why accepted |
|---|---|---|
| BT.709 luma coefficients on DWG values | Not colorimetric for this gamut | Positive, standard, stable; see 1.1 |
| White balance as normalized channel gains | Not a chromatic adaptation transform | A true CAT needs a documented DWG matrix this project does not have. Gain model is the standard well-behaved substitute, exact in stops and exposure-preserving on neutrals |
| Saturation in log rather than linear | Not chromaticity-preserving in a strict colorimetric sense | Log saturation is far gentler and reads as color rather than clipping; linear saturation drives channels negative almost immediately on saturated color. Extreme boosts drift slightly in hue |
| 25% per-channel share in shoulder and toe | Introduces a small hue shift by construction | This *is* the film behaviour being modelled; at 25% it stays well short of a pure per-channel curve's hue skew |
| Bleach bypass desaturation | Not a literal density sum, which would leave log chroma untouched | Models the perceptual result of a neutral silver image over the dye, which is what the look actually is |
| BT.709 OETF assumed for the source LUTs' encoding | The files say "Rec.709" without settling OETF vs 2.4 power | Matches the label; re-bakeable with one flag if the looks land wrong |
| Hue position `p` measured in log RGB | Not a perceptual hue space | No trigonometry, no ill-conditioning near neutral, and the six primaries are exact fixed points. A perceptual hue space would need a documented DWG matrix this project does not have |
| Chroma as max − min channel spread | Not a colorimetric chroma | Exactly zero for neutrals, always non-negative, no square root or arc tangent, and stable to the last bit near grey — which is precisely where a hue-angle formulation fails |
| Skin protection via chroma selectivity | Statistical, not semantic | No hue detection is used anywhere in Phase 2; an unusually saturated skin tone will be treated as saturated color. Measured 12.7× selectivity between saturated red and mid skin |

---

## 3. Verification (Phase 1)

Maths ported line-for-line to a reference model and tested; the `.dctl` also
compiles clean as C under `-Wall -Wextra -Wshadow` against a shim whose
`float3` has no operator overloads, proving no reliance on vector arithmetic.

- Defaults are a **bit-exact** pass-through (20k random pixels, zero delta)
- No control alters the image at its default value
- Neutral grey stays neutral across all non-color controls (3,645 combinations)
- No NaN or Inf from 60k random control/pixel combinations including NaN, ±Inf, ±1e30 inputs
- Output stays inside the soft limits over 40k random grades
- Tone chain is monotonic on a neutral ramp (4k random grades)
- Exposure is exact in stops (max relative error 2e-5)
- White balance preserves neutral luminance exactly (error < 1e-12)
- Saturation preserves luma exactly, does not rotate the color vector, and gives exact monochrome at 0
- Shoulder is C0/C1 continuous at the knee
- Toe keeps black pinned at exactly 0 and is strictly increasing
- Shadow depth does not drive black negative
- Linear mode round-trips at defaults

**Phase 2 additions:**

- Neutrals untouched by all four operators at every setting
- Film density never brightens a channel, and preserves channel differences bit-exactly (hue-safe)
- Subtractive saturation never raises the max channel and never reduces chroma; gap expansion monotonic
- Richness holds the median channel exactly fixed
- Color separation preserves min, max and chroma exactly; hue mapping strictly increasing; the six primaries are exact fixed points
- Skin within 0.174 stop under density + richness + separation at maximum, hue position essentially frozen
- Constant-hue exposure ramp stays monotonic in luma through the whole chain

Every division is guarded or has a non-zero compile-time constant denominator;
`_expf` is clamped, `_logf` is floored, `_sqrtf` is floored, and the single
`_powf` call takes a base floored at zero with a positive exponent.

---

## 4. Phase status

| Phase | Contents | Status |
|---|---|---|
| 1 | Structure, UI, safety, exposure, WB/tint, contrast, pivot, saturation, shoulder, toe, depth, black/white point | **Complete** |
| 2 | Film density, subtractive saturation, richness, color separation | **Complete** |
| 3 | Hue densities, split toning, highlight warmth, shadow cooling, bleach bypass | **Complete** |
| 4 | Film-look engine and profile architecture | Not started |
| 5 | ~20 film looks on the shared engine | Not started |
| — | LUT look selector, 19 looks, merged into `CineCore.dctl` Section 10 | **Route A adopted. Awaiting the re-bake** |
| 6 | GPU, cleanliness, stability, Resolve compatibility | Not started |

Phase 3 inserts at the marked point in `SECTION 11`, after color separation.
`SECTION 9` (color character) is where the hue density, split toning and
bleach bypass functions belong. No restructuring required.

---

## 5. Open questions

1. **Luminance weighting** — unchanged and still labelled an approximation.
   Swappable at any time in Section 2.2; Phase 2 added no new dependence on it.
2. **Skin qualification** — closed by Phase 3. Orange Density is the semantic
   skin handle; see 1.9.2 for measurements.
3. **The duplicated toning controls** — see 1.9.1. Split toning is implemented
   once. Say if the Effects-group pair was meant to be a separate axis.
