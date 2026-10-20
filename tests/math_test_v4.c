// Host-side math test for Gradelux FilmLab Version 4.
// Compiles the .dctl as C through a small shim (checks the maths, not
// Resolve's GPU compiler).
//
//   cd tests && gcc -O2 -o math_test_v4 math_test_v4.c -lm && ./math_test_v4
//
// Regression dumps (compare with cmp, or numerically):
//   -DDUMP_V1  / -DDUMP_V4_CORE : V1 controls only (V4 Look Off + Print None must equal V1)
//   -DDUMP_V3_ADJ / -DDUMP_V4_ADJ : film adjustments without a look (V4 must match V3)
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
#define DEFINE_UI_PARAMS(...)

enum { GLX_LOOK_OFF, GLX_LOOK_PURE, GLX_LOOK_EMBER, GLX_LOOK_SLATE, GLX_LOOK_SOLACE, GLX_LOOK_CHROME,
       GLX_LOOK_DUNE, GLX_LOOK_NOIR, GLX_LOOK_BLOOM, GLX_LOOK_HAZE, GLX_LOOK_VIVID, GLX_LOOK_MUSE,
       GLX_LOOK_TUNDRA, GLX_LOOK_SIENNA, GLX_LOOK_VELVET, GLX_LOOK_CINDER, GLX_LOOK_LUMEN, GLX_LOOK_FABLE,
       GLX_LOOK_MOSS, GLX_LOOK_PRISM, GLX_LOOK_ECLIPSE, GLX_LOOK_MONO, GLX_LOOK_OBSIDIAN, GLX_LOOK_SILVER,
       GLX_LOOK_COUNT };
enum { GLX_PRINT_NONE, GLX_PRINT_2383, GLX_PRINT_2393, GLX_PRINT_3513, GLX_PRINT_3510, GLX_PRINT_2383_SOFT,
       GLX_PRINT_COUNT };
static const char *LOOK_NAMES[] = { "Off", "Pure", "Ember", "Slate", "Solace", "Chrome", "Dune", "Noir",
    "Bloom", "Haze", "Vivid", "Muse", "Tundra", "Sienna", "Velvet", "Cinder", "Lumen", "Fable", "Moss",
    "Prism", "Eclipse", "Mono", "Obsidian", "Silver" };
static const char *PRINT_NAMES[] = { "None", "2383", "2393", "3513", "3510", "2383 Soft" };

static int   ui_Look, ui_Print;
static float ui_CreativeIntensity = 100, ui_PrintStrength = 100, ui_PrintContrast, ui_PrintDensity,
             ui_PrintSaturation, ui_PrintRolloff;
static float ui_ColorDensity, ui_Separation = 50, ui_HlWarmth, ui_ShadowTone;
static float ui_Density, ui_SubSat, ui_Contrast, ui_Shadow, ui_Rolloff, ui_Intensity = 100;

#if defined(DUMP_V1)
#include "../Gradelux_FilmLab.dctl"
#elif defined(DUMP_V3_ADJ)
#include "../Gradelux_FilmLab_v3.dctl"
#else
#include "../Gradelux_FilmLab_v4.dctl"
#endif

static void reset(void) {
    ui_Look = 0; ui_Print = 0; ui_CreativeIntensity = 100; ui_PrintStrength = 100;
    ui_PrintContrast = ui_PrintDensity = ui_PrintSaturation = ui_PrintRolloff = 0;
    ui_ColorDensity = 0; ui_Separation = 50; ui_HlWarmth = 0; ui_ShadowTone = 0;
    ui_Density = ui_SubSat = ui_Contrast = ui_Shadow = ui_Rolloff = 0; ui_Intensity = 100;
}
static float frand(float a, float b) { return a + (b - a) * (float)rand() / (float)RAND_MAX; }
static float3 run(float r, float g, float b) { return transform(0, 0, 0, 0, r, g, b); }

#if defined(DUMP_V1) || defined(DUMP_V4_CORE)
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
static int is_mono(int id) { return id >= GLX_LOOK_MONO; }

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
        float3 h1 = glx_film_color(c.x, c.y, c.z, L, 0.0f, pp);
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
        reset(); ui_Look = 1 + i % 23; ui_Print = 1 + i % 5; ui_HlWarmth = 40; ui_Density = 30; ui_Intensity = 0;
        o = run(r, g, b);
        CHECK(o.x == r && o.y == g && o.z == b, "identity at 0%% Global Intensity");
        reset(); ui_Look = 1 + i % 23; ui_CreativeIntensity = 0; ui_ColorDensity = 70;
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
        reset(); ui_Look = 1 + i % 23; ui_Contrast = 30; ui_HlWarmth = 20;
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
        reset(); ui_Look = 1 + i % 23; ui_CreativeIntensity = 0.001f;
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
                    if (o.x < prev.x - 1e-6f || o.y < prev.y - 1e-6f || o.z < prev.z - 1e-6f) nonmono++;   // 1e-6: float rounding on flat shoulders
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
               (id >= GLX_LOOK_MONO && id < GLX_LOOK_COUNT) ? 0.0 : skH, skS, log2(w.x / w.z));
        if (id > 0 && id < GLX_LOOK_MONO) CHECK(fabs(skH) < 6.0, "%s skin hue shift %.2f", nm, skH);
        if (id > 0 && id < GLX_LOOK_COUNT) CHECK(slope > 1.03, "%s too flat (slope %.3f)", nm, slope);
        if (id >= GLX_LOOK_COUNT) CHECK(fabs(log2(w.x / w.z)) < 1e-4, "print %s tints white", nm);
    }

    // 5b. Skin across every colour look x print combination
    {
        double worst = 0, sum = 0; int n = 0;
        for (int l = 1; l < GLX_LOOK_MONO; l++)
            for (int pr = 1; pr < GLX_PRINT_COUNT; pr++) {
                reset(); ui_Look = l; ui_Print = pr; double m = 0;
                for (int c = 0; c < 5; c++) {
                    float3 i0 = make_float3(CHART[c][0], CHART[c][1], CHART[c][2]);
                    float3 o = run_work(i0.x, i0.y, i0.z);
                    double d = hue_deg(o) - hue_deg(i0);
                    if (fabs(d) > fabs(m)) m = d;
                }
                sum += fabs(m); n++; worst = fmax(worst, fabs(m));
            }
        printf("\nSkin hue over all colour look x print combinations: mean %.2f deg, worst %.2f deg\n", sum / n, worst);
        CHECK(worst < 7.0, "look+print skin hue shift %.2f deg", worst);
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
    for (int id = 1; id < GLX_LOOK_MONO; id++)
        for (int a = 1; a < GLX_PRINT_COUNT; a++)
            for (int b = a + 1; b < GLX_PRINT_COUNT; b++)
                minC = fmin(minC, chart_dist(comboChart[id][a], comboChart[id][b]));
    printf("\nClosest looks: %s vs %s (%.3f stops RMS); closest prints: %s vs %s (%.3f); "
           "closest prints over the same colour look: %.3f\n",
           LOOK_NAMES[la], LOOK_NAMES[lb], minL, PRINT_NAMES[pa], PRINT_NAMES[pb], minP, minC);
    CHECK(minL > 0.08, "looks not distinct");
    CHECK(minP > 0.08, "prints not distinct");
    CHECK(minC > 0.04, "prints indistinguishable over some look");

    printf(fails ? "\n%d FAILURES\n" : "\nALL CHECKS PASSED\n", fails);
    return fails != 0;
}
#endif
