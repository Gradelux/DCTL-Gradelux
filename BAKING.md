# Re-baking look LUTs for DaVinci Intermediate input

The supplied `.cube` looks expect **Rec.709** input. CineCore works in
**DaVinci Wide Gamut / DaVinci Intermediate**. Rather than convert inside the
DCTL — which would mean baking a Rec.709 assumption and a CST into the grading
engine — the LUTs are converted once, offline, into DI-native looks.

After baking, `CineCoreLook.dctl` applies them with **no colour space
conversion at all**, and the look stage can live on the same node as the rest
of CineCore.

---

## 1. Project setup

Use **DaVinci YRGB**, not colour managed, in Project Settings → Color
Management → Color Science.

This matters. In an unmanaged project Resolve feeds a plain 0–1 ramp through
the node graph when generating a LUT, with no hidden transform in front of it.
The first CST node is what declares "this ramp is DaVinci Intermediate", so the
generated LUT is DI-in by construction. In a colour managed project there is an
extra transform in the path and the result depends on settings that are easy to
get wrong.

Open any clip in the Color page. The clip's own content is irrelevant — the
bake samples the node graph, not the picture — but a real shot is useful for
the visual check in step 4.

---

## 2. Node tree

Three nodes, in order:

### Node 1 — Color Space Transform

| Setting | Value |
|---|---|
| Input Color Space | DaVinci Wide Gamut |
| Input Gamma | DaVinci Intermediate |
| Output Color Space | Rec.709 |
| Output Gamma | Rec.709 |
| Tone Mapping Method | **None** |
| Gamut Mapping Method | **None** |
| Forward OOTF | **None** |
| Inverse OOTF | **None** |

### Node 2 — the original look LUT

Right-click the node → LUT → browse to the source `.cube`. Nothing else on
this node.

### Node 3 — Color Space Transform, the exact inverse of node 1

| Setting | Value |
|---|---|
| Input Color Space | Rec.709 |
| Input Gamma | Rec.709 |
| Output Color Space | DaVinci Wide Gamut |
| Output Gamma | DaVinci Intermediate |
| Tone Mapping Method | **None** |
| Gamut Mapping Method | **None** |
| Forward OOTF | **None** |
| Inverse OOTF | **None** |

**Tone mapping and gamut mapping must be None on both.** They are not
invertible. With either enabled the round trip is not a clean conversion and
every baked LUT inherits the error.

---

## 3. Which Rec.709 gamma?

Resolve offers both `Rec.709` and `Gamma 2.4`, and they are different curves:
`Rec.709` is the BT.709 OETF with its linear toe, `Gamma 2.4` is a pure power
function. The LUTs say only "Rec.709", which does not settle it.

Start with `Rec.709` on both CSTs, matching the label. Then check: apply the
same source LUT the way you normally do and compare against the three-node
chain above on the same shot. They should match. If they do not, switch both
CSTs to `Gamma 2.4` and compare again.

Whichever matches, use it for **every** LUT in the set, and keep node 1 and
node 3 on the same choice.

---

## 4. Verify the round trip before baking anything

This check costs ten seconds and catches every settings mistake:

**Disable node 2** (the LUT) with its number key. The image should now look
**completely unchanged** from the source — node 1 and node 3 should cancel
exactly.

If the image shifts at all, the two CSTs are not inverse-matched. Check that
tone mapping and gamut mapping are None on both, and that the gamma choice
matches. Do not bake until this check passes.

Re-enable node 2 when it does.

---

## 5. Generate

Right-click the clip in the timeline → **Generate 3D LUT (CUBE)** → choose a
size, then save into the `luts` folder beside the DCTL, named exactly as
listed in `luts/README.md`.

**Size:** 65-point is recommended. The DI → Rec.709 → DI round trip is
strongly curved, especially in the shadows, and 33 points can band there even
when the source LUT was 33³ itself. The cost is GPU memory:

| Size | Per LUT | 19 LUTs |
|---|---|---|
| 33³ | ≈ 0.4 MB | ≈ 8 MB |
| 65³ | ≈ 3.3 MB | ≈ 63 MB |

Use 33-point if GPU memory is tight, and check the shadows of a dark shot for
banding.

Repeat nodes 2 → 5 for each look: swap the LUT on node 2, generate, save.

---

## 6. What to expect afterwards

**Above diffuse white, the baked LUT is flat.** Diffuse white is DI code value
0.5138 (linear 1.0). A display-referred LUT has a 0–1 domain, so everything
brighter than that saturates to the same output:

| DI code | linear | Rec.709 encoded | inside the LUT domain? |
|---|---|---|---|
| 0.3360 | 0.18 | 0.4894 | yes — mid grey |
| 0.5138 | 1.00 | 0.9999 | yes — diffuse white |
| 0.6000 | 2.27 | 1.4067 | no, clips |
| 0.8000 | 15.08 | 3.0973 | no, clips |

**This is not caused by baking.** Applying these LUTs through a live CST clips
in exactly the same place — the LUT node clamps to its own domain either way.
It is a property of a display-referred LUT meeting scene-referred data.

The fix is CineCore's **Highlight Shoulder**, which exists for precisely this:
roll speculars down under diffuse white before the look stage and nothing is
lost. Grading the highlights into range before a print emulation is also how
the real process works.

---

## 7. Naming

Save each baked LUT under the same filename it had before, into `luts/`.
`CineCoreLook.dctl` references files by name, so no code changes are needed —
the DI-native LUT simply replaces the Rec.709 one.

Keep the originals somewhere outside `luts/`. If the gamma choice in step 3
turns out to be wrong, you will want to re-bake from them.
