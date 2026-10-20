// Host-side math test for Gradelux FilmLab.
// Compiles the actual .dctl as C through a small shim (this checks the maths,
// not Resolve's GPU compiler).
// Build: cd tests && gcc -O2 -o math_test math_test.c -lm && ./math_test
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
static float _clampf(float x, float a, float b) { return fminf(fmaxf(x, a), b); }
#define DEFINE_UI_PARAMS(...)
static float ui_Density, ui_SubSat, ui_Contrast, ui_Shadow, ui_Rolloff, ui_Intensity = 100;

#include "../Gradelux_FilmLab.dctl"

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void reset(void) { ui_Density = ui_SubSat = ui_Contrast = ui_Shadow = ui_Rolloff = 0; ui_Intensity = 100; }
static float3 run(float r, float g, float b) { return transform(0, 0, 0, 0, r, g, b); }
static float3 run_lin(float r, float g, float b) {
    float3 o = run(glx_lin_to_di(r), glx_lin_to_di(g), glx_lin_to_di(b));
    return make_float3(glx_di_to_lin(o.x), glx_di_to_lin(o.y), glx_di_to_lin(o.z));
}
static float3 run_work(float r, float g, float b) {
    float3 d = glx_work_to_dwg(r, g, b);
    float3 o = run_lin(d.x, d.y, d.z);
    return glx_dwg_to_work(o.x, o.y, o.z);
}
static float frand(float a, float b) { return a + (b - a) * (float)rand() / (float)RAND_MAX; }
static int bad(float3 o) { return isnan(o.x) || isnan(o.y) || isnan(o.z) || isinf(o.x) || isinf(o.y) || isinf(o.z); }

int main(void)
{
    srand(1234);
    float grey = glx_lin_to_di(0.18f);
    printf("18%% grey in DaVinci Intermediate: %.6f\n", grey);

    // 1. DI round trip over the signed range
    double maxe = 0;
    for (int i = 0; i <= 100000; i++) {
        float v = -0.5f + 1.7f * i / 100000.0f;
        double e = fabs(glx_lin_to_di(glx_di_to_lin(v)) - v);
        if (e > maxe) maxe = e;
    }
    printf("DI round-trip max error: %.2e\n", maxe);
    CHECK(maxe < 5e-6, "DI round trip");

    // 2. Exact identity at defaults and at 0%% intensity
    reset();
    for (int i = 0; i < 100000; i++) {
        float r = frand(-0.5f, 1.2f), g = frand(-0.5f, 1.2f), b = frand(-0.5f, 1.2f);
        float3 o = run(r, g, b);
        CHECK(o.x == r && o.y == g && o.z == b, "identity at defaults");
        ui_Density = 80; ui_Contrast = 50; ui_Intensity = 0;
        o = run(r, g, b);
        CHECK(o.x == r && o.y == g && o.z == b, "identity at 0%% intensity");
        reset();
    }

    // Parameter presets used by the sweeps below
    float presets[][5] = {
        { 100,  100,  100,  100, 100}, {-100, -100, -100, -100, 100},
        { 100, -100,  100, -100,   0}, { -50,   50,  -50,   50,  50},
        {   0,    0,  100,    0,   0}, {   0,    0, -100,    0,   0},
        {   0,    0,    0,  100,   0}, {   0,    0,    0, -100,   0},
        {   0,    0,    0,    0, 100}, {  60,   40,   30,   20,  40},
    };
    int np = sizeof(presets) / sizeof(presets[0]);

    for (int q = 0; q < np; q++) {
        reset();
        ui_Density = presets[q][0]; ui_SubSat = presets[q][1]; ui_Contrast = presets[q][2];
        ui_Shadow = presets[q][3]; ui_Rolloff = presets[q][4];

        // 3. Neutrals stay neutral, grey stays put (unless only roll-off is on, which
        //    never touches grey anyway), ramp monotonic, black stays black
        float prev = -1e9f;
        int nonmono = 0, nonneutral = 0;
        for (int i = 0; i <= 40000; i++) {
            float v = -0.1f + 1.1f * i / 40000.0f;
            float3 o = run(v, v, v);
            if (fabsf(o.x - o.y) > 1e-6f || fabsf(o.y - o.z) > 1e-6f) nonneutral++;
            if (o.x < prev - 1e-7f) nonmono++;
            prev = o.x;
        }
        float3 gq = run(grey, grey, grey);
        float3 bk = run(0, 0, 0);
        CHECK(nonmono == 0, "preset %d monotonic (%d)", q, nonmono);
        CHECK(nonneutral == 0, "preset %d neutral (%d)", q, nonneutral);
        CHECK(fabsf(gq.x - grey) < 2e-6f, "preset %d grey %.6f", q, gq.x);
        CHECK(bk.x == 0 && bk.y == 0 && bk.z == 0, "preset %d black", q);

        // 4. Random signed / extended inputs: no NaN/Inf
        for (int i = 0; i < 200000; i++) {
            float3 o = run(frand(-1.0f, 1.3f), frand(-1.0f, 1.3f), frand(-1.0f, 1.3f));
            CHECK(!bad(o), "preset %d NaN/Inf", q);
            if (bad(o)) break;
        }
    }

    // 5. Tone stage preserves hue: channel ratios unchanged
    reset(); ui_Contrast = 100; ui_Shadow = 100; ui_Rolloff = 100;
    double maxratio = 0;
    for (int i = 0; i < 100000; i++) {
        float r = frand(0.001f, 20), g = frand(0.001f, 20), b = frand(0.001f, 20);
        float3 o = run_lin(r, g, b);
        double e = fabs((o.y / o.x) / (g / r) - 1) + fabs((o.z / o.x) / (b / r) - 1);
        if (e > maxratio) maxratio = e;
    }
    printf("Tone stage max channel-ratio drift: %.2e\n", maxratio);
    CHECK(maxratio < 1e-3, "tone hue preservation");

    // 6. Subtractive saturation (in the Rec.2020 working space): no channel
    //    changes sign and channel order is preserved (no hue flips)
    reset(); ui_SubSat = 100;
    int created_neg = 0, order = 0;
    for (int i = 0; i < 100000; i++) {
        float r = frand(0, 10), g = frand(0, 10), b = frand(0, 10);
        float3 o = run_work(r, g, b);
        if (o.x < -1e-5f || o.y < -1e-5f || o.z < -1e-5f) created_neg++;
        if ((r > g) != (o.x > o.y) && fabsf(r - g) > 1e-3f) order++;
    }
    CHECK(created_neg == 0, "subsat created negatives (%d)", created_neg);
    CHECK(order == 0, "subsat changed channel order (%d)", order);

    // 7. Film Density: skin vs pure colours (linear DWG samples)
    reset(); ui_Density = 100;
    struct { const char *n; float r, g, b; } s[] = {
        {"light skin", 0.4277f, 0.3091f, 0.2594f}, {"deep skin", 0.1813f, 0.0918f, 0.0478f},
        {"709 red",    0.5628f, 0.0778f, 0.0647f}, {"709 green", 0.3235f, 0.7496f, 0.1920f},
        {"709 blue",   0.1137f, 0.1727f, 0.7433f}, {"grey",      0.18f,   0.18f,   0.18f},
    };
    for (int i = 0; i < 6; i++) {
        float3 o = run_lin(s[i].r, s[i].g, s[i].b);
        printf("Density 100  %-10s  change %+.3f stops\n", s[i].n, log2f(o.x / s[i].r));
    }
    {
        float3 sk = run_lin(0.1813f, 0.0918f, 0.0478f), rd = run_lin(0.5628f, 0.0778f, 0.0647f);
        CHECK(log2f(sk.x / 0.1813f) > 0.5f * log2f(rd.x / 0.5628f), "skin protected vs red");
    }

    // 8. Subtractive saturation: saturation change on a few colours
    reset(); ui_SubSat = 100;
    for (int i = 0; i < 5; i++) {
        float3 o = run_lin(s[i].r, s[i].g, s[i].b);
        float3 wi = glx_dwg_to_work(s[i].r, s[i].g, s[i].b), wo = glx_dwg_to_work(o.x, o.y, o.z);
        float si = (fmaxf(s[i].r, fmaxf(s[i].g, s[i].b)) - fminf(s[i].r, fminf(s[i].g, s[i].b))) / fmaxf(s[i].r, fmaxf(s[i].g, s[i].b));
        float so = (fmaxf(o.x, fmaxf(o.y, o.z)) - fminf(o.x, fminf(o.y, o.z))) / fmaxf(o.x, fmaxf(o.y, o.z));
        printf("SubSat 100   %-10s  saturation %.3f -> %.3f, luma %+.3f stops\n", s[i].n, si, so,
               log2f(glx_luma(wo.x, wo.y, wo.z) / glx_luma(wi.x, wi.y, wi.z)));
    }

    // 8b. Hue rotation from Subtractive Saturation (opponent-plane angle, colours
    //     with saturation > 0.1 inside Rec.2020)
    reset(); ui_SubSat = 100;
    {
        double worst = 0, sum = 0; int cnt = 0;
        for (int i = 0; i < 200000; i++) {
            float r = frand(0.01f, 2), g = frand(0.01f, 2), b = frand(0.01f, 2);
            float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b));
            if ((mx - mn) / mx < 0.1f) continue;
            float3 o = run_work(r, g, b);
            double h0 = atan2((g - b) * 0.70710678, (2 * r - g - b) * 0.40824829);
            double h1 = atan2((o.y - o.z) * 0.70710678, (2 * o.x - o.y - o.z) * 0.40824829);
            double d = fabs(h1 - h0) * 57.29578; if (d > 180) d = 360 - d;
            if (d > worst) worst = d;
            sum += d; cnt++;
        }
        printf("SubSat 100 hue rotation: mean %.2f deg, max %.2f deg\n", sum / cnt, worst);
        CHECK(worst < 10.0, "subsat hue rotation");
    }

    // 9. Curves at key exposures
    float stops[] = {-6, -4, -2, -1, 0, 1, 2, 3, 4, 6, 8};
    const char *names[] = {"Contrast +100", "Contrast -100", "Shadow +100", "Shadow -100", "Roll-Off 100"};
    float cfg[][3] = {{100, 0, 0}, {-100, 0, 0}, {0, 100, 0}, {0, -100, 0}, {0, 0, 100}};
    for (int c = 0; c < 5; c++) {
        reset(); ui_Contrast = cfg[c][0]; ui_Shadow = cfg[c][1]; ui_Rolloff = cfg[c][2];
        printf("%-14s", names[c]);
        for (int i = 0; i < 11; i++) {
            float x = 0.18f * exp2f(stops[i]);
            float3 o = run_lin(x, x, x);
            printf(" %+g:%+.2f", stops[i], log2f(o.x / 0.18f) - stops[i]);
        }
        printf("  (stops in: change)\n");
    }

    // 10. Global Intensity: 50%% is the midpoint in DI code values
    reset(); ui_Contrast = 100; ui_Density = 100;
    {
        float r = 0.55f, g = 0.42f, b = 0.30f;
        float3 full = run(r, g, b);
        ui_Intensity = 50;
        float3 half = run(r, g, b);
        CHECK(fabsf(half.x - 0.5f * (r + full.x)) < 1e-6f, "intensity blend");
    }

    printf(fails ? "\n%d FAILURES\n" : "\nALL CHECKS PASSED\n", fails);
    return fails != 0;
}
