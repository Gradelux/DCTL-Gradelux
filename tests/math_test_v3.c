// Host-side math test for Gradelux FilmLab Version 3.
// Compiles the .dctl as C through a small shim (checks the maths, not
// Resolve's GPU compiler).
//
//   cd tests && gcc -O2 -o math_test_v3 math_test_v3.c -lm && ./math_test_v3
//
// Regression dumps (compare with: cmp a.txt b.txt):
//   -DDUMP_V1 / -DDUMP_V3_CORE : V1 controls only (V3 with Film Look Off must equal V1)
//   -DDUMP_V2_ADJ / -DDUMP_V3_ADJ : V2 adjustments without a stock vs V3 Off
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

enum { GLX_LOOK_OFF, GLX_LOOK_PURE, GLX_LOOK_EMBER, GLX_LOOK_SLATE, GLX_LOOK_SOLACE, GLX_LOOK_CHROME,
       GLX_LOOK_DUNE, GLX_LOOK_NOIR, GLX_LOOK_BLOOM, GLX_LOOK_HAZE, GLX_LOOK_VIVID, GLX_LOOK_MUSE,
       GLX_LOOK_TUNDRA, GLX_LOOK_SIENNA, GLX_LOOK_VELVET, GLX_LOOK_CINDER, GLX_LOOK_LUMEN, GLX_LOOK_FABLE,
       GLX_LOOK_MOSS, GLX_LOOK_PRISM, GLX_LOOK_ECLIPSE, GLX_LOOK_MONO, GLX_LOOK_OBSIDIAN, GLX_LOOK_SILVER,
       GLX_LOOK_COUNT };
enum { GLX_STOCK_NONE };   // only used by the V2 dump build
static const char *LOOK_NAMES[] = { "Off", "Pure", "Ember", "Slate", "Solace", "Chrome", "Dune", "Noir",
    "Bloom", "Haze", "Vivid", "Muse", "Tundra", "Sienna", "Velvet", "Cinder", "Lumen", "Fable", "Moss",
    "Prism", "Eclipse", "Mono", "Obsidian", "Silver" };

static int   ui_Look, ui_Stock;
static float ui_CreativeIntensity = 100, ui_StockStrength = 100;
static float ui_ColorDensity, ui_Separation = 50, ui_HlWarmth, ui_ShadowTone;
static float ui_Density, ui_SubSat, ui_Contrast, ui_Shadow, ui_Rolloff, ui_Intensity = 100;

#if defined(DUMP_V1)
#include "../Gradelux_FilmLab.dctl"
#elif defined(DUMP_V2_ADJ)
#define GLX_STOCK_V500T 1
#define GLX_STOCK_V250D 2
#define GLX_STOCK_V50D 3
#define GLX_STOCK_2383 4
#define GLX_STOCK_PORTRA400 5
#define GLX_STOCK_GOLD200 6
#define GLX_STOCK_ETERNA 7
#define GLX_STOCK_400H 8
#define GLX_STOCK_CS800T 9
#define GLX_STOCK_MODERN 10
#include "../Gradelux_FilmLab_v2.dctl"
#else
#include "../Gradelux_FilmLab_v3.dctl"
#endif

static void reset(void) {
    ui_Look = 0; ui_Stock = 0; ui_CreativeIntensity = 100; ui_StockStrength = 100;
    ui_ColorDensity = 0; ui_Separation = 50; ui_HlWarmth = 0; ui_ShadowTone = 0;
    ui_Density = ui_SubSat = ui_Contrast = ui_Shadow = ui_Rolloff = 0; ui_Intensity = 100;
}
static float frand(float a, float b) { return a + (b - a) * (float)rand() / (float)RAND_MAX; }
static float3 run(float r, float g, float b) { return transform(0, 0, 0, 0, r, g, b); }

#if defined(DUMP_V1) || defined(DUMP_V3_CORE)
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
#elif defined(DUMP_V2_ADJ) || defined(DUMP_V3_ADJ)
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
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
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

int main(void)
{
    srand(4321);
    // 1. Identity: defaults, 0% Global Intensity, 0% Creative Intensity
    for (int i = 0; i < 50000; i++) {
        float r = frand(-0.5f, 1.2f), g = frand(-0.5f, 1.2f), b = frand(-0.5f, 1.2f);
        reset(); ui_ColorDensity = frand(-100, 100); ui_CreativeIntensity = frand(0, 200);
        float3 o = run(r, g, b);
        CHECK(o.x == r && o.y == g && o.z == b, "identity at defaults (Off)");
        reset(); ui_Look = 1 + i % 23; ui_HlWarmth = 40; ui_Density = 30; ui_Intensity = 0;
        o = run(r, g, b);
        CHECK(o.x == r && o.y == g && o.z == b, "identity at 0%% Global Intensity");
        reset(); ui_Look = 1 + i % 23; ui_CreativeIntensity = 0; ui_ColorDensity = 70;
        o = run(r, g, b);
        CHECK(o.x == r && o.y == g && o.z == b, "Creative Intensity 0%% bypass");
        if (fails) break;
    }

    // 2. Creative Intensity 0% keeps the independent controls; the look fades
    //    continuously to zero (tested at 0.001%)
    //    (the look's effect fades linearly to zero, even for extreme inputs)
    for (int i = 0; i < 20000; i++) {
        float r = frand(-0.2f, 1.1f), g = frand(-0.2f, 1.1f), b = frand(-0.2f, 1.1f);
        reset(); ui_Contrast = 40; ui_SubSat = 30; ui_HlWarmth = 25; ui_Separation = 70; ui_ShadowTone = -30;
        float3 a = run(r, g, b);
        ui_Look = 1 + i % 23; ui_CreativeIntensity = 0;
        float3 c = run(r, g, b);
        CHECK(a.x == c.x && a.y == c.y && a.z == c.z, "CI 0%% with other controls (look %d)", ui_Look);
        reset(); ui_Look = 1 + i % 23; ui_CreativeIntensity = 0.001f;
        c = run(r, g, b);
        {   // compare in linear light, relative to the pixel's magnitude
            float lr = glx_di_to_lin(r), lg = glx_di_to_lin(g), lb = glx_di_to_lin(b);
            float mag = fmaxf(fmaxf(fabsf(lr), fabsf(lg)), fmaxf(fabsf(lb), 0.01f));
            float d = fmaxf(fmaxf(fabsf(glx_di_to_lin(c.x) - lr), fabsf(glx_di_to_lin(c.y) - lg)), fabsf(glx_di_to_lin(c.z) - lb));
            CHECK(d / mag < 0.01f, "CI 0.001%% not continuous (look %d: rel change %.4f)", ui_Look, d / mag);
        }
        if (fails) break;
    }

    // 3. Per look, at 100% and 200%
    static float out[GLX_LOOK_COUNT][NCHART][3];
    printf("\n%-9s %6s %8s %21s %21s %8s %7s %8s\n", "Look", "slope", "grey", "shadow -4 st (R G B)", "hilite +3 st (R G B)",
           "skin hue", "sat x", "strength");
    for (int id = 0; id < GLX_LOOK_COUNT; id++) {
        for (int pass = 0; pass < 2; pass++) {
            reset(); ui_Look = id; ui_CreativeIntensity = pass ? 200.0f : 100.0f;
            const char *tag = pass ? "200%" : "100%";

            float3 bk = run(0, 0, 0);
            CHECK(!bad(bk) && fabsf(bk.x) < 0.2f, "%s %s black", LOOK_NAMES[id], tag);

            // Neutral ramp: monotonic and continuous in every channel
            float3 prev = run(-0.05f, -0.05f, -0.05f);
            int nonmono = 0, jumps = 0;
            for (int i = 1; i <= 50000; i++) {
                float v = -0.05f + 1.15f * i / 50000.0f;
                float3 o = run(v, v, v);
                if (o.x < prev.x || o.y < prev.y || o.z < prev.z) nonmono++;
                if (fabsf(o.x - prev.x) > 4e-4f || fabsf(o.y - prev.y) > 4e-4f || fabsf(o.z - prev.z) > 4e-4f) jumps++;
                prev = o;
            }
            CHECK(nonmono == 0, "%s %s ramp not monotonic (%d)", LOOK_NAMES[id], tag, nonmono);
            CHECK(jumps == 0, "%s %s ramp discontinuity (%d)", LOOK_NAMES[id], tag, jumps);

            // Random signed / extended inputs: finite; mono exactly neutral;
            // in-gamut colour inputs never go negative
            int nan = 0, neg = 0, tint = 0;
            for (int i = 0; i < 100000; i++) {
                float3 o = run(frand(-1.0f, 1.3f), frand(-1.0f, 1.3f), frand(-1.0f, 1.3f));
                if (bad(o)) nan++;
                if (is_mono(id) && (o.x != o.y || o.y != o.z)) tint++;
                float3 w = run_work(frand(0, 8), frand(0, 8), frand(0, 8));
                if (w.x < -1e-4f || w.y < -1e-4f || w.z < -1e-4f) neg++;
            }
            CHECK(nan == 0, "%s %s NaN/Inf (%d)", LOOK_NAMES[id], tag, nan);
            CHECK(tint == 0, "%s %s monochrome not exactly neutral (%d)", LOOK_NAMES[id], tag, tint);
            CHECK(neg == 0, "%s %s negatives from in-gamut colours (%d)", LOOK_NAMES[id], tag, neg);

            // (a) The look's hue mapping (colour stage) never reorders hues,
            //     including with Film Color Separation at its extremes.
            // (b) The full pipeline is continuous around the hue circle (no
            //     jumps). Strong look tints may legitimately pull muted colours
            //     toward the look's cast, so (b) checks smoothness, not order.
            if (!is_mono(id)) {
                GlxProfile pp = glx_profile_base();
                GlxLook LL = glx_look_scale(glx_look_get(id), pass ? 2.0f : 1.0f);
                for (int sep = 0; sep <= 100; sep += 50) {
                    float sepF = (sep - 50) * 0.02f;
                    double prevH = -1e9, unwrap = 0; int folds = 0, jumpsC = 0;
                    float3 prevO = make_float3(0, 0, 0);
                    for (int i = 0; i <= 3600; i++) {
                        double th = (-180.0 + i * 0.1) * 0.0174532925;
                        float a = 0.10f * cos(th), bb = 0.10f * sin(th), m = 0.25f;
                        float3 c = make_float3(m + 2 * 0.40824829f * a, m - 0.40824829f * a + 0.70710678f * bb,
                                               m - 0.40824829f * a - 0.70710678f * bb);
                        float3 h1 = glx_film_color(c.x, c.y, c.z, LL, 0.0f, pp);
                        float3 h2 = glx_film_color(h1.x, h1.y, h1.z, glx_look_clean(), sepF, pp);
                        double h = hue_deg(h2) + unwrap;
                        if (prevH > -1e8 && h < prevH - 180) { unwrap += 360; h += 360; }
                        if (prevH > -1e8 && h < prevH - 1e-6) folds++;
                        prevH = h;
                        ui_Separation = sep;
                        float3 o = run_work(c.x, c.y, c.z);
                        if (i > 0 && (fabsf(o.x - prevO.x) > 2e-3f || fabsf(o.y - prevO.y) > 2e-3f || fabsf(o.z - prevO.z) > 2e-3f)) jumpsC++;
                        prevO = o;
                    }
                    ui_Separation = 50;
                    CHECK(folds == 0, "%s %s separation %d: hue map folds (%d)", LOOK_NAMES[id], tag, sep, folds);
                    CHECK(jumpsC == 0, "%s %s separation %d: output jumps around hue circle (%d)", LOOK_NAMES[id], tag, sep, jumpsC);
                }
            }
        }

        // Summary at 100%
        reset(); ui_Look = id;
        for (int c = 0; c < NCHART; c++) {
            float3 o = run_work(CHART[c][0], CHART[c][1], CHART[c][2]);
            out[id][c][0] = o.x; out[id][c][1] = o.y; out[id][c][2] = o.z;
        }
        float3 a = run_lin(0.18f * 1.0353f, 0.18f * 1.0353f, 0.18f * 1.0353f);
        float3 b = run_lin(0.18f / 1.0353f, 0.18f / 1.0353f, 0.18f / 1.0353f);
        double ya = WORK_YR * a.x + WORK_YG * a.y + WORK_YB * a.z, yb = WORK_YR * b.x + WORK_YG * b.y + WORK_YB * b.z;
        double slope = (log2(ya) - log2(yb)) / 0.1;
        float3 gq = run_work(0.18f, 0.18f, 0.18f);
        float3 sh = run_work(0.01125f, 0.01125f, 0.01125f);
        float3 hl = run_work(1.44f, 1.44f, 1.44f);
        double skH = 0, skS = 0;
        for (int c = 0; c < 5; c++) {
            float3 i0 = make_float3(CHART[c][0], CHART[c][1], CHART[c][2]);
            float3 o = make_float3(out[id][c][0], out[id][c][1], out[id][c][2]);
            double d = hue_deg(o) - hue_deg(i0);
            if (fabs(d) > fabs(skH)) skH = d;
            skS += satv(o) / satv(i0) / 5;
        }
        double strength = 0;
        for (int c = 0; c < NCHART; c++)
            for (int ch = 0; ch < 3; ch++) {
                double x = log2(fmax(out[id][c][ch], 1e-4)) - log2(fmax(CHART[c][ch], 1e-4));
                strength += x * x;
            }
        strength = sqrt(strength / (NCHART * 3));
        printf("%-9s %6.3f %8.4f   %+5.2f %+5.2f %+5.2f   %+5.2f %+5.2f %+5.2f %+7.2f° %6.2fx %7.3f\n",
               LOOK_NAMES[id], slope, gq.y, log2(sh.x / 0.01125), log2(sh.y / 0.01125), log2(sh.z / 0.01125),
               log2(hl.x / 1.44), log2(hl.y / 1.44), log2(hl.z / 1.44), skH, skS, strength);
        if (id > 0 && !is_mono(id)) {
            CHECK(fabs(skH) < 6.0, "%s skin hue shift %.2f deg", LOOK_NAMES[id], skH);
            CHECK(strength > 0.12, "%s too subtle (%.3f stops RMS)", LOOK_NAMES[id], strength);
        }
    }

    // 4. All looks distinct from each other
    double minDist = 1e9; int pa = 0, pb = 0;
    for (int a = 1; a < GLX_LOOK_COUNT; a++)
        for (int b = a + 1; b < GLX_LOOK_COUNT; b++) {
            double d = 0;
            for (int c = 0; c < NCHART; c++)
                for (int ch = 0; ch < 3; ch++) {
                    double x = log2(fmax(out[a][c][ch], 1e-4)) - log2(fmax(out[b][c][ch], 1e-4));
                    d += x * x;
                }
            d = sqrt(d / (NCHART * 3));
            if (d < minDist) { minDist = d; pa = a; pb = b; }
        }
    printf("\nClosest pair of looks: %s vs %s, RMS difference %.3f stops on the test chart\n",
           LOOK_NAMES[pa], LOOK_NAMES[pb], minDist);
    CHECK(minDist > 0.08, "looks not distinct enough");

    // 5. Monochrome looks: channel mix renders colours differently
    printf("\nMonochrome rendering (output level relative to Rec.2020 luminance, stops):\n%-9s", "");
    const char *pn[] = {"lt skin", "dk skin", "red", "yellow", "green", "cyan", "blue", "sky"};
    int pi[] = {0, 1, 5, 6, 7, 8, 9, 11};
    for (int j = 0; j < 8; j++) printf(" %8s", pn[j]);
    printf("\n");
    for (int id = GLX_LOOK_MONO; id < GLX_LOOK_COUNT; id++) {
        printf("%-9s", LOOK_NAMES[id]);
        for (int j = 0; j < 8; j++) {
            const float *c = CHART[pi[j]];
            double y = WORK_YR * c[0] + WORK_YG * c[1] + WORK_YB * c[2];
            printf(" %+8.2f", log2(out[id][pi[j]][1] / y));
        }
        printf("\n");
    }

    // 6. All controls stable at extremes with every look
    float sweep[][6] = {{-100, 0, 100, 100, 200, 100}, {100, 100, -100, -100, 200, 0}, {0, 50, 100, -100, 50, 100}};
    for (int q = 0; q < 3; q++)
        for (int id = 0; id < GLX_LOOK_COUNT; id++) {
            reset(); ui_Look = id; ui_ColorDensity = sweep[q][0]; ui_Separation = sweep[q][1];
            ui_HlWarmth = sweep[q][2]; ui_ShadowTone = sweep[q][3]; ui_CreativeIntensity = sweep[q][4];
            ui_Density = sweep[q][5]; ui_SubSat = 100; ui_Contrast = 100; ui_Shadow = -100; ui_Rolloff = 100;
            int nan = 0;
            for (int i = 0; i < 30000; i++) if (bad(run(frand(-1, 1.3f), frand(-1, 1.3f), frand(-1, 1.3f)))) nan++;
            CHECK(nan == 0, "extreme sweep %d look %s NaN (%d)", q, LOOK_NAMES[id], nan);
        }

    printf(fails ? "\n%d FAILURES\n" : "\nALL CHECKS PASSED\n", fails);
    return fails != 0;
}
#endif
