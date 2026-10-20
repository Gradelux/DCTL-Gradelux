// Host-side math test for Gradelux FilmLab Version 2.
// Compiles the .dctl as C through a small shim (checks the maths, not
// Resolve's GPU compiler).
//
// Build & run:
//   cd tests && gcc -O2 -o math_test_v2 math_test_v2.c -lm && ./math_test_v2
//
// V1 regression (stock None must match V1 bit-for-bit):
//   gcc -O2 -DDUMP_V1 -o dump_v1 math_test_v2.c -lm
//   gcc -O2 -DDUMP_V2 -o dump_v2 math_test_v2.c -lm
//   ./dump_v1 > v1.txt && ./dump_v2 > v2.txt && cmp v1.txt v2.txt
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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
enum { GLX_STOCK_NONE, GLX_STOCK_V500T, GLX_STOCK_V250D, GLX_STOCK_V50D, GLX_STOCK_2383,
       GLX_STOCK_PORTRA400, GLX_STOCK_GOLD200, GLX_STOCK_ETERNA, GLX_STOCK_400H,
       GLX_STOCK_CS800T, GLX_STOCK_MODERN, GLX_STOCK_COUNT };
static const char *STOCK_NAMES[] = { "None / Clean", "Vision3 500T", "Vision3 250D", "Vision3 50D",
    "Kodak 2383", "Portra 400", "Gold 200", "Eterna", "Fuji 400H", "CineStill 800T", "Modern Neg" };

static int   ui_Stock;
static float ui_StockStrength = 100, ui_ColorDensity, ui_Separation = 50, ui_HlWarmth, ui_ShadowTone;
static float ui_Density, ui_SubSat, ui_Contrast, ui_Shadow, ui_Rolloff, ui_Intensity = 100;

#ifdef DUMP_V1
#include "../Gradelux_FilmLab.dctl"
#else
#include "../Gradelux_FilmLab_v2.dctl"
#endif

static void reset(void) {
    ui_Stock = 0; ui_StockStrength = 100; ui_ColorDensity = 0; ui_Separation = 50; ui_HlWarmth = 0; ui_ShadowTone = 0;
    ui_Density = ui_SubSat = ui_Contrast = ui_Shadow = ui_Rolloff = 0; ui_Intensity = 100;
}
static float frand(float a, float b) { return a + (b - a) * (float)rand() / (float)RAND_MAX; }
static float3 run(float r, float g, float b) { return transform(0, 0, 0, 0, r, g, b); }

#if defined(DUMP_V1) || defined(DUMP_V2)
int main(void)
{
    srand(99);
    for (int i = 0; i < 20000; i++) {
        reset();
        ui_Density = frand(-100, 100); ui_SubSat = frand(-100, 100); ui_Contrast = frand(-100, 100);
        ui_Shadow = frand(-100, 100); ui_Rolloff = frand(0, 100); ui_Intensity = frand(0, 100);
        float3 o = run(frand(-0.3f, 1.2f), frand(-0.3f, 1.2f), frand(-0.3f, 1.2f));
        printf("%a %a %a\n", o.x, o.y, o.z);
    }
    return 0;
}
#else

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
static int bad(float3 o) { return !isfinite(o.x) || !isfinite(o.y) || !isfinite(o.z); }

// Linear DWG <-> working helpers for analysis
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

// Test colours in linear Rec.2020 (working space)
static const float CHART[][3] = {
    {0.4486f, 0.3203f, 0.2356f}, {0.1379f, 0.0901f, 0.0620f}, {0.6149f, 0.4338f, 0.2834f},  // skin light/dark/tan
    {0.1978f, 0.1021f, 0.0282f}, {0.8990f, 0.7614f, 0.6228f},                               // skin deep/pale
    {0.40f, 0.05f, 0.03f}, {0.40f, 0.30f, 0.03f}, {0.08f, 0.30f, 0.05f},                    // red, yellow, green
    {0.04f, 0.25f, 0.30f}, {0.03f, 0.06f, 0.35f}, {0.30f, 0.05f, 0.30f},                    // cyan, blue, magenta
    {0.09f, 0.12f, 0.20f}, {0.20f, 0.18f, 0.10f}, {0.05f, 0.07f, 0.04f},                    // sky, sand, foliage dark
};
#define NCHART (int)(sizeof(CHART) / sizeof(CHART[0]))

int main(void)
{
    srand(4321);
    float grey = glx_lin_to_di(0.18f);

    // 1. Identity at defaults, at 0% intensity, and None at any strength
    reset();
    for (int i = 0; i < 50000; i++) {
        float r = frand(-0.5f, 1.2f), g = frand(-0.5f, 1.2f), b = frand(-0.5f, 1.2f);
        reset(); ui_StockStrength = frand(0, 100); ui_ColorDensity = frand(-100, 100);
        float3 o = run(r, g, b);
        CHECK(o.x == r && o.y == g && o.z == b, "identity at defaults / None");
        reset(); ui_Stock = 1 + i % 10; ui_HlWarmth = 40; ui_Density = 30; ui_Intensity = 0;
        o = run(r, g, b);
        CHECK(o.x == r && o.y == g && o.z == b, "identity at 0%% intensity");
        if (fails) break;
    }

    // 2. Strength 0% bypasses the stock exactly; V1 + adjustments still act
    for (int i = 0; i < 50000; i++) {
        float r = frand(-0.3f, 1.2f), g = frand(-0.3f, 1.2f), b = frand(-0.3f, 1.2f);
        reset(); ui_Contrast = 40; ui_SubSat = 30; ui_HlWarmth = 25; ui_Separation = 70; ui_ShadowTone = -30;
        float3 a = run(r, g, b);
        ui_Stock = 1 + i % 10; ui_StockStrength = 0; ui_ColorDensity = 80;
        float3 c = run(r, g, b);
        CHECK(a.x == c.x && a.y == c.y && a.z == c.z, "strength 0 bypass (stock %d)", ui_Stock);
        if (fails) break;
    }

    // 3. Per-stock: grey fixed, black fixed, monotonic & continuous ramps,
    //    no NaN, no new negatives, distinct output
    float chartOut[GLX_STOCK_COUNT][NCHART][3];
    printf("\n%-15s %6s %7s %22s %22s %9s %9s\n", "Stock", "slope", "grey", "shadow -4 st (R/G/B)", "hilite +3 st (R/G/B)", "skin hue", "skin sat");
    for (int st = 0; st < GLX_STOCK_COUNT; st++) {
        reset(); ui_Stock = st;
        float3 gq = run(grey, grey, grey);
        CHECK(fabsf(gq.x - grey) < 2e-6f && fabsf(gq.y - grey) < 2e-6f && fabsf(gq.z - grey) < 2e-6f,
              "%s grey moved (%.6f %.6f %.6f)", STOCK_NAMES[st], gq.x, gq.y, gq.z);
        float3 bk = run(0, 0, 0);
        CHECK(bk.x == 0 && bk.y == 0 && bk.z == 0, "%s black moved", STOCK_NAMES[st]);

        float3 prev = run(-0.05f, -0.05f, -0.05f);
        int nonmono = 0, jumps = 0;
        for (int i = 1; i <= 50000; i++) {
            float v = -0.05f + 1.15f * i / 50000.0f;
            float3 o = run(v, v, v);
            if (o.x < prev.x || o.y < prev.y || o.z < prev.z) nonmono++;
            if (fabsf(o.x - prev.x) > 3e-4f || fabsf(o.y - prev.y) > 3e-4f || fabsf(o.z - prev.z) > 3e-4f) jumps++;
            prev = o;
        }
        CHECK(nonmono == 0, "%s ramp not monotonic (%d)", STOCK_NAMES[st], nonmono);
        CHECK(jumps == 0, "%s ramp discontinuity (%d)", STOCK_NAMES[st], jumps);

        int nan = 0, neg = 0;
        for (int i = 0; i < 200000; i++) {
            float3 o = run(frand(-1.0f, 1.3f), frand(-1.0f, 1.3f), frand(-1.0f, 1.3f));
            if (bad(o)) nan++;
            float3 w = run_work(frand(0, 8), frand(0, 8), frand(0, 8));   // in-gamut colours
            if (w.x < -1e-4f || w.y < -1e-4f || w.z < -1e-4f) neg++;
        }
        CHECK(nan == 0, "%s NaN/Inf (%d)", STOCK_NAMES[st], nan);
        CHECK(neg == 0, "%s created negatives from in-gamut colours (%d)", STOCK_NAMES[st], neg);

        for (int c = 0; c < NCHART; c++) {
            float3 o = run_work(CHART[c][0], CHART[c][1], CHART[c][2]);
            chartOut[st][c][0] = o.x; chartOut[st][c][1] = o.y; chartOut[st][c][2] = o.z;
        }

        // Summary: slope at grey (stops/stop), neutral tints, skin behaviour
        float3 lo = run_lin(0.18f * 1.03526f, 0.18f * 1.03526f, 0.18f * 1.03526f);   // +0.05 stop
        float3 hi2 = run_lin(0.18f / 1.03526f, 0.18f / 1.03526f, 0.18f / 1.03526f);
        double slope = (log2(lo.x) - log2(hi2.x)) / 0.1;
        float3 sh = run_work(0.18f / 16, 0.18f / 16, 0.18f / 16);
        float3 hl = run_work(0.18f * 8, 0.18f * 8, 0.18f * 8);
        double skH = 0, skS = 0;
        for (int c = 0; c < 5; c++) {
            float3 i0 = make_float3(CHART[c][0], CHART[c][1], CHART[c][2]);
            float3 o = make_float3(chartOut[st][c][0], chartOut[st][c][1], chartOut[st][c][2]);
            double d = hue_deg(o) - hue_deg(i0);
            if (fabs(d) > fabs(skH)) skH = d;
            skS += satv(o) / satv(i0) / 5;
        }
        printf("%-15s %6.3f %7.4f   %6.3f %6.3f %6.3f   %6.2f %6.2f %6.2f %+8.2f° %8.2fx\n", STOCK_NAMES[st], slope,
               gq.x, log2(sh.x / (0.18 / 16)), log2(sh.y / (0.18 / 16)), log2(sh.z / (0.18 / 16)),
               log2(hl.x / 1.44), log2(hl.y / 1.44), log2(hl.z / 1.44), skH, skS);
        if (st > 0) CHECK(fabs(skH) < 4.0, "%s skin hue shift %.2f deg", STOCK_NAMES[st], skH);
    }

    // 4. Every dropdown option gives a distinct result
    double minDist = 1e9; int pa = 0, pb = 0;
    for (int a = 0; a < GLX_STOCK_COUNT; a++)
        for (int b = a + 1; b < GLX_STOCK_COUNT; b++) {
            double d = 0;
            for (int c = 0; c < NCHART; c++)
                for (int ch = 0; ch < 3; ch++) {
                    double x = log2(fmax(chartOut[a][c][ch], 1e-4)) - log2(fmax(chartOut[b][c][ch], 1e-4));
                    d += x * x;
                }
            d = sqrt(d / (NCHART * 3));
            if (d < minDist) { minDist = d; pa = a; pb = b; }
        }
    printf("\nClosest pair of stocks: %s vs %s, RMS difference %.3f stops on the test chart\n",
           STOCK_NAMES[pa], STOCK_NAMES[pb], minDist);
    CHECK(minDist > 0.03, "stocks not distinct enough");

    // 5. Film adjustments: hue order preserved by Separation; NaN-free sweeps
    for (int sep = 0; sep <= 100; sep += 25) {
        reset(); ui_Separation = sep; ui_Stock = GLX_STOCK_2383;
        double prevH = -1e9, unwrap = 0; int folds = 0;
        for (int i = 0; i <= 3600; i++) {
            double th = (-180.0 + i * 0.1) * 0.0174532925;
            float a = 0.10f * cos(th), bb = 0.10f * sin(th), m = 0.25f;
            float3 c = make_float3(m + 2 * 0.40824829f * a, m - 0.40824829f * a + 0.70710678f * bb, m - 0.40824829f * a - 0.70710678f * bb);
            float3 o = run_work(c.x, c.y, c.z);
            double h = hue_deg(o) + unwrap;
            if (prevH > -1e8 && h < prevH - 180) { unwrap += 360; h += 360; }
            if (prevH > -1e8 && h < prevH - 1e-6) folds++;
            prevH = h;
        }
        CHECK(folds == 0, "separation %d reorders hues (%d)", sep, folds);
    }
    float sweep[][5] = {{-100, 0, 100, 100, 100}, {100, 100, -100, -100, 0}, {0, 50, 100, -100, 100}};
    for (int q = 0; q < 3; q++)
        for (int st = 0; st < GLX_STOCK_COUNT; st++) {
            reset(); ui_Stock = st; ui_ColorDensity = sweep[q][0]; ui_Separation = sweep[q][1];
            ui_HlWarmth = sweep[q][2]; ui_ShadowTone = sweep[q][3]; ui_StockStrength = sweep[q][4];
            ui_Density = 100; ui_SubSat = 100; ui_Contrast = 100; ui_Shadow = -100; ui_Rolloff = 100;
            int nan = 0;
            for (int i = 0; i < 50000; i++) if (bad(run(frand(-1, 1.3f), frand(-1, 1.3f), frand(-1, 1.3f)))) nan++;
            CHECK(nan == 0, "extreme sweep %d stock %d NaN (%d)", q, st, nan);
        }

    // 6. Warmth / Shadow Tone leave grey alone and act in their zones
    reset(); ui_HlWarmth = 100; ui_ShadowTone = -100;
    {
        float3 gq = run(grey, grey, grey);
        CHECK(fabsf(gq.x - grey) < 2e-6f && fabsf(gq.z - grey) < 2e-6f, "warmth/shadow moved grey");
        float3 hl = run_work(1.44f, 1.44f, 1.44f), sh = run_work(0.0112f, 0.0112f, 0.0112f);
        printf("Warmth +100 at +3 stops: R/B = %.3f   Shadow Tone -100 at -4 stops: B/R = %.3f, G/R = %.3f\n",
               hl.x / hl.z, sh.z / sh.x, sh.y / sh.x);
        CHECK(hl.x > hl.z && sh.z > sh.x && sh.y > sh.x, "warmth / shadow tone direction");
    }

    printf(fails ? "\n%d FAILURES\n" : "\nALL CHECKS PASSED\n", fails);
    return fails != 0;
}
#endif
