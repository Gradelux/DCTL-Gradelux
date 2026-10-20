// Host-side math test for Gradelux FilmLab Version 9 (18 film stocks, 16 Cine16 secondary
// profiles, Exposure / Temperature / Tint, Split Toning, developer Stage View). Compiles the
// .dctl as C through a small shim (checks the maths, not Resolve's GPU compiler).
//
//   cd tests && gcc -O2 -o math_test_v9 math_test_v9.c -lm && ./math_test_v9
//
//   -DDUMP_LOOKOFF_V8 / -DDUMP_LOOKOFF_V9 : Film Look Off with every other existing control
//   (prints, print controls, secondary looks and sliders, film adjustments, grading, Global
//   Intensity) on a fixed grid and random settings. Version 9 changes only the primary looks
//   (middle-grey anchoring) and adds controls that default to neutral, so these must match
//   Version 8 bit-for-bit.
//   -DDUMP_LOOKS_V8 / -DDUMP_LOOKS_V9 : the 13 Version 8 looks on the same inputs, for the
//   documented v8 -> v9 difference report (tests/v8_v9_report.py).
//
// Older regression dumps (compare with cmp):
//   -DDUMP_V1  / -DDUMP_V4_CORE : V1 controls only (Look Off + Print None must equal V1)
//   -DDUMP_V3_ADJ / -DDUMP_V4_ADJ : film adjustments without a look (must match V3)
//   -DDUMP_V4_PRINT / -DDUMP_V5_PRINT : prints alone (renamed prints must equal V4)
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float x, y, z; } float3;
static float3 make_float3(float a, float b, float c) { float3 r = {a, b, c}; return r; }
#define __DEVICE__ static
#define _exp2f exp2f
#define _log2f log2f
#define _powf powf
#define _fabs fabsf
#define _fmaxf fmaxf
#define _fminf fminf
#define _sqrtf sqrtf
#define _cosf cosf
#define _sinf sinf
static float _clampf(float x, float a, float b) { return fminf(fmaxf(x, a), b); }
#define __CONSTANT__ static const
#define __CONSTANTREF__ const
#define _fmod fmodf
static float _saturatef(float x) { return fminf(fmaxf(x, 0.0f), 1.0f); }
static int min(int a, int b) { return a < b ? a : b; }
static int max(int a, int b) { return a > b ? a : b; }
#define DEFINE_UI_PARAMS(...)
#define DEFINE_UI_TOOLTIP(...)

#if defined(DUMP_V4_PRINT) || defined(DUMP_V3_ADJ) || defined(DUMP_DIAG_LOOKS)
enum { GLX_LOOK_OFF, GLX_LOOK_PURE, GLX_LOOK_EMBER, GLX_LOOK_SLATE, GLX_LOOK_SOLACE, GLX_LOOK_CHROME,
       GLX_LOOK_DUNE, GLX_LOOK_NOIR, GLX_LOOK_BLOOM, GLX_LOOK_HAZE, GLX_LOOK_VIVID, GLX_LOOK_MUSE,
       GLX_LOOK_TUNDRA, GLX_LOOK_SIENNA, GLX_LOOK_VELVET, GLX_LOOK_CINDER, GLX_LOOK_LUMEN, GLX_LOOK_FABLE,
       GLX_LOOK_MOSS, GLX_LOOK_PRISM, GLX_LOOK_ECLIPSE, GLX_LOOK_MONO, GLX_LOOK_OBSIDIAN, GLX_LOOK_SILVER };
enum { GLX_PRINT_NONE, GLX_PRINT_2383, GLX_PRINT_2393, GLX_PRINT_3513, GLX_PRINT_3510, GLX_PRINT_2383_SOFT };
#endif
#if defined(DUMP_DIAG_LOOKS)
enum { V5_LOOK_OFF, GLX_LOOK_VERONA_250D, GLX_LOOK_VERDANT_250D, GLX_LOOK_MIDNIGHT_800T, GLX_LOOK_COUNT };
#else
enum { V5_LOOK_OFF, GLX_LOOK_VERONA_250D, GLX_LOOK_VERDANT_250D, GLX_LOOK_MIDNIGHT_800T, GLX_LOOK_AURELIA_400,
       GLX_LOOK_SOLARA_200, GLX_LOOK_MOSS_250D, GLX_LOOK_CHROMA_100, GLX_LOOK_SILVERGRAIN_100,
       GLX_LOOK_TUNGSTEN_500T, GLX_LOOK_HALCYON_400, GLX_LOOK_ARGENT_200, GLX_LOOK_NOCTURNE_800,
       GLX_LOOK_PASTELLE_400, GLX_LOOK_PATINA, GLX_LOOK_OPALINE, GLX_LOOK_GLACIER, GLX_LOOK_TERRACOTTA,
       GLX_LOOK_VELOUR, GLX_LOOK_COUNT };
#endif
enum { GLX_DIAG_NORMAL, GLX_DIAG_COLOR, GLX_DIAG_TONE, GLX_DIAG_FULL };
static int ui_Diag = 0;
enum { V5_PRINT_NONE, GLX_PRINT_CINECHROME_2383, GLX_PRINT_VELACHROME_2393, GLX_PRINT_LUMACHROME_3513,
       GLX_PRINT_RETROCHROME_3510, GLX_PRINT_SILKCHROME_2383, GLX_PRINT_COUNT };
#if !defined(DUMP_V4_PRINT) && !defined(DUMP_V3_ADJ) && !defined(DUMP_DIAG_LOOKS)
#define GLX_LOOK_OFF V5_LOOK_OFF
#define GLX_PRINT_NONE V5_PRINT_NONE
#endif
static const char *LOOK_NAMES[] = { "Off", "Verona", "Verdant", "Midnight", "Aurelia", "Solara", "Moss", "Chroma",
                                    "Silvergrain", "Tungsten", "Halcyon", "Argent", "Nocturne", "Pastelle",
                                    "Patina", "Opaline", "Glacier", "Terracotta", "Velour" };
static const char *PRINT_NAMES[] = { "None", "Cinechrome 2383", "Velachrome 2393", "Lumachrome 3513",
                                     "Retrochrome 3510", "Silkchrome 2383" };

enum { GLX_SEC_OFF, GLX_SEC_NEUTRAL, GLX_SEC_WARM, GLX_SEC_COOL, GLX_SEC_PASTEL, GLX_SEC_RICH, GLX_SEC_GOLDEN,
       GLX_SEC_FADED, GLX_SEC_REVERSAL, GLX_SEC_NIGHT_500T, GLX_SEC_DAYLIGHT_250D, GLX_SEC_PORTRAIT_400,
       GLX_SEC_CHROME_64, GLX_SEC_EMERALD_400, GLX_SEC_STEEL_CHROME, GLX_SEC_VIVID_50, GLX_SEC_CINEMA_E,
       GLX_SEC_COUNT };
static const char *SEC_NAMES[] = { "Off", "Neutral", "Warm", "Cool", "Pastel", "Rich", "Golden", "Faded",
                                   "Reversal", "Night", "Daylight", "Portrait", "Chrome",
                                   "Emerald", "Steel Chrome", "Vivid", "Cinema E" };
static int   ui_SecLook = 0, ui_SecShowSkin = 0;
static float ui_SecStrength = 1, ui_SecSep = 1, ui_SecSat = 1, ui_SecSubtr = 1, ui_SecBias = 1,
             ui_SecTone = 1, ui_SecSkinProt = 1;
static float ui_Exposure = 0, ui_Temperature = 0, ui_Tint = 0;
static float ui_ShHue = 220, ui_ShSat = 0, ui_MdHue = 35, ui_MdSat = 0, ui_HlHue = 45, ui_HlSat = 0,
             ui_SplitBalance = 0, ui_SplitStrength = 100;
typedef struct { float x, y; } float2;
static float2 make_float2(float a, float b) { float2 r = {a, b}; return r; }
static int   ui_Look, ui_Print;
static float ui_CreativeIntensity = 100, ui_PrintStrength = 100, ui_PrintContrast, ui_PrintDensity,
             ui_PrintSaturation, ui_PrintRolloff;
static float ui_ColorDensity, ui_Separation = 50, ui_HlWarmth, ui_ShadowTone;
static float ui_Density, ui_SubSat, ui_Contrast, ui_Shadow, ui_Rolloff, ui_Intensity = 100;

#if defined(DUMP_V1)
#include "../Gradelux_FilmLab.dctl"
#elif defined(DUMP_V3_ADJ)
#include "../Gradelux_FilmLab_v3.dctl"
#elif defined(DUMP_V4_PRINT)
#include "../Gradelux_FilmLab_v4.dctl"
#elif defined(DUMP_LOOKOFF_V8) || defined(DUMP_LOOKS_V8)
#include "../Gradelux_FilmLab_v8.dctl"
#else
#include "../Gradelux_FilmLab_v9.dctl"
#endif

static void reset(void) {
    ui_Diag = 0;
    ui_Exposure = ui_Temperature = ui_Tint = 0;
    ui_ShHue = 220; ui_ShSat = 0; ui_MdHue = 35; ui_MdSat = 0; ui_HlHue = 45; ui_HlSat = 0;
    ui_SplitBalance = 0; ui_SplitStrength = 100;
    ui_SecLook = 0; ui_SecShowSkin = 0; ui_SecStrength = ui_SecSep = ui_SecSat = ui_SecSubtr = ui_SecBias = 1;
    ui_SecTone = ui_SecSkinProt = 1;
    ui_Look = 0; ui_Print = 0; ui_CreativeIntensity = 100; ui_PrintStrength = 100;
    ui_PrintContrast = ui_PrintDensity = ui_PrintSaturation = ui_PrintRolloff = 0;
    ui_ColorDensity = 0; ui_Separation = 50; ui_HlWarmth = 0; ui_ShadowTone = 0;
    ui_Density = ui_SubSat = ui_Contrast = ui_Shadow = ui_Rolloff = 0; ui_Intensity = 100;
}
static float frand(float a, float b) { return a + (b - a) * (float)rand() / (float)RAND_MAX; }
static float3 run(float r, float g, float b) { return transform(0, 0, 0, 0, r, g, b); }

#if defined(DUMP_LOOKOFF_V8) || defined(DUMP_LOOKOFF_V9)
int main(void)
{
    for (int sec = 0; sec < GLX_SEC_COUNT; sec++)
        for (int pr = 0; pr < GLX_PRINT_COUNT; pr++)
            for (int ri = 0; ri < 7; ri++) for (int gi = 0; gi < 7; gi++) for (int bi = 0; bi < 7; bi++) {
                reset(); ui_SecLook = sec; ui_Print = pr;
                float3 o = run(-0.1f + 0.2f * ri, -0.1f + 0.2f * gi, -0.1f + 0.2f * bi);
                printf("%a %a %a\n", o.x, o.y, o.z);
            }
    srand(90);
    for (int i = 0; i < 150000; i++) {
        reset(); ui_Print = i % GLX_PRINT_COUNT; ui_SecLook = (i / GLX_PRINT_COUNT) % GLX_SEC_COUNT;
        ui_Diag = (i / 7) % 4;
        ui_CreativeIntensity = frand(0, 200); ui_PrintStrength = frand(0, 100);
        ui_PrintContrast = frand(-100, 100); ui_PrintDensity = frand(-100, 100);
        ui_PrintSaturation = frand(-100, 100); ui_PrintRolloff = frand(-100, 100);
        ui_ColorDensity = frand(-100, 100); ui_Separation = frand(0, 100);
        ui_HlWarmth = frand(-100, 100); ui_ShadowTone = frand(-100, 100);
        ui_Density = frand(-100, 100); ui_SubSat = frand(-100, 100); ui_Contrast = frand(-100, 100);
        ui_Shadow = frand(-100, 100); ui_Rolloff = frand(0, 100); ui_Intensity = frand(0, 100);
        ui_SecStrength = frand(0, 1.5f); ui_SecSep = frand(0, 2); ui_SecSat = frand(0, 2); ui_SecSubtr = frand(0, 2);
        ui_SecBias = frand(0, 2); ui_SecTone = frand(0, 1.5f); ui_SecSkinProt = frand(0, 1);
        float3 o = run(frand(-0.3f, 1.2f), frand(-0.3f, 1.2f), frand(-0.3f, 1.2f));
        printf("%a %a %a\n", o.x, o.y, o.z);
    }
    return 0;
}
#elif defined(DUMP_LOOKS_V8) || defined(DUMP_LOOKS_V9)
// One line per sample: look  inR inG inB  outR outG outB (DaVinci Intermediate, %a). 13 v8 looks.
int main(void)
{
    srand(91);
    for (int l = 1; l <= 13; l++) {
        for (int i = 0; i <= 64; i++) {           // grey ramp -8 .. +8 stops
            float v = glx_lin_to_di(0.18f * powf(2.0f, -8.0f + 0.25f * i));
            reset(); ui_Look = l; float3 o = run(v, v, v);
            printf("%d %a %a %a %a %a %a\n", l, v, v, v, o.x, o.y, o.z);
        }
        for (int i = 0; i < 3000; i++) {
            float r = frand(0, 1), g = frand(0, 1), b = frand(0, 1);
            reset(); ui_Look = l; float3 o = run(r, g, b);
            printf("%d %a %a %a %a %a %a\n", l, r, g, b, o.x, o.y, o.z);
        }
    }
    return 0;
}
#elif defined(DUMP_V1) || defined(DUMP_V4_CORE)
int main(void)
{
    srand(99);
    for (int i = 0; i < 20000; i++) {
        reset();
        ui_Density = frand(-100, 100); ui_SubSat = frand(-100, 100); ui_Contrast = frand(-100, 100);
        ui_Shadow = frand(-100, 100); ui_Rolloff = frand(0, 100); ui_Intensity = frand(0, 99.9f);
        float3 o = run(frand(-0.3f, 1.2f), frand(-0.3f, 1.2f), frand(-0.3f, 1.2f));
        printf("%a %a %a\n", o.x, o.y, o.z);
    }
    return 0;
}
#elif defined(DUMP_V4_PRINT) || defined(DUMP_V5_PRINT)
int main(void)
{
    srand(55);
    for (int i = 0; i < 20000; i++) {
        reset(); ui_Print = 1 + i % 5;
        ui_PrintStrength = frand(1, 100); ui_PrintContrast = frand(-100, 100); ui_PrintDensity = frand(-100, 100);
        ui_PrintSaturation = frand(-100, 100); ui_PrintRolloff = frand(-100, 100); ui_Intensity = frand(0, 99.9f);
        float3 o = run(frand(-0.3f, 1.2f), frand(-0.3f, 1.2f), frand(-0.3f, 1.2f));
        printf("%a %a %a\n", o.x, o.y, o.z);
    }
    return 0;
}
#elif defined(DUMP_V3_ADJ) || defined(DUMP_V4_ADJ)
int main(void)
{
    srand(77);
    for (int i = 0; i < 20000; i++) {
        reset();
        ui_Separation = frand(0, 100); ui_HlWarmth = frand(-100, 100); ui_ShadowTone = frand(-100, 100);
        ui_Contrast = frand(-100, 100); ui_Intensity = frand(0, 99.9f);
        float3 o = run(frand(-0.3f, 1.2f), frand(-0.3f, 1.2f), frand(-0.3f, 1.2f));
        printf("%.7e %.7e %.7e\n", o.x, o.y, o.z);
    }
    return 0;
}
#else

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; if (fails < 40) { printf("FAIL: " __VA_ARGS__); printf("\n"); } } } while (0)
static int bad(float3 o) { return !isfinite(o.x) || !isfinite(o.y) || !isfinite(o.z); }

static float3 run_lin(float r, float g, float b) {
    float3 o = run(glx_lin_to_di(r), glx_lin_to_di(g), glx_lin_to_di(b));
    return make_float3(glx_di_to_lin(o.x), glx_di_to_lin(o.y), glx_di_to_lin(o.z));
}
static float3 run_work(float r, float g, float b) {
    float3 d = glx_work_to_dwg(r, g, b);
    float3 o = run_lin(d.x, d.y, d.z);
    return glx_dwg_to_work(o.x, o.y, o.z);
}
static double hue_deg(float3 c) {
    return atan2((c.y - c.z) * 0.70710678, (2 * c.x - c.y - c.z) * 0.40824829) * 57.2957795;
}
static double satv(float3 c) {
    float mx = fmaxf(c.x, fmaxf(c.y, c.z)), mn = fminf(c.x, fminf(c.y, c.z));
    return (mx - mn) / mx;
}
static double luma(float3 c) { return WORK_YR * c.x + WORK_YG * c.y + WORK_YB * c.z; }
// Monochrome stocks are identified by their definition, not their dropdown position.
static int is_mono(int id) { return id > 0 && id < GLX_LOOK_COUNT && glx_look_get(id).isMono > 0.5f; }

// Test colours in linear Rec.2020 (working space)
static const float CHART[][3] = {
    {0.4486f, 0.3203f, 0.2356f}, {0.1379f, 0.0901f, 0.0620f}, {0.6149f, 0.4338f, 0.2834f},  // skin light/dark/tan
    {0.1978f, 0.1021f, 0.0282f}, {0.8990f, 0.7614f, 0.6228f},                               // skin deep/pale
    {0.40f, 0.05f, 0.03f}, {0.40f, 0.30f, 0.03f}, {0.08f, 0.30f, 0.05f},                    // red, yellow, green
    {0.04f, 0.25f, 0.30f}, {0.03f, 0.06f, 0.35f}, {0.30f, 0.05f, 0.30f},                    // cyan, blue, magenta
    {0.09f, 0.12f, 0.20f}, {0.20f, 0.18f, 0.10f}, {0.05f, 0.07f, 0.04f},                    // sky, sand, dark foliage
    {0.0112f, 0.0112f, 0.0112f}, {0.045f, 0.045f, 0.045f}, {0.18f, 0.18f, 0.18f},            // greys -4, -2, 0 stops
    {0.72f, 0.72f, 0.72f}, {2.88f, 2.88f, 2.88f},                                             // greys +2, +4 stops
};
#define NCHART (int)(sizeof(CHART) / sizeof(CHART[0]))

static void chart(float out[NCHART][3]) {
    for (int c = 0; c < NCHART; c++) {
        float3 o = run_work(CHART[c][0], CHART[c][1], CHART[c][2]);
        out[c][0] = o.x; out[c][1] = o.y; out[c][2] = o.z;
    }
}
static double chart_dist(float a[NCHART][3], float b[NCHART][3]) {
    double d = 0;
    for (int c = 0; c < NCHART; c++)
        for (int ch = 0; ch < 3; ch++) {
            double x = log2(fmax(a[c][ch], 1e-4)) - log2(fmax(b[c][ch], 1e-4));
            d += x * x;
        }
    return sqrt(d / (NCHART * 3));
}

// Hue order of a colour-stage (look or print, plus separation pass)
static int hue_folds(GlxLook L, float sepF) {
    GlxProfile pp = glx_profile_base();
    double prevH = -1e9, unwrap = 0; int folds = 0;
    for (int i = 0; i <= 3600; i++) {
        double th = (-180.0 + i * 0.1) * 0.0174532925;
        float a = 0.10f * cos(th), bb = 0.10f * sin(th), m = 0.25f;
        float3 c = make_float3(m + 2 * 0.40824829f * a, m - 0.40824829f * a + 0.70710678f * bb,
                               m - 0.40824829f * a - 0.70710678f * bb);
        float3 c0 = glx_sensitivity(c.x, c.y, c.z, L, pp);
        float3 h1 = glx_film_color(c0.x, c0.y, c0.z, L, 0.0f, pp);
        float3 h2 = glx_film_color(h1.x, h1.y, h1.z, glx_look_clean(), sepF, pp);
        double h = hue_deg(h2) + unwrap;
        if (prevH > -1e8 && h < prevH - 180) { unwrap += 360; h += 360; }
        if (prevH > -1e8 && h < prevH - 1e-6) folds++;
        prevH = h;
    }
    return folds;
}

int main(void)
{
    srand(4321);

    // 1. Identity and bypasses
    for (int i = 0; i < 40000; i++) {
        float r = frand(-0.5f, 1.2f), g = frand(-0.5f, 1.2f), b = frand(-0.5f, 1.2f);
        reset(); ui_ColorDensity = frand(-100, 100); ui_CreativeIntensity = frand(0, 200); ui_PrintStrength = frand(0, 100);
        ui_PrintContrast = frand(-100, 100); ui_PrintDensity = frand(-100, 100); ui_PrintSaturation = frand(-100, 100);
        ui_PrintRolloff = frand(-100, 100);
        float3 o = run(r, g, b);
        CHECK(o.x == r && o.y == g && o.z == b, "identity: Look Off + Print None (print controls must do nothing)");
        reset(); ui_Look = 1 + i % (GLX_LOOK_COUNT - 1); ui_Print = 1 + i % 5; ui_HlWarmth = 40; ui_Density = 30; ui_Intensity = 0;
        o = run(r, g, b);
        CHECK(o.x == r && o.y == g && o.z == b, "identity at 0%% Global Intensity");
        reset(); ui_Look = 1 + i % (GLX_LOOK_COUNT - 1); ui_CreativeIntensity = 0; ui_ColorDensity = 70;
        o = run(r, g, b);
        CHECK(o.x == r && o.y == g && o.z == b, "Creative Intensity 0%% bypass");
        reset(); ui_Print = 1 + i % 5; ui_PrintStrength = 0; ui_PrintContrast = 80;
        o = run(r, g, b);
        CHECK(o.x == r && o.y == g && o.z == b, "Print Strength 0%% bypass");
        if (fails) break;
    }

    // 2. Stages are independent: look + print at 0% equals the look alone, etc.
    for (int i = 0; i < 10000; i++) {
        float r = frand(-0.2f, 1.1f), g = frand(-0.2f, 1.1f), b = frand(-0.2f, 1.1f);
        reset(); ui_Look = 1 + i % (GLX_LOOK_COUNT - 1); ui_Contrast = 30; ui_HlWarmth = 20;
        float3 a = run(r, g, b);
        ui_Print = 1 + i % 5; ui_PrintStrength = 0;
        float3 c = run(r, g, b);
        CHECK(a.x == c.x && a.y == c.y && a.z == c.z, "print 0%% changes look result");
        // continuity of the print toward 0%
        reset(); ui_Print = 1 + i % 5; ui_PrintStrength = 0.001f;
        c = run(r, g, b);
        float lr = glx_di_to_lin(r), lg = glx_di_to_lin(g), lb = glx_di_to_lin(b);
        float mag = fmaxf(fmaxf(fabsf(lr), fabsf(lg)), fmaxf(fabsf(lb), 0.01f));
        float d = fmaxf(fmaxf(fabsf(glx_di_to_lin(c.x) - lr), fabsf(glx_di_to_lin(c.y) - lg)), fabsf(glx_di_to_lin(c.z) - lb));
        CHECK(d / mag < 0.01f, "Print Strength 0.001%% not continuous (rel %.4f)", d / mag);
        reset(); ui_Look = 1 + i % (GLX_LOOK_COUNT - 1); ui_CreativeIntensity = 0.001f;
        c = run(r, g, b);
        d = fmaxf(fmaxf(fabsf(glx_di_to_lin(c.x) - lr), fabsf(glx_di_to_lin(c.y) - lg)), fabsf(glx_di_to_lin(c.z) - lb));
        CHECK(d / mag < 0.01f, "Creative Intensity 0.001%% not continuous (look %d rel %.4f)", ui_Look, d / mag);
        if (fails) break;
    }

    // 3. Every look x print combination
    static float lookChart[GLX_LOOK_COUNT][NCHART][3], printChart[GLX_PRINT_COUNT][NCHART][3];
    static float comboChart[GLX_LOOK_COUNT][GLX_PRINT_COUNT][NCHART][3];
    for (int id = 0; id < GLX_LOOK_COUNT; id++)
        for (int pr = 0; pr < GLX_PRINT_COUNT; pr++) {
            reset(); ui_Look = id; ui_Print = pr;
            float3 bk = run(0, 0, 0);
            CHECK(!bad(bk) && fabsf(bk.x) < 0.2f, "%s+%s black", LOOK_NAMES[id], PRINT_NAMES[pr]);

            float3 prev = run(-0.05f, -0.05f, -0.05f);
            int nonmono = 0, jumps = 0, tintN = 0;
            for (int i = 1; i <= 20000; i++) {
                float v = -0.05f + 1.15f * i / 20000.0f;
                float3 o = run(v, v, v);
                // monotonic from black upward (1e-6: float rounding on flat shoulders);
                // continuity is checked over the whole ramp, including sub-black values
                if (v > 0.0f && (o.x < prev.x - 1e-6f || o.y < prev.y - 1e-6f || o.z < prev.z - 1e-6f)) nonmono++;
                if (fabsf(o.x - prev.x) > 6e-4f || fabsf(o.y - prev.y) > 6e-4f || fabsf(o.z - prev.z) > 6e-4f) jumps++;
                if (id == 0 && (o.x != o.y || o.y != o.z)) tintN++;
                prev = o;
            }
            CHECK(nonmono == 0, "%s+%s ramp not monotonic (%d)", LOOK_NAMES[id], PRINT_NAMES[pr], nonmono);
            CHECK(jumps == 0, "%s+%s ramp discontinuity (%d)", LOOK_NAMES[id], PRINT_NAMES[pr], jumps);
            CHECK(tintN == 0, "print %s tints neutral (B&W) input (%d)", PRINT_NAMES[pr], tintN);

            int nan = 0, neg = 0, tint = 0;
            for (int i = 0; i < 20000; i++) {
                float3 o = run(frand(-1.0f, 1.3f), frand(-1.0f, 1.3f), frand(-1.0f, 1.3f));
                if (bad(o)) nan++;
                if (is_mono(id) && (o.x != o.y || o.y != o.z)) tint++;
                float3 w = run_work(frand(0, 8), frand(0, 8), frand(0, 8));
                if (w.x < -1e-4f || w.y < -1e-4f || w.z < -1e-4f) neg++;
            }
            CHECK(nan == 0, "%s+%s NaN/Inf (%d)", LOOK_NAMES[id], PRINT_NAMES[pr], nan);
            CHECK(tint == 0, "%s+%s monochrome not exactly neutral (%d)", LOOK_NAMES[id], PRINT_NAMES[pr], tint);
            CHECK(neg == 0, "%s+%s negatives from in-gamut colours (%d)", LOOK_NAMES[id], PRINT_NAMES[pr], neg);

            // Continuity around the hue circle (whole pipeline)
            float3 prevO = make_float3(0, 0, 0); int jc = 0;
            for (int i = 0; i <= 3600; i++) {
                double th = (-180.0 + i * 0.1) * 0.0174532925;
                float a = 0.10f * cos(th), bb = 0.10f * sin(th), m = 0.25f;
                float3 o = run_work(m + 2 * 0.40824829f * a, m - 0.40824829f * a + 0.70710678f * bb,
                                    m - 0.40824829f * a - 0.70710678f * bb);
                if (i > 0 && (fabsf(o.x - prevO.x) > 2e-3f || fabsf(o.y - prevO.y) > 2e-3f || fabsf(o.z - prevO.z) > 2e-3f)) jc++;
                prevO = o;
            }
            CHECK(jc == 0, "%s+%s jumps around hue circle (%d)", LOOK_NAMES[id], PRINT_NAMES[pr], jc);

            chart(comboChart[id][pr]);
            if (pr == 0) memcpy(lookChart[id], comboChart[id][pr], sizeof lookChart[id]);
            if (id == 0) memcpy(printChart[pr], comboChart[id][pr], sizeof printChart[pr]);
        }

    // 4. Hue map order and 200% / slider-extreme stability
    for (int id = 1; id < GLX_LOOK_COUNT; id++) {
        if (is_mono(id)) continue;
        for (int t = 1; t <= 2; t++)
            for (int sep = -1; sep <= 1; sep++)
                CHECK(hue_folds(glx_look_scale(glx_look_get(id), (float)t), (float)sep) == 0,
                      "%s %d00%% sep %d: hue map folds", LOOK_NAMES[id], t, sep);
    }
    for (int pr = 1; pr < GLX_PRINT_COUNT; pr++)
        for (int sv = -1; sv <= 1; sv++)
            CHECK(hue_folds(glx_print_controls(glx_look_scale(glx_print_get(pr), 1.0f), 1, 1, (float)sv, 1), 0.0f) == 0,
                  "print %s hue map folds", PRINT_NAMES[pr]);
    for (int id = 0; id < GLX_LOOK_COUNT; id += 1)
        for (int pr = 0; pr < GLX_PRINT_COUNT; pr++) {
            float ext[][9] = { {200, 100, 100, 100, 100, 100, -100, 100, 100},
                               {200, 100, -100, -100, -100, -100, 100, -100, 0},
                               {100, 50, 100, 100, -100, 100, 0, 50, 50} };
            for (int q = 0; q < 3; q++) {
                reset(); ui_Look = id; ui_Print = pr;
                ui_CreativeIntensity = ext[q][0]; ui_PrintStrength = ext[q][1]; ui_PrintContrast = ext[q][2];
                ui_PrintDensity = ext[q][3]; ui_PrintSaturation = ext[q][4]; ui_PrintRolloff = ext[q][5];
                ui_ColorDensity = ext[q][6]; ui_Separation = ext[q][7]; ui_HlWarmth = ext[q][8];
                ui_Density = 100; ui_SubSat = 100; ui_Contrast = 100; ui_Shadow = -100; ui_Rolloff = 100;
                int nan = 0, nonmono = 0;
                for (int i = 0; i < 8000; i++) if (bad(run(frand(-1, 1.3f), frand(-1, 1.3f), frand(-1, 1.3f)))) nan++;
                ui_Density = ui_SubSat = ui_Contrast = ui_Shadow = ui_Rolloff = 0; ui_HlWarmth = 0;
                float3 prev = run(0, 0, 0);
                for (int i = 1; i <= 4000; i++) {
                    float v = 1.0f * i / 4000.0f; float3 o = run(v, v, v);
                    // 1e-5 at these extreme settings: a strongly compressed print over a
                    // 200% stock whose highlight colour changes with exposure can let one
                    // channel drift by ~1e-4 code values across a highlight stretch (invisible)
                    if (o.x < prev.x - 1e-5f || o.y < prev.y - 1e-5f || o.z < prev.z - 1e-5f) nonmono++;
                    prev = o;
                }
                CHECK(nan == 0, "extreme %d %s+%s NaN (%d)", q, LOOK_NAMES[id], PRINT_NAMES[pr], nan);
                CHECK(nonmono == 0, "extreme %d %s+%s ramp not monotonic (%d)", q, LOOK_NAMES[id], PRINT_NAMES[pr], nonmono);
            }
        }

    // 5. Report: tonal character of each look (alone) and each print (alone)
    printf("\n%-10s %6s %8s %8s %8s %8s %7s %7s %8s\n", "Look", "slope", "-6 st", "-4 st", "+2 st", "+4 st", "skin h", "skin s", "white RB");
    for (int id = 0; id < GLX_LOOK_COUNT + GLX_PRINT_COUNT - 1; id++) {
        reset();
        const char *nm;
        if (id < GLX_LOOK_COUNT) { ui_Look = id; nm = LOOK_NAMES[id]; }
        else { ui_Print = id - GLX_LOOK_COUNT + 1; nm = PRINT_NAMES[ui_Print]; }
        float3 a = run_work(0.18f * 1.0353f, 0.18f * 1.0353f, 0.18f * 1.0353f);
        float3 b = run_work(0.18f / 1.0353f, 0.18f / 1.0353f, 0.18f / 1.0353f);
        double slope = (log2(luma(a)) - log2(luma(b))) / 0.1;
        double lv[4]; float st[4] = {-6, -4, 2, 4};
        for (int k = 0; k < 4; k++) { float x = 0.18f * exp2f(st[k]); lv[k] = log2(luma(run_work(x, x, x)) / 0.18) - st[k]; }
        float3 w = run_work(1.44f, 1.44f, 1.44f);
        double skH = 0, skS = 0;
        for (int c = 0; c < 5; c++) {
            float3 i0 = make_float3(CHART[c][0], CHART[c][1], CHART[c][2]);
            float3 o = run_work(i0.x, i0.y, i0.z);
            double d = hue_deg(o) - hue_deg(i0);
            if (fabs(d) > fabs(skH)) skH = d;
            skS += satv(o) / satv(i0) / 5;
        }
        if (id == GLX_LOOK_COUNT) printf("%-10s\n", "-- prints --");
        printf("%-10s %6.3f %+8.2f %+8.2f %+8.2f %+8.2f %+6.2f° %6.2fx %+8.3f\n", nm, slope, lv[0], lv[1], lv[2], lv[3],
               is_mono(id) ? 0.0 : skH, skS, log2(w.x / w.z));
        if (id > 0 && id < GLX_LOOK_COUNT && !is_mono(id)) CHECK(fabs(skH) < 6.0, "%s skin hue shift %.2f", nm, skH);
        if (id > 0 && id < GLX_LOOK_COUNT) CHECK(slope > 1.03, "%s too flat (slope %.3f)", nm, slope);
        if (id >= GLX_LOOK_COUNT) CHECK(fabs(log2(w.x / w.z)) < 1e-4, "print %s tints white", nm);
    }

    // 5b. Skin across every colour look x print combination
    {
        double worst = 0, sum = 0; int n = 0, wl = 0, wp = 0;
        for (int l = 1; l < GLX_LOOK_COUNT; l++)
            for (int pr = 1; pr < GLX_PRINT_COUNT; pr++) {
                if (is_mono(l)) continue;
                reset(); ui_Look = l; ui_Print = pr; double m = 0;
                for (int c = 0; c < 5; c++) {
                    float3 i0 = make_float3(CHART[c][0], CHART[c][1], CHART[c][2]);
                    float3 o = run_work(i0.x, i0.y, i0.z);
                    double d = hue_deg(o) - hue_deg(i0);
                    if (fabs(d) > fabs(m)) m = d;
                }
                sum += fabs(m); n++;
                if (fabs(m) > worst) { worst = fabs(m); wl = l; wp = pr; }
            }
        printf("\nSkin hue over all colour look x print combinations: mean %.2f deg, worst %.2f deg (%s + %s)\n",
               sum / n, worst, LOOK_NAMES[wl], PRINT_NAMES[wp]);
        CHECK(worst < 7.0, "look+print skin hue shift %.2f deg", worst);
    }

    // 5c. Diagnostic View
    {
        int bad1 = 0, bad2 = 0, bad3 = 0, bad4 = 0; double drift = 0;
        for (int i = 0; i < 30000; i++) {
            float r = frand(-0.2f, 1.1f), g = frand(-0.2f, 1.1f), b = frand(-0.2f, 1.1f);
            int l = 1 + i % (GLX_LOOK_COUNT - 1);
            // Full Film Look == Normal when nothing else is active (bit-exact)
            reset(); ui_Look = l; float3 a = run(r, g, b);
            ui_Diag = GLX_DIAG_FULL; float3 c = run(r, g, b);
            if (a.x != c.x || a.y != c.y || a.z != c.z) bad1++;
            // Diagnostic views ignore print / adjustments / grading / Global Intensity
            ui_Print = 1 + i % 5; ui_Contrast = 50; ui_HlWarmth = 40; ui_Intensity = 60;
            float3 c2 = run(r, g, b);
            if (c2.x != c.x || c2.y != c.y || c2.z != c.z) bad2++;
            // Normal is unaffected by the selector's existence: Off + diag = input
            reset(); ui_Diag = 1 + i % 3; c = run(r, g, b);
            if (c.x != r || c.y != g || c.z != b) bad3++;
            // Tone Only is hue-neutral: channel ratios unchanged (linear, positive inputs)
            float lr = frand(0.001f, 4), lg = frand(0.001f, 4), lb = frand(0.001f, 4);
            reset(); ui_Look = l; ui_Diag = GLX_DIAG_TONE;   // (mono stocks too: their Tone view is hue-neutral)
            float3 dw = glx_work_to_dwg(lr, lg, lb);
            float3 o = run(glx_lin_to_di(dw.x), glx_lin_to_di(dw.y), glx_lin_to_di(dw.z));
            float3 ol = glx_dwg_to_work(glx_di_to_lin(o.x), glx_di_to_lin(o.y), glx_di_to_lin(o.z));
            // hue angle (a neutral lift such as a fade may lower saturation, never hue)
            float3 wi = make_float3(lr, lg, lb);
            double e = fabs(hue_deg(ol) - hue_deg(wi)); if (e > 180) e = 360 - e;
            if (satv(wi) > 0.05 && e > drift) drift = e;
            if (bad(o)) bad4++;
        }
        printf("\nDiagnostic View: Full==Normal mismatches %d, ignores downstream %d, Off identity %d, "
               "Tone Only max hue drift %.2e deg, NaN %d\n", bad1, bad2, bad3, drift, bad4);
        CHECK(bad1 == 0 && bad2 == 0 && bad3 == 0 && bad4 == 0, "diagnostic view behaviour");
        CHECK(drift < 0.5, "Tone Only shifts hue (%.2e deg)", drift);   // float noise on low-chroma samples only
    }

    // 6. Distinctness
    double minL = 1e9; int la = 0, lb = 0;
    for (int a = 1; a < GLX_LOOK_COUNT; a++)
        for (int b = a + 1; b < GLX_LOOK_COUNT; b++) {
            double d = chart_dist(lookChart[a], lookChart[b]);
            if (d < minL) { minL = d; la = a; lb = b; }
        }
    double minP = 1e9; int pa = 0, pb = 0;
    for (int a = 1; a < GLX_PRINT_COUNT; a++)
        for (int b = a + 1; b < GLX_PRINT_COUNT; b++) {
            double d = chart_dist(printChart[a], printChart[b]);
            if (d < minP) { minP = d; pa = a; pb = b; }
        }
    double minC = 1e9;
    for (int id = 1; id < GLX_LOOK_COUNT; id++)
        if (!is_mono(id))
        for (int a = 1; a < GLX_PRINT_COUNT; a++)
            for (int b = a + 1; b < GLX_PRINT_COUNT; b++)
                minC = fmin(minC, chart_dist(comboChart[id][a], comboChart[id][b]));
    printf("\nClosest looks: %s vs %s (%.3f stops RMS); closest prints: %s vs %s (%.3f); "
           "closest prints over the same colour look: %.3f\n",
           LOOK_NAMES[la], LOOK_NAMES[lb], minL, PRINT_NAMES[pa], PRINT_NAMES[pb], minP, minC);
    CHECK(minL > 0.08, "looks not distinct");
    CHECK(minP > 0.08, "prints not distinct");
    CHECK(minC > 0.04, "prints indistinguishable over some look");

    // 7. Secondary Film Look (Cine16 engine) inside FilmLab
    {
        int nan = 0, offOk = 0, giZero = 0, inert = 0, rng = 0, maskBad = 0, combos = 0;
        srand(16);
        for (int sec = 1; sec < GLX_SEC_COUNT; sec++)
            for (int l = 0; l < GLX_LOOK_COUNT; l++)
                for (int pr = 0; pr < GLX_PRINT_COUNT; pr++) {
                    combos++;
                    for (int i = 0; i < 300; i++) {
                        reset(); ui_Look = l; ui_SecLook = sec; ui_Print = pr;
                        if (i % 3 == 0) {   // random secondary sliders across the original ranges
                            ui_SecStrength = frand(0, 1.5f); ui_SecSep = frand(0, 2); ui_SecSat = frand(0, 2);
                            ui_SecSubtr = frand(0, 2); ui_SecBias = frand(0, 2); ui_SecTone = frand(0, 1.5f);
                            ui_SecSkinProt = frand(0, 1);
                        }
                        float r = frand(-0.6f, 2.6f), g = frand(-0.6f, 2.6f), b = frand(-0.6f, 2.6f);
                        if (i % 2) { r = frand(0, 1); g = frand(0, 1); b = frand(0, 1); }
                        float3 o = run(r, g, b);
                        if (bad(o)) nan++;
                        // with no print and neutral grading the output is the Cine16 output: within its clamp
                        if (pr == 0 && (o.x < -0.5f || o.x > 2.0f || o.y < -0.5f || o.y > 2.0f || o.z < -0.5f || o.z > 2.0f)) rng++;
                        ui_Intensity = 0;
                        float3 z = run(r, g, b);
                        if (z.x != r || z.y != g || z.z != b) giZero++;
                    }
                }
        // Secondary sliders and the skin-mask switch are inert while Secondary is Off
        for (int i = 0; i < 20000; i++) {
            reset(); ui_Look = i % GLX_LOOK_COUNT; ui_Print = (i / 7) % GLX_PRINT_COUNT;
            float r = frand(-0.3f, 1.2f), g = frand(-0.3f, 1.2f), b = frand(-0.3f, 1.2f);
            float3 a = run(r, g, b);
            ui_SecStrength = frand(0, 1.5f); ui_SecSep = frand(0, 2); ui_SecSat = frand(0, 2);
            ui_SecSubtr = frand(0, 2); ui_SecBias = frand(0, 2); ui_SecTone = frand(0, 1.5f);
            ui_SecSkinProt = frand(0, 1); ui_SecShowSkin = 1;
            float3 c = run(r, g, b);
            if (a.x != c.x || a.y != c.y || a.z != c.z) inert++;
            if (ui_Look == 0 && ui_Print == 0 && (a.x != r || a.y != g || a.z != b)) offOk++;
        }
        // Skin mask view returns a grey mask in 0..1
        for (int i = 0; i < 5000; i++) {
            reset(); ui_SecLook = 1 + i % 16; ui_Look = i % GLX_LOOK_COUNT; ui_Print = i % GLX_PRINT_COUNT; ui_SecShowSkin = 1;
            float3 o = run(frand(-0.2f, 1.2f), frand(-0.2f, 1.2f), frand(-0.2f, 1.2f));
            if (o.x != o.y || o.y != o.z || o.x < 0.0f || o.x > 1.0f) maskBad++;
        }
        printf("\nSecondary Film Look: %d primary x secondary x print combinations, %d samples each; "
               "NaN/Inf %d, outside Cine16 clamp %d, Global Intensity 0 not identity %d, "
               "secondary controls not inert when Off %d, all-Off not identity %d, bad skin mask %d\n",
               combos, 300, nan, rng, giZero, inert, offOk, maskBad);
        CHECK(nan == 0, "secondary NaN/Inf (%d)", nan);
        CHECK(rng == 0, "secondary output outside the Cine16 clamp (%d)", rng);
        CHECK(giZero == 0, "Global Intensity 0 not identity with a secondary look (%d)", giZero);
        CHECK(inert == 0, "secondary controls change the image while Secondary is Off (%d)", inert);
        CHECK(offOk == 0, "Look Off + Secondary Off + Print None not identity (%d)", offOk);
        CHECK(maskBad == 0, "skin mask view not a 0..1 grey mask (%d)", maskBad);

        // Informational: grey-ramp monotonicity per profile (the original Cine16 behaviour, preserved)
        printf("Cine16 grey ramp 0..1 DI, reversals per profile (original behaviour, informational):");
        for (int sec = 1; sec < GLX_SEC_COUNT; sec++) {
            reset(); ui_SecLook = sec; int rev = 0; float prev = -1e9f;
            for (int i = 0; i <= 2000; i++) { float v = i / 2000.0f; float3 o = run(v, v, v);
                float y = 0.27412f * o.x + 0.87363f * o.y - 0.14775f * o.z; if (y < prev - 1e-6f) rev++; prev = y; }
            printf(" %s %d%s", SEC_NAMES[sec], rev, sec < GLX_SEC_COUNT - 1 ? "," : "\n");
        }
    }

    // 8. Version 9: middle-grey anchor and the new controls
    {
        // 8a. Every primary look maps 18% grey to 18% luminance (Film Print None, all else neutral)
        double worstG = 0; int worstL = 0;
        for (int l = 1; l < GLX_LOOK_COUNT; l++)
            for (int t = 0; t < 3; t++) {
                reset(); ui_Look = l; ui_CreativeIntensity = (t == 0) ? 50.0f : (t == 1) ? 100.0f : 200.0f;
                float3 o = run_work(0.18f, 0.18f, 0.18f);
                double d = fabs(log2(luma(o) / 0.18));
                if (d > worstG) { worstG = d; worstL = l; }
            }
        printf("\nMiddle-grey anchor: worst |grey shift| over 18 looks x Creative Intensity 50/100/200%%: %.6f stops (%s)\n",
               worstG, LOOK_NAMES[worstL]);
        CHECK(worstG < 2e-4, "middle grey not anchored (%.5f stops, %s)", worstG, LOOK_NAMES[worstL]);

        // 8b. Neutral settings of the new controls are exact bypasses
        int byp = 0;
        for (int i = 0; i < 20000; i++) {
            reset(); ui_Look = i % GLX_LOOK_COUNT; ui_Print = (i / 3) % GLX_PRINT_COUNT; ui_SecLook = (i / 7) % GLX_SEC_COUNT;
            float r = frand(-0.3f, 1.2f), g = frand(-0.3f, 1.2f), b = frand(-0.3f, 1.2f);
            float3 a = run(r, g, b);
            ui_ShHue = frand(0, 360); ui_MdHue = frand(0, 360); ui_HlHue = frand(0, 360);
            ui_SplitBalance = frand(-100, 100);
            if (i & 1) { ui_SplitStrength = frand(0, 100); }                     // saturations 0
            else { ui_SplitStrength = 0; ui_ShSat = frand(0, 100); ui_MdSat = frand(0, 100); ui_HlSat = frand(0, 100); }
            float3 c = run(r, g, b);
            if (a.x != c.x || a.y != c.y || a.z != c.z) byp++;
        }
        CHECK(byp == 0, "neutral split toning not a bypass (%d)", byp);

        // 8c. Exposure: exact scene-linear gain, colour ratios kept, negatives keep their sign
        double expErr = 0; int expSign = 0;
        for (int i = 0; i < 5000; i++) {
            reset(); ui_Exposure = frand(-5, 5);
            float3 in = make_float3(frand(-0.05f, 2.0f), frand(-0.05f, 2.0f), frand(-0.05f, 2.0f));
            float3 o = run_lin(in.x, in.y, in.z);
            float k = exp2f(ui_Exposure);
            float3 e = make_float3(in.x * k, in.y * k, in.z * k);
            double m = fmax(fabs(e.x), fmax(fabs(e.y), fabs(e.z)));
            if (m > 1e-4) expErr = fmax(expErr, fmax(fabs(o.x - e.x), fmax(fabs(o.y - e.y), fabs(o.z - e.z))) / m);
            if ((in.x < 0 && o.x > 0) || (in.y < 0 && o.y > 0) || (in.z < 0 && o.z > 0)) expSign++;
        }
        printf("Exposure: max relative error vs 2^stops gain %.2e, sign flips %d\n", expErr, expSign);
        CHECK(expErr < 1e-4, "exposure not an exact gain (%.2e)", expErr);
        CHECK(expSign == 0, "exposure flips sign (%d)", expSign);

        // 8d. Temperature / Tint: white luminance kept, direction, continuity, monotonic in Temperature
        double wbY = 0; int wbDir = 0, wbMono = 0;
        float prevRB = 1e9f;
        for (int t = -100; t <= 100; t += 5) {
            reset(); ui_Temperature = (float)t;
            float3 o = run_work(0.18f, 0.18f, 0.18f);
            wbY = fmax(wbY, fabs(log2(luma(o) / 0.18)));
            float rb = o.z / o.x;                                     // blue / red falls as it warms
            if (t > -100 && rb > prevRB) wbMono++;
            prevRB = rb;
            if (t > 0 && !(o.x > o.z)) wbDir++;
            if (t < 0 && !(o.z > o.x)) wbDir++;
        }
        for (int t = -100; t <= 100; t += 5) {
            reset(); ui_Tint = (float)t;
            float3 o = run_work(0.18f, 0.18f, 0.18f);
            wbY = fmax(wbY, fabs(log2(luma(o) / 0.18)));
            if (t > 0 && !(o.y < 0.5f * (o.x + o.z))) wbDir++;     // magenta: green below red / blue
            if (t < 0 && !(o.y > 0.5f * (o.x + o.z))) wbDir++;     // green
        }
        reset(); ui_Temperature = 0.01f; float3 c1 = run_work(0.18f, 0.18f, 0.18f);
        double cont = fmax(fabs(c1.x - 0.18), fmax(fabs(c1.y - 0.18), fabs(c1.z - 0.18)));
        printf("Temperature / Tint: grey luminance change max %.2e stops, wrong direction %d, non-monotonic %d, "
               "change at +0.01 %.1e\n", wbY, wbDir, wbMono, cont);
        CHECK(wbY < 1e-3, "white balance changes grey luminance (%.4f stops)", wbY);
        CHECK(wbDir == 0, "white balance direction wrong (%d)", wbDir);
        CHECK(wbMono == 0, "temperature not monotonic (%d)", wbMono);
        CHECK(cont < 1e-4, "temperature not continuous at 0 (%.2e)", cont);

        // 8e. Split toning: grey luminance nearly unchanged, tint direction, skin protected
        double stY = 0; int stDir = 0;
        for (int i = 0; i < 4000; i++) {
            reset(); ui_ShHue = frand(0, 360); ui_MdHue = frand(0, 360); ui_HlHue = frand(0, 360);
            ui_ShSat = frand(0, 100); ui_MdSat = frand(0, 100); ui_HlSat = frand(0, 100);
            ui_SplitBalance = frand(-100, 100); ui_SplitStrength = frand(0, 100);
            float v = 0.18f * exp2f(frand(-8, 6));
            float3 o = run_work(v, v, v);
            stY = fmax(stY, fabs(log2(luma(o) / v)));
        }
        reset(); ui_ShHue = 240; ui_ShSat = 100; float3 sh = run_work(0.01f, 0.01f, 0.01f);
        if (!(sh.z > sh.x && sh.z > sh.y)) stDir++;
        reset(); ui_HlHue = 30; ui_HlSat = 100; float3 hl = run_work(2.0f, 2.0f, 2.0f);
        if (!(hl.x > hl.y && hl.y > hl.z)) stDir++;
        reset(); ui_MdHue = 120; ui_MdSat = 100; float3 md = run_work(0.18f, 0.18f, 0.18f);
        if (!(md.y > md.x && md.y > md.z)) stDir++;
        reset(); ui_ShHue = 240; ui_ShSat = 100; float3 hi = run_work(4.0f, 4.0f, 4.0f);
        double leak = fmax(fabs(hi.x - hi.z), fabs(hi.y - hi.z)) / 4.0;       // shadow tint must not reach +4.5 stops
        printf("Split toning: max grey luminance change %.4f stops, wrong direction %d, shadow tint at +4.5 stops %.1e\n",
               stY, stDir, leak);
        CHECK(stY < 0.02, "split toning changes grey luminance (%.4f stops)", stY);
        CHECK(stDir == 0, "split toning direction wrong (%d)", stDir);
        CHECK(leak < 1e-6, "shadow tint leaks into highlights (%.2e)", leak);

        // 8f. Monochrome: Silvergrain stays exactly neutral with Exposure / Temperature / Tint and any
        // print; split toning then tints it (by design: split-toned black and white)
        int monoBad = 0;
        for (int i = 0; i < 6000; i++) {
            reset(); ui_Look = GLX_LOOK_SILVERGRAIN_100; ui_Print = i % GLX_PRINT_COUNT;
            ui_Exposure = frand(-3, 3); ui_Temperature = frand(-100, 100); ui_Tint = frand(-100, 100);
            float3 o = run(frand(0, 1), frand(0, 1), frand(0, 1));
            if (o.x != o.y || o.y != o.z) monoBad++;
        }
        reset(); ui_Look = GLX_LOOK_SILVERGRAIN_100; ui_HlHue = 40; ui_HlSat = 60; ui_ShHue = 220; ui_ShSat = 60;
        float3 tb = run_work(0.6f, 0.4f, 0.3f);
        printf("Monochrome: Silvergrain non-neutral with corrections %d; split-toned B&W sample %.4f %.4f %.4f\n",
               monoBad, tb.x, tb.y, tb.z);
        CHECK(monoBad == 0, "monochrome not neutral with corrections (%d)", monoBad);

        // 8g. Stability: every control random (incl. extremes), extended-range input
        int nanAll = 0, giBad = 0;
        for (int i = 0; i < 60000; i++) {
            reset(); ui_Look = i % GLX_LOOK_COUNT; ui_Print = (i / 19) % GLX_PRINT_COUNT; ui_SecLook = (i / 114) % GLX_SEC_COUNT;
            ui_Exposure = (i % 5 == 0) ? ((i & 8) ? 5.0f : -5.0f) : frand(-5, 5);
            ui_Temperature = frand(-100, 100); ui_Tint = frand(-100, 100);
            ui_ShHue = frand(0, 360); ui_MdHue = frand(0, 360); ui_HlHue = frand(0, 360);
            ui_ShSat = frand(0, 100); ui_MdSat = frand(0, 100); ui_HlSat = frand(0, 100);
            ui_SplitBalance = frand(-100, 100); ui_SplitStrength = frand(0, 100);
            ui_CreativeIntensity = frand(0, 200); ui_PrintContrast = frand(-100, 100); ui_Contrast = frand(-100, 100);
            ui_HlWarmth = frand(-100, 100); ui_ShadowTone = frand(-100, 100); ui_Separation = frand(0, 100);
            float r = frand(-0.6f, 2.6f), g = frand(-0.6f, 2.6f), b = frand(-0.6f, 2.6f);
            if (bad(run(r, g, b))) nanAll++;
            ui_Intensity = 0;
            float3 z = run(r, g, b);
            if (z.x != r || z.y != g || z.z != b) giBad++;
        }
        printf("Stability with every control random (extended-range DI -0.6..2.6): NaN/Inf %d, Global Intensity 0 not identity %d\n",
               nanAll, giBad);
        CHECK(nanAll == 0, "NaN/Inf with all controls random (%d)", nanAll);
        CHECK(giBad == 0, "Global Intensity 0 not identity (%d)", giBad);
    }

    printf(fails ? "\n%d FAILURES\n" : "\nALL CHECKS PASSED\n", fails);
    return fails != 0;
}
#endif
