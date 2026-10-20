// =============================================================================
//  Gradelux FilmLab — host shim for compiling the DCTL as C (Version 9 / 9.1 / 10)
//  Version 9.1 and 10 removed controls from the interface; their globals stay
//  declared here (unused by the newer versions) so one shim builds all three.
//  Version 10: #define FILMLAB_V10 before including this file. That adds the
//  three LUT looks (Vista, Lux, Laguna) to the look enumeration and an
//  APPLY_LUT emulation (see below). The Version 10 .dctl itself cannot be
//  compiled as C because of its embedded .cube text; build the host copy
//  first:  python3 tools/v10_luts.py strip build/Gradelux_FilmLab_v10_host.dctl
// -----------------------------------------------------------------------------
//  Shared by the tools and tests. Provides the DCTL built-ins the source uses,
//  the dropdown enumerations (as Resolve generates them from DEFINE_UI_PARAMS)
//  and every UI control as a global at its DCTL default. Include this, then
//  the .dctl:
//      #include "../tools/filmlab_shim.h"
//      #include "../Gradelux_FilmLab_v9_1.dctl"   (or _v9.dctl, or the v10 host copy)
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
       GLX_LOOK_VELOUR,
#if defined(FILMLAB_V10)
       GLX_LOOK_VISTA, GLX_LOOK_LUX, GLX_LOOK_LAGUNA,
#endif
       GLX_LOOK_COUNT };
#define GLX_LOOK_ENGINE_COUNT (GLX_LOOK_VELOUR + 1)   // Off + the 18 film-engine looks
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
                                    "Patina", "Opaline", "Glacier", "Terracotta", "Velour",
                                    "Vista", "Lux", "Laguna" };
static const char *PRINT_NAMES[] = { "None", "Cinechrome 2383", "Velachrome 2393", "Lumachrome 3513",
                                     "Retrochrome 3510", "Silkchrome 2383" };
static const char *SEC_NAMES[] = { "Off", "Neutral", "Warm", "Cool", "Pastel", "Rich", "Golden", "Faded",
                                   "Reversal", "Night", "Daylight", "Portrait", "Chrome", "Emerald",
                                   "Steel Chrome", "Vivid", "Cinema E" };

#if defined(FILMLAB_V10)
// ---- APPLY_LUT emulation (host only) ------------------------------------------
// Resolve applies the embedded .cube with its own LUT engine. On the host the
// same luts/<Name>.cube file is loaded (GlxLutVista -> luts/Vista.cube) and
// interpolated like a 3D LUT: input clamped to 0..1, red index fastest (the
// .cube order). filmlab_lut_interp: 0 = trilinear, 1 = tetrahedral (Resolve's
// two project settings). This checks the DCTL's plumbing around the lookup;
// it does not prove Resolve's own interpolation is bit-identical.
static int filmlab_lut_interp = 0;
typedef struct { const char *name; int n; float *v; } FilmlabLut;
static FilmlabLut filmlab_luts[8];
static int filmlab_nluts = 0;

static const char *filmlab_lut_dir(void)
{
    const char *e = getenv("FILMLAB_LUT_DIR");
    if (e) return e;
    static char dir[1024];
    const char *f = __FILE__;                        // .../tools/filmlab_shim.h
    const char *sl = strrchr(f, '/');
    if (!sl) return "../luts";
    snprintf(dir, sizeof dir, "%.*s/../luts", (int)(sl - f), f);
    return dir;
}

static FilmlabLut *filmlab_lut_get(const char *ident)
{
    for (int i = 0; i < filmlab_nluts; i++) if (!strcmp(filmlab_luts[i].name, ident)) return &filmlab_luts[i];
    const char *base = strncmp(ident, "GlxLut", 6) == 0 ? ident + 6 : ident;
    char path[1200]; snprintf(path, sizeof path, "%s/%s.cube", filmlab_lut_dir(), base);
    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr, "APPLY_LUT emulation: cannot open %s\n", path); exit(1); }
    char line[512]; int n = 0, k = 0; float *v = NULL;
    while (fgets(line, sizeof line, fp)) {
        if (!strncmp(line, "LUT_3D_SIZE", 11)) { n = atoi(line + 11); v = (float *)malloc(sizeof(float) * 3 * n * n * n); continue; }
        float a, b, c;
        if (v && k < n * n * n && sscanf(line, "%f %f %f", &a, &b, &c) == 3) { v[3*k] = a; v[3*k+1] = b; v[3*k+2] = c; k++; }
    }
    fclose(fp);
    if (!v || k != n * n * n) { fprintf(stderr, "APPLY_LUT emulation: bad cube %s\n", path); exit(1); }
    FilmlabLut *L = &filmlab_luts[filmlab_nluts++];
    L->name = ident; L->n = n; L->v = v;
    return L;
}

static float filmlab_lut_at(const FilmlabLut *L, int r, int g, int b, int ch)
{
    return L->v[3 * (r + L->n * (g + L->n * b)) + ch];
}

static float3 filmlab_apply_lut(float r, float g, float b, const char *ident)
{
    FilmlabLut *L = filmlab_lut_get(ident);
    int n1 = L->n - 1;
    float x[3] = { _saturatef(r) * n1, _saturatef(g) * n1, _saturatef(b) * n1 };
    int i[3]; float f[3];
    for (int c = 0; c < 3; c++) { i[c] = (int)floorf(x[c]); if (i[c] >= n1) i[c] = n1 - 1; f[c] = x[c] - i[c]; }
    float o[3];
    for (int ch = 0; ch < 3; ch++) {
#define C(a, bb, cc) filmlab_lut_at(L, i[0] + (a), i[1] + (bb), i[2] + (cc), ch)
        if (filmlab_lut_interp == 0) {
            float c00 = C(0,0,0) + (C(1,0,0) - C(0,0,0)) * f[0], c10 = C(0,1,0) + (C(1,1,0) - C(0,1,0)) * f[0];
            float c01 = C(0,0,1) + (C(1,0,1) - C(0,0,1)) * f[0], c11 = C(0,1,1) + (C(1,1,1) - C(0,1,1)) * f[0];
            float c0 = c00 + (c10 - c00) * f[1], c1 = c01 + (c11 - c01) * f[1];
            o[ch] = c0 + (c1 - c0) * f[2];
        } else {
            float fr = f[0], fg = f[1], fb = f[2];
            if (fr > fg) {
                if (fg > fb)      o[ch] = (1-fr)*C(0,0,0) + (fr-fg)*C(1,0,0) + (fg-fb)*C(1,1,0) + fb*C(1,1,1);
                else if (fr > fb) o[ch] = (1-fr)*C(0,0,0) + (fr-fb)*C(1,0,0) + (fb-fg)*C(1,0,1) + fg*C(1,1,1);
                else              o[ch] = (1-fb)*C(0,0,0) + (fb-fr)*C(0,0,1) + (fr-fg)*C(1,0,1) + fg*C(1,1,1);
            } else {
                if (fb > fg)      o[ch] = (1-fb)*C(0,0,0) + (fb-fg)*C(0,0,1) + (fg-fr)*C(0,1,1) + fr*C(1,1,1);
                else if (fb > fr) o[ch] = (1-fg)*C(0,0,0) + (fg-fb)*C(0,1,0) + (fb-fr)*C(0,1,1) + fr*C(1,1,1);
                else              o[ch] = (1-fg)*C(0,0,0) + (fg-fr)*C(0,1,0) + (fr-fb)*C(1,1,0) + fb*C(1,1,1);
            }
        }
#undef C
    }
    return make_float3(o[0], o[1], o[2]);
}
#define APPLY_LUT(r, g, b, ident) filmlab_apply_lut((r), (g), (b), #ident)
#define DEFINE_CUBE_LUT(...)
#endif

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
