// =============================================================================
//  Gradelux FilmLab — host shim for compiling the DCTL as C (Version 9 / 9.1)
//  Version 9.1 removed some controls from the interface; their globals stay
//  declared here (unused by 9.1) so the same shim builds both versions.
// -----------------------------------------------------------------------------
//  Shared by the tools and tests. Provides the DCTL built-ins the source uses,
//  the dropdown enumerations (as Resolve generates them from DEFINE_UI_PARAMS)
//  and every UI control as a global at its DCTL default. Include this, then
//  the .dctl:
//      #include "../tools/filmlab_shim.h"
//      #include "../Gradelux_FilmLab_v9_1.dctl"   (or _v9.dctl)
//  filmlab_reset() puts every control back to its default.
// =============================================================================
#ifndef FILMLAB_SHIM_H
#define FILMLAB_SHIM_H
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float x, y, z; } float3;
static float3 make_float3(float a, float b, float c) { float3 r = {a, b, c}; return r; }
typedef struct { float x, y; } float2;
static float2 make_float2(float a, float b) { float2 r = {a, b}; return r; }
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

// ---- dropdown enumerations (internal IDs; labels below) ----
enum { GLX_LOOK_OFF, GLX_LOOK_VERONA_250D, GLX_LOOK_VERDANT_250D, GLX_LOOK_MIDNIGHT_800T, GLX_LOOK_AURELIA_400,
       GLX_LOOK_SOLARA_200, GLX_LOOK_MOSS_250D, GLX_LOOK_CHROMA_100, GLX_LOOK_SILVERGRAIN_100,
       GLX_LOOK_TUNGSTEN_500T, GLX_LOOK_HALCYON_400, GLX_LOOK_ARGENT_200, GLX_LOOK_NOCTURNE_800,
       GLX_LOOK_PASTELLE_400, GLX_LOOK_PATINA, GLX_LOOK_OPALINE, GLX_LOOK_GLACIER, GLX_LOOK_TERRACOTTA,
       GLX_LOOK_VELOUR, GLX_LOOK_COUNT };
enum { GLX_PRINT_NONE, GLX_PRINT_CINECHROME_2383, GLX_PRINT_VELACHROME_2393, GLX_PRINT_LUMACHROME_3513,
       GLX_PRINT_RETROCHROME_3510, GLX_PRINT_SILKCHROME_2383, GLX_PRINT_COUNT };
enum { GLX_SEC_OFF, GLX_SEC_NEUTRAL, GLX_SEC_WARM, GLX_SEC_COOL, GLX_SEC_PASTEL, GLX_SEC_RICH, GLX_SEC_GOLDEN,
       GLX_SEC_FADED, GLX_SEC_REVERSAL, GLX_SEC_NIGHT_500T, GLX_SEC_DAYLIGHT_250D, GLX_SEC_PORTRAIT_400,
       GLX_SEC_CHROME_64, GLX_SEC_EMERALD_400, GLX_SEC_STEEL_CHROME, GLX_SEC_VIVID_50, GLX_SEC_CINEMA_E,
       GLX_SEC_COUNT };
enum { GLX_DIAG_NORMAL, GLX_DIAG_COLOR, GLX_DIAG_TONE, GLX_DIAG_FULL };

// Customer-facing labels (Version 9: no stock numbers)
static const char *LOOK_NAMES[] = { "Off", "Verona", "Verdant", "Midnight", "Aurelia", "Solara", "Moss", "Chroma",
                                    "Silvergrain", "Tungsten", "Halcyon", "Argent", "Nocturne", "Pastelle",
                                    "Patina", "Opaline", "Glacier", "Terracotta", "Velour" };
static const char *PRINT_NAMES[] = { "None", "Cinechrome 2383", "Velachrome 2393", "Lumachrome 3513",
                                     "Retrochrome 3510", "Silkchrome 2383" };
static const char *SEC_NAMES[] = { "Off", "Neutral", "Warm", "Cool", "Pastel", "Rich", "Golden", "Faded",
                                   "Reversal", "Night", "Daylight", "Portrait", "Chrome", "Emerald",
                                   "Steel Chrome", "Vivid", "Cinema E" };

// ---- every UI control, at its DCTL default ----
static int   ui_Look, ui_Print, ui_SecLook, ui_SecShowSkin, ui_Diag;
static float ui_Exposure, ui_Temperature, ui_Tint;
static float ui_CreativeIntensity;
static float ui_SecStrength, ui_SecSep, ui_SecSat, ui_SecSubtr, ui_SecBias, ui_SecTone, ui_SecSkinProt;
static float ui_PrintStrength, ui_PrintContrast, ui_PrintDensity, ui_PrintSaturation, ui_PrintRolloff;
static float ui_ColorDensity, ui_Separation, ui_HlWarmth, ui_ShadowTone;
static float ui_Density, ui_SubSat, ui_Contrast, ui_Shadow, ui_Rolloff;
static float ui_ShHue, ui_ShSat, ui_MdHue, ui_MdSat, ui_HlHue, ui_HlSat, ui_SplitBalance, ui_SplitStrength;
static float ui_Intensity;

static void filmlab_reset(void)
{
    ui_Look = 0; ui_Print = 0; ui_SecLook = 0; ui_SecShowSkin = 0; ui_Diag = 0;
    ui_Exposure = 0; ui_Temperature = 0; ui_Tint = 0;
    ui_CreativeIntensity = 100;
    ui_SecStrength = ui_SecSep = ui_SecSat = ui_SecSubtr = ui_SecBias = ui_SecTone = ui_SecSkinProt = 1.0f;
    ui_PrintStrength = 100; ui_PrintContrast = ui_PrintDensity = ui_PrintSaturation = ui_PrintRolloff = 0;
    ui_ColorDensity = 0; ui_Separation = 50; ui_HlWarmth = 0; ui_ShadowTone = 0;
    ui_Density = ui_SubSat = ui_Contrast = ui_Shadow = ui_Rolloff = 0;
    ui_ShHue = 220; ui_ShSat = 0; ui_MdHue = 35; ui_MdSat = 0; ui_HlHue = 45; ui_HlSat = 0;
    ui_SplitBalance = 0; ui_SplitStrength = 100;
    ui_Intensity = 100;
}
#endif
