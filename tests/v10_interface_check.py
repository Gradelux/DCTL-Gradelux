#!/usr/bin/env python3
"""Gradelux FilmLab Version 10: interface and content check (static, no compiler).

Checks Gradelux_FilmLab_v10.dctl for:
  - exactly two dropdowns and eleven sliders, in the specified order and with
    the Version 9.1 internal names, ranges and defaults;
  - the Film Look dropdown: 19 options (Off + the 18 film looks, IDs unchanged)
    and the Secondary Film Look dropdown: 17 options (Off + 16 Cine16);
  - no film print code or control left, and no LUT looks or LUT data.
Run from the repository root:  python3 tests/v10_interface_check.py
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = open(os.path.join(ROOT, "Gradelux_FilmLab_v10.dctl"), encoding="utf-8").read()
code = src
fails = 0
def ok(cond, what):
    global fails
    print(("  ok    " if cond else "  FAIL  ") + what)
    fails += 0 if cond else 1

params = re.findall(r"^DEFINE_UI_PARAMS\((\w+),\s*([^,]+),\s*(\w+),(.*)\)\s*$", code, re.M)
combos = [p for p in params if p[2] == "DCTLUI_COMBO_BOX"]
sliders = [p for p in params if p[2] == "DCTLUI_SLIDER_FLOAT"]
print("Interface")
ok(len(params) == 13, "13 controls (found %d)" % len(params))
ok(len(combos) == 2, "2 dropdowns: %s" % [c[1] for c in combos])
want = [("ui_Exposure", "Exposure", "0.0, -5.0, 5.0, 0.1"), ("ui_Temperature", "Temperature", "0.0, -100.0, 100.0, 1.0"),
        ("ui_Tint", "Tint", "0.0, -100.0, 100.0, 1.0"), ("ui_CreativeIntensity", "Film Strength", "100.0, 0.0, 200.0, 1.0"),
        ("ui_SecStrength", "Secondary Strength", "1.00, 0.0, 1.5, 0.01"), ("ui_Contrast", "Film Contrast", "0.0, -100.0, 100.0, 1.0"),
        ("ui_Density", "Film Density", "0.0, -100.0, 100.0, 1.0"), ("ui_SubSat", "Subtractive Saturation", "0.0, -100.0, 100.0, 1.0"),
        ("ui_Rolloff", "Highlight Roll-Off", "0.0, 0.0, 100.0, 1.0"), ("ui_Shadow", "Shadow Depth", "0.0, -100.0, 100.0, 1.0"),
        ("ui_Intensity", "Global Intensity", "100.0, 0.0, 100.0, 1.0")]
got = [(p[0], p[1].strip(), re.sub(r"\s+", " ", p[3].strip())) for p in sliders]
ok(len(sliders) == 11, "11 sliders (found %d)" % len(sliders))
for w, g in zip(want, got):
    ok(w == g, "slider %-22s %-22s range/default %s" % (g[0], g[1], g[2]))
order = [p[1].strip() for p in params]
ok(order == ["Exposure", "Temperature", "Tint", "Film Look", "Film Strength", "Secondary Film Look", "Secondary Strength",
             "Film Contrast", "Film Density", "Subtractive Saturation", "Highlight Roll-Off", "Shadow Depth",
             "Global Intensity"], "panel order: " + " | ".join(order))

def labels(c):
    return re.findall(r'"([^"]*)"', c[3])
look = dict((c[1].strip(), c) for c in combos)["Film Look"]
L = labels(look)
ok(len(L) == 19, "Film Look: 19 options (found %d)" % len(L))
ok(L == ["Off", "Verona", "Verdant", "Midnight", "Aurelia", "Solara", "Moss", "Chroma", "Silvergrain", "Tungsten",
         "Halcyon", "Argent", "Nocturne", "Pastelle", "Patina", "Opaline", "Glacier", "Terracotta", "Velour"],
   "Film Look order: 0 Off ... 18 Velour (as V9.1)")
S = labels(dict((c[1].strip(), c) for c in combos)["Secondary Film Look"])
ok(len(S) == 17 and S[0] == "Off", "Secondary Film Look: Off + 16 Cine16 profiles")

print("Removed content")
ok(not re.search(r"ui_Print|GLX_PRINT|glx_print|printOn|isPrint", code), "no film print control or code")
ok(not re.search(r"ui_SecSep|ui_SecSat|ui_SecSubtr|ui_SecBias|ui_SecTone|ui_SecSkinProt|ui_ShHue|ui_SplitStrength|ui_Diag|ui_SecShowSkin|ui_ColorDensity|ui_Separation|ui_HlWarmth|ui_ShadowTone", code),
   "no split-toning, extra film, Cine16 detail or diagnostic controls")
ok(not re.search(r"\b(vista|lux|laguna|daze|koda)\b", src, re.I), "no Vista, Lux, Laguna, Daze or Koda")
ok(not re.search(r"DEFINE_CUBE_LUT|DEFINE_LUT|APPLY_LUT", src), "no embedded or external LUTs")

print("INTERFACE CHECK PASSED" if fails == 0 else "INTERFACE CHECK: %d FAILURES" % fails)
sys.exit(1 if fails else 0)
