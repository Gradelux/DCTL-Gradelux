# CineCore — Development Notes

Running record of color-science decisions, approximations and verification.
Phases 1 and 2 are complete and internally consistent: no placeholder maths.

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
scene-referred. **Not resolved unilaterally — awaiting a decision.** The three
viable routes, in the order they are recommended:

| Route | What it needs | Consequence |
|---|---|---|
| **A. Re-bake the LUTs for DI input** | Resolve node tree `CST DI->709 / LUT / CST 709->DI`, then Generate 3D LUT | Best fit. LUTs become native to the working space, no conversion in the DCTL, every project rule preserved, and the stage can merge straight into CineCore. Costs LUT resolution across DI's very wide range |
| **B. Keep the look stage on a separate node after a CST** | Nothing — works today | Zero risk, standard Resolve practice, no rule changes. Costs the single-node integration |
| **C. Add a DWG->Rec.709 conversion inside the DCTL** | Lifting the three rules, plus an official DWG matrix | Single node, full integration. But the matrix is not in the documentation available here and must not be derived, so this route is blocked on a documented matrix being supplied |

`CineCoreLook.dctl` implements route B today and is written so that routes A and
C need no change to its logic — only to what the LUTs contain, or to what feeds
the node.

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

## 2. Labelled approximations

| Approximation | Nature | Why accepted |
|---|---|---|
| BT.709 luma coefficients on DWG values | Not colorimetric for this gamut | Positive, standard, stable; see 1.1 |
| White balance as normalized channel gains | Not a chromatic adaptation transform | A true CAT needs a documented DWG matrix this project does not have. Gain model is the standard well-behaved substitute, exact in stops and exposure-preserving on neutrals |
| Saturation in log rather than linear | Not chromaticity-preserving in a strict colorimetric sense | Log saturation is far gentler and reads as color rather than clipping; linear saturation drives channels negative almost immediately on saturated color. Extreme boosts drift slightly in hue |
| 25% per-channel share in shoulder and toe | Introduces a small hue shift by construction | This *is* the film behaviour being modelled; at 25% it stays well short of a pure per-channel curve's hue skew |
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
| 3 | Hue densities, split toning, highlight warmth, shadow cooling, bleach bypass | Not started |
| 4 | Film-look engine and profile architecture | Not started |
| 5 | ~20 film looks on the shared engine | Not started |
| — | `CineCoreLook.dctl`, LUT look selector, 19 looks | **Usable, pending the 1.8.1 decision** |
| 6 | GPU, cleanliness, stability, Resolve compatibility | Not started |

Phase 3 inserts at the marked point in `SECTION 11`, after color separation.
`SECTION 9` (color character) is where the hue density, split toning and
bleach bypass functions belong. No restructuring required.

---

## 5. Open questions

1. **Luminance weighting** — unchanged and still labelled an approximation.
   Swappable at any time in Section 2.2; Phase 2 added no new dependence on it.
2. **Skin qualification** — Phase 2 protects skin only through chroma
   selectivity. If semantic skin protection is wanted, the natural place is
   Phase 3's hue density band for orange.
