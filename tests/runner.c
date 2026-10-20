// =============================================================================
//  Gradelux FilmLab / Cine16 regression runner
// -----------------------------------------------------------------------------
//  Compiles ONE DCTL (chosen at build time) as C through a small shim and runs
//  its transform() on samples read from stdin, printing exact results
//  (C99 hex floats, %a). tests/cine16_regression.py builds three runners and
//  compares them; it can also chain them (one runner's output feeding the next).
//
//  Build (from tests/):
//    gcc -O2 -DRUN_CINE16  -o run_c16 runner.c -lm   original Cine16 DCTL (reference/)
//    gcc -O2 -DRUN_FILMLAB7 -o run_v7 runner.c -lm   Gradelux_FilmLab_v7.dctl
//    gcc -O2 -DRUN_FILMLAB8 -o run_v8 runner.c -lm   Gradelux_FilmLab_v8.dctl
//    gcc -O2 -DRUN_FILMLAB9 -o run_v9 runner.c -lm   Gradelux_FilmLab_v9.dctl (every Version 9 control at its default)
//    gcc -O2 -DRUN_FILMLAB91 -o run_v91 runner.c -lm Gradelux_FilmLab_v9_1.dctl (same line format; the
//                                                    secondary sliders other than strength are ignored:
//                                                    Version 9.1 fixes them at 1.00)
//    gcc -O2 -DRUN_FILMLAB10 -o run_v10 runner.c -lm Version 10 host copy (build/Gradelux_FilmLab_v10_host.dctl,
//                                                    from tools/v10_luts.py strip); same line format,
//                                                    print must be 0 (Version 10 has no prints); looks
//                                                    19-21 are the LUT looks (APPLY_LUT emulated)
//
//  Input: one sample per line, whitespace-separated:
//    RUN_CINE16  : profile strength sep sat subtr bias tone skinprot showskin  R G B
//    RUN_FILMLAB7: look print                                                  R G B
//    RUN_FILMLAB8 / RUN_FILMLAB9: look secondary print strength sep sat subtr bias tone skinprot showskin  R G B
//  (R G B: DaVinci Intermediate code values, decimal or hex float.) Every
//  other control stays at its default. Output: "R G B" per line, %a.
// =============================================================================
#if defined(RUN_FILMLAB10)
#define RUN_FILMLAB9
#define FILMLAB_V10
#include "../tools/filmlab_shim.h"
#include "../build/Gradelux_FilmLab_v10_host.dctl"
#elif defined(RUN_FILMLAB91)
#define RUN_FILMLAB9
#include "../tools/filmlab_shim.h"
#include "../Gradelux_FilmLab_v9_1.dctl"
#elif defined(RUN_FILMLAB9)
#include "../tools/filmlab_shim.h"
#include "../Gradelux_FilmLab_v9.dctl"
#else
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float x, y, z; } float3;
static float3 make_float3(float a, float b, float c) { float3 r = {a, b, c}; return r; }
#define __DEVICE__ static
#define __CONSTANT__ static const
#define __CONSTANTREF__ const
#define _exp2f exp2f
#define _log2f log2f
#define _powf powf
#define _fabs fabsf
#define _fmaxf fmaxf
#define _fminf fminf
#define _sqrtf sqrtf
#define _cosf cosf
#define _sinf sinf
#define _fmod fmodf
static float _clampf(float x, float a, float b) { return fminf(fmaxf(x, a), b); }
static float _saturatef(float x) { return fminf(fmaxf(x, 0.0f), 1.0f); }
static int min(int a, int b) { return a < b ? a : b; }
static int max(int a, int b) { return a > b ? a : b; }
#define DEFINE_UI_PARAMS(...)
#define DEFINE_UI_TOOLTIP(...)

#if defined(RUN_CINE16)
// ---- original Cine16 UI ----
enum { PROF_NEUTRAL, PROF_WARM, PROF_COOL, PROF_PASTEL, PROF_RICH, PROF_GOLDEN, PROF_FADED, PROF_REVERSAL,
       PROF_NIGHT_500T, PROF_DAYLIGHT_250D, PROF_PORTRAIT_400, PROF_CHROME_64, PROF_EMERALD_400,
       PROF_STEEL_CHROME, PROF_VIVID_50, PROF_CINEMA_E };
static int   p_profile = 0, p_showskin = 0;
static float p_strength = 1, p_sep = 1, p_sat = 1, p_subtr = 1, p_bias = 1, p_tone = 1, p_skinprot = 1;
#include "../reference/Cine16_1B_Color_Profile.dctl"
#else
// ---- FilmLab UI ----
enum { GLX_LOOK_OFF, GLX_LOOK_VERONA_250D, GLX_LOOK_VERDANT_250D, GLX_LOOK_MIDNIGHT_800T, GLX_LOOK_AURELIA_400,
       GLX_LOOK_SOLARA_200, GLX_LOOK_MOSS_250D, GLX_LOOK_CHROMA_100, GLX_LOOK_SILVERGRAIN_100,
       GLX_LOOK_TUNGSTEN_500T, GLX_LOOK_HALCYON_400, GLX_LOOK_ARGENT_200, GLX_LOOK_NOCTURNE_800,
       GLX_LOOK_PASTELLE_400, GLX_LOOK_COUNT };
enum { GLX_PRINT_NONE, GLX_PRINT_CINECHROME_2383, GLX_PRINT_VELACHROME_2393, GLX_PRINT_LUMACHROME_3513,
       GLX_PRINT_RETROCHROME_3510, GLX_PRINT_SILKCHROME_2383, GLX_PRINT_COUNT };
enum { GLX_DIAG_NORMAL, GLX_DIAG_COLOR, GLX_DIAG_TONE, GLX_DIAG_FULL };
static int   ui_Look = 0, ui_Print = 0, ui_Diag = 0;
static float ui_CreativeIntensity = 100, ui_PrintStrength = 100, ui_PrintContrast = 0, ui_PrintDensity = 0,
             ui_PrintSaturation = 0, ui_PrintRolloff = 0;
static float ui_ColorDensity = 0, ui_Separation = 50, ui_HlWarmth = 0, ui_ShadowTone = 0;
static float ui_Density = 0, ui_SubSat = 0, ui_Contrast = 0, ui_Shadow = 0, ui_Rolloff = 0, ui_Intensity = 100;
#if defined(RUN_FILMLAB8)
enum { GLX_SEC_OFF, GLX_SEC_NEUTRAL, GLX_SEC_WARM, GLX_SEC_COOL, GLX_SEC_PASTEL, GLX_SEC_RICH, GLX_SEC_GOLDEN,
       GLX_SEC_FADED, GLX_SEC_REVERSAL, GLX_SEC_NIGHT_500T, GLX_SEC_DAYLIGHT_250D, GLX_SEC_PORTRAIT_400,
       GLX_SEC_CHROME_64, GLX_SEC_EMERALD_400, GLX_SEC_STEEL_CHROME, GLX_SEC_VIVID_50, GLX_SEC_CINEMA_E };
static int   ui_SecLook = 0, ui_SecShowSkin = 0;
static float ui_SecStrength = 1, ui_SecSep = 1, ui_SecSat = 1, ui_SecSubtr = 1, ui_SecBias = 1,
             ui_SecTone = 1, ui_SecSkinProt = 1;
#include "../Gradelux_FilmLab_v8.dctl"
#else
#include "../Gradelux_FilmLab_v7.dctl"
#endif
#endif
#endif   /* RUN_FILMLAB9 */

int main(void)
{
#if defined(RUN_FILMLAB9)
    filmlab_reset();
#endif
    char line[1024];
    while (fgets(line, sizeof line, stdin)) {
        char *s = line, *e;
        double v[16]; int n = 0;
        while (n < 16) { double d = strtod(s, &e); if (e == s) break; v[n++] = d; s = e; }
        if (n == 0) continue;
#if defined(RUN_CINE16)
        if (n != 12) { fprintf(stderr, "bad line (%d values)\n", n); return 1; }
        p_profile = (int)v[0]; p_strength = (float)v[1]; p_sep = (float)v[2]; p_sat = (float)v[3];
        p_subtr = (float)v[4]; p_bias = (float)v[5]; p_tone = (float)v[6]; p_skinprot = (float)v[7];
        p_showskin = (int)v[8];
        float3 o = transform(0, 0, 0, 0, (float)v[9], (float)v[10], (float)v[11]);
#elif defined(RUN_FILMLAB8) || defined(RUN_FILMLAB9)
        if (n != 14) { fprintf(stderr, "bad line (%d values)\n", n); return 1; }
        ui_Look = (int)v[0]; ui_SecLook = (int)v[1]; ui_Print = (int)v[2];
#if defined(RUN_FILMLAB10)
        if (ui_Print != 0) { fprintf(stderr, "Version 10 has no film prints\n"); return 1; }
#endif
        ui_SecStrength = (float)v[3]; ui_SecSep = (float)v[4]; ui_SecSat = (float)v[5]; ui_SecSubtr = (float)v[6];
        ui_SecBias = (float)v[7]; ui_SecTone = (float)v[8]; ui_SecSkinProt = (float)v[9]; ui_SecShowSkin = (int)v[10];
        float3 o = transform(0, 0, 0, 0, (float)v[11], (float)v[12], (float)v[13]);
#else
        if (n != 5) { fprintf(stderr, "bad line (%d values)\n", n); return 1; }
        ui_Look = (int)v[0]; ui_Print = (int)v[1];
        float3 o = transform(0, 0, 0, 0, (float)v[2], (float)v[3], (float)v[4]);
#endif
        printf("%a %a %a\n", o.x, o.y, o.z);
    }
    return 0;
}
