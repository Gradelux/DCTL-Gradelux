# CineCore — Development Notes

Running record of colour-science decisions, approximations and verification.
Phase 1 is complete and internally consistent: no placeholder maths.

---

## 1. Colour-science decisions

### 1.1 Luminance weighting — **implementation choice, not a derivation**

**Decision.** CineCore uses the ITU-R BT.709 luma coefficients
(0.2126 / 0.7152 / 0.0722) as a positive weighting applied directly to the
working RGB values. They live in `SECTION 2.2` and nowhere else.

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
| Saturated colour | How much a saturated colour appears to darken under luma-preserving operations, and where the shoulder/toe knees fall for strongly coloured pixels. Visible, but a **character** difference. |
| Failure mode avoided | A negative coefficient would invert luma-preserving maths on saturated blues. That one *is* a correctness issue. |

Verdict: **largely a design choice, not a material image-quality issue** — with
the single exception that the sign of the coefficients genuinely matters.

To change it, edit only the three defines in Section 2.2. Equal weights
(0.33333333 each) are gamut-agnostic and equally stable, at the cost of a
poorer perceptual match.

### 1.2 DaVinci Intermediate transfer function

Constants are the published DI values, not values fitted here. They are
self-checking against the encoding's documented landmarks, which were verified
numerically before use:

| linear | code value |
|---|---|
| 0.00 | 0.000000 |
| 0.18 | 0.336043 |
| 1.00 | 0.513837 |
| 10.0 | 0.756599 |
| 100.0 | 1.000000 |

The two segments meet continuously at the cut point and the pair round-trips
to float precision. **If your documentation states different constants,
replace them in Section 2.1 — nothing else in the file needs to change.**

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
- **No output colour-space transform.** Input and output are both in the
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
recover colour. Values are sanitised once on input and once on output only —
never between stages, since every function is total.

---

## 2. Labelled approximations

| Approximation | Nature | Why accepted |
|---|---|---|
| BT.709 luma coefficients on DWG values | Not colorimetric for this gamut | Positive, standard, stable; see 1.1 |
| White balance as normalised channel gains | Not a chromatic adaptation transform | A true CAT needs a documented DWG matrix this project does not have. Gain model is the standard well-behaved substitute, exact in stops and exposure-preserving on neutrals |
| Saturation in log rather than linear | Not chromaticity-preserving in a strict colorimetric sense | Log saturation is far gentler and reads as colour rather than clipping; linear saturation drives channels negative almost immediately on saturated colour. Extreme boosts drift slightly in hue |
| 25% per-channel share in shoulder and toe | Introduces a small hue shift by construction | This *is* the film behaviour being modelled; at 25% it stays well short of a pure per-channel curve's hue skew |

---

## 3. Verification (Phase 1)

Maths ported line-for-line to a reference model and tested; the `.dctl` also
compiles clean as C under `-Wall -Wextra -Wshadow` against a shim whose
`float3` has no operator overloads, proving no reliance on vector arithmetic.

- Defaults are a **bit-exact** pass-through (20k random pixels, zero delta)
- No control alters the image at its default value
- Neutral grey stays neutral across all non-colour controls (3,645 combinations)
- No NaN or Inf from 60k random control/pixel combinations including NaN, ±Inf, ±1e30 inputs
- Output stays inside the soft limits over 40k random grades
- Tone chain is monotonic on a neutral ramp (4k random grades)
- Exposure is exact in stops (max relative error 2e-5)
- White balance preserves neutral luminance exactly (error < 1e-12)
- Saturation preserves luma exactly, does not rotate the colour vector, and gives exact monochrome at 0
- Shoulder is C0/C1 continuous at the knee
- Toe keeps black pinned at exactly 0 and is strictly increasing
- Shadow depth does not drive black negative
- Linear mode round-trips at defaults

Every division is guarded or has a non-zero compile-time constant denominator;
`_expf` is clamped, `_logf` is floored, `_sqrtf` is floored, `_powf` is unused.

---

## 4. Phase status

| Phase | Contents | Status |
|---|---|---|
| 1 | Structure, UI, safety, exposure, WB/tint, contrast, pivot, saturation, shoulder, toe, depth, black/white point | **Complete** |
| 2 | Film density, subtractive saturation, richness, colour separation | Not started |
| 3 | Hue densities, split toning, highlight warmth, shadow cooling, bleach bypass | Not started |
| 4 | Film-look engine and profile architecture | Not started |
| 5 | ~20 film looks on the shared engine | Not started |
| 6 | GPU, cleanliness, stability, Resolve compatibility | Not started |

Phase 2 inserts at the marked point in `SECTION 9`, between black/white point
and the existing saturation. No restructuring required.

---

## 5. Open questions

1. **DI constants** — please confirm against your DCTL documentation. They are
   self-consistent and land on the documented landmarks, but confirmation is
   cheap and they are the one place a wrong number would be systematic.
2. **Luminance weighting** — happy to switch to equal weights, or to a
   documented DWG set if your documentation contains one.
