// =============================================================================
//  Gradelux FilmLab Version 10: LUT look tests (host)
// -----------------------------------------------------------------------------
//  Checks the processing around the three original LUT looks (Vista, Lux,
//  Laguna): input domain, the optional Rec.709 round trip, Film Strength,
//  the order with Exposure / Cine16 / grading / Global Intensity, identity
//  rules, and finite output everywhere. Resolve's APPLY_LUT is emulated by the
//  shim from the same luts/*.cube files (trilinear and tetrahedral), so these
//  tests prove the DCTL's own code, not Resolve's interpolation.
//
//  Build (from the repository root):
//    python3 tools/v10_luts.py strip build/Gradelux_FilmLab_v10_host.dctl
//    sed 's/GLX_LUT_SPACE_\([A-Z]*\) *GLX_LUT_[A-Z0-9]*/GLX_LUT_SPACE_\1 GLX_LUT_DI/' \
//        build/Gradelux_FilmLab_v10_host.dctl > build/Gradelux_FilmLab_v10_host_di.dctl
//    sed 's/GLX_LUT_SPACE_\([A-Z]*\) *GLX_LUT_[A-Z0-9]*/GLX_LUT_SPACE_\1 GLX_LUT_REC709/' \
//        build/Gradelux_FilmLab_v10_host.dctl > build/Gradelux_FilmLab_v10_host_709.dctl
//    gcc -O2 -DSPACE_DI  -o build/t10_di  tests/math_test_v10.c -lm && build/t10_di
//    gcc -O2 -DSPACE_709 -o build/t10_709 tests/math_test_v10.c -lm && build/t10_709
//  (without -DSPACE_*: the DCTL's own setting, build/Gradelux_FilmLab_v10_host.dctl)
//  Optional: FILMLAB_LUT_DIR=<dir with identity Vista/Lux/Laguna.cube> and
//  the argument "identity" run the round-trip accuracy test instead.
// =============================================================================
#define FILMLAB_V10
#include "../tools/filmlab_shim.h"
#if defined(SPACE_709)
#include "../build/Gradelux_FilmLab_v10_host_709.dctl"
#elif defined(SPACE_DI)
#include "../build/Gradelux_FilmLab_v10_host_di.dctl"
#else
#include "../build/Gradelux_FilmLab_v10_host.dctl"
#endif
#define SPACE GLX_LUT_SPACE_VISTA
#if (GLX_LUT_SPACE_LUX != SPACE) || (GLX_LUT_SPACE_LAGUNA != SPACE)
#error "these tests expect the same input space for all three LUTs"
#endif

static long n_checks = 0, n_fail = 0;
static void check(int ok, const char *what)
{
    n_checks++;
    if (!ok) { n_fail++; if (n_fail < 20) printf("  FAIL: %s\n", what); }
}
static float frand(float a, float b) { return a + (b - a) * (float)rand() / (float)RAND_MAX; }
static int same3(float3 a, float3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
static int finite3(float3 a) { return isfinite(a.x) && isfinite(a.y) && isfinite(a.z); }

static const int LUT_LOOK[3] = { GLX_LOOK_VISTA, GLX_LOOK_LUX, GLX_LOOK_LAGUNA };
static const char *LUT_ID[3] = { "GlxLutVista", "GlxLutLux", "GlxLutLaguna" };

// Independent statement of the LUT stage: the LUT in its space, nothing else.
static float3 ref_lut(int i, float3 x)
{
    float3 li = (SPACE == GLX_LUT_REC709) ? glx_di_to_rec709(x) : glx_lut_domain(x);
    float3 lo = filmlab_apply_lut(li.x, li.y, li.z, LUT_ID[i]);
    return (SPACE == GLX_LUT_REC709) ? glx_rec709_to_di(lo) : lo;
}

// sample set: grid over DI -0.1 .. 1.1, grey ramp, extended values
static int nsamp = 0;
static float3 samp[3000];
static void build_samples(void)
{
    for (int i = 0; i < 11; i++) for (int j = 0; j < 11; j++) for (int k = 0; k < 11; k++)
        samp[nsamp++] = make_float3(-0.1f + 0.12f * i, -0.1f + 0.12f * j, -0.1f + 0.12f * k);
    for (int i = 0; i <= 100; i++) { float v = -0.2f + 1.5f * i / 100.0f; samp[nsamp++] = make_float3(v, v, v); }
    srand(1010);
    for (int i = 0; i < 500; i++) samp[nsamp++] = make_float3(frand(-0.6f, 2.6f), frand(-0.6f, 2.6f), frand(-0.6f, 2.6f));
}

static void identity_run(void)
{
    // With identity LUTs, a correct LUT stage returns its input: exactly the
    // clamped input in DI mode; in Rec.709 mode the input itself for colours
    // inside Rec.709 and below display white (the round trip's accuracy).
    double maxe = 0; long n = 0;
    for (int i = 0; i < 3; i++) {
        for (int s = 0; s < 200000; s++) {
            float3 x;
            if (SPACE == GLX_LUT_REC709) {
                float3 c = make_float3(frand(0, 1), frand(0, 1), frand(0, 1));   // Rec.709 gamma 2.4 values
                float3 l = glx_rec709_to_dwg(powf(c.x, 2.4f), powf(c.y, 2.4f), powf(c.z, 2.4f));
                x = make_float3(glx_lin_to_di(l.x), glx_lin_to_di(l.y), glx_lin_to_di(l.z));
            } else {
                x = make_float3(frand(-0.3f, 1.3f), frand(-0.3f, 1.3f), frand(-0.3f, 1.3f));
            }
            filmlab_reset(); ui_Look = LUT_LOOK[i];
            float3 o = transform(0, 0, 0, 0, x.x, x.y, x.z);
            float3 e = (SPACE == GLX_LUT_REC709) ? x : glx_lut_domain(x);
            double d = fmax(fabs(o.x - e.x), fmax(fabs(o.y - e.y), fabs(o.z - e.z)));
            if (d > maxe) maxe = d;
            n++;
        }
    }
    printf("identity LUTs (%s): %ld samples, max |out - expected| = %.3g (DaVinci Intermediate code value)\n",
           SPACE == GLX_LUT_REC709 ? "Rec.709 round trip" : "DI direct", n, maxe);
    check(maxe < (SPACE == GLX_LUT_REC709 ? 2e-5 : 1e-6), "identity LUT round trip");
}

int main(int argc, char **argv)
{
    printf("FilmLab V10 LUT tests, LUT input space = %s\n", SPACE == GLX_LUT_REC709 ? "Rec.709 Gamma 2.4" : "DaVinci Intermediate (direct)");
    filmlab_reset();
    if (argc > 1 && !strcmp(argv[1], "identity")) {
        identity_run();
        printf("%ld checks, %ld failed\n", n_checks, n_fail);
        return n_fail != 0;
    }
    build_samples();

    for (int interp = 0; interp < 2; interp++) {
        filmlab_lut_interp = interp;
        long c0 = n_checks, f0 = n_fail;
        for (int i = 0; i < 3; i++) {
            for (int s = 0; s < nsamp; s++) {
                float3 x = samp[s];
                // 1. LUT look at 100%, every other control at default: the LUT stage alone.
                filmlab_reset(); ui_Look = LUT_LOOK[i];
                float3 o100 = transform(0, 0, 0, 0, x.x, x.y, x.z);
                float3 L = ref_lut(i, x);
                check(same3(o100, L), "100% = the LUT stage exactly");
                // 2. Film Strength 0%: exact bypass.
                ui_CreativeIntensity = 0;
                check(same3(transform(0, 0, 0, 0, x.x, x.y, x.z), x), "Film Strength 0 = identity");
                // 3. Film Strength 50% / 150% / 200%: mix of input and LUT result in DI.
                const float T[3] = { 50, 150, 200 };
                for (int t = 0; t < 3; t++) {
                    ui_CreativeIntensity = T[t];
                    float tt = T[t] * 0.01f;
                    float3 e = make_float3(_clampf(x.x + (L.x - x.x) * tt, -0.5f, 2.0f), _clampf(x.y + (L.y - x.y) * tt, -0.5f, 2.0f),
                                           _clampf(x.z + (L.z - x.z) * tt, -0.5f, 2.0f));
                    check(same3(transform(0, 0, 0, 0, x.x, x.y, x.z), e), "Film Strength mix");
                }
                // 4. Global Intensity 0% and 37%.
                filmlab_reset(); ui_Look = LUT_LOOK[i]; ui_Intensity = 0;
                check(same3(transform(0, 0, 0, 0, x.x, x.y, x.z), x), "Global Intensity 0 = identity");
                ui_Intensity = 37;
                float gi = _clampf(37 * 0.01f, 0.0f, 1.0f);
                float3 e37 = make_float3(x.x + (o100.x - x.x) * gi, x.y + (o100.y - x.y) * gi, x.z + (o100.z - x.z) * gi);
                check(same3(transform(0, 0, 0, 0, x.x, x.y, x.z), e37), "Global Intensity mix");
                // 5. Exposure / Temperature / Tint run before the LUT.
                filmlab_reset(); ui_Look = LUT_LOOK[i]; ui_Exposure = 1.3f; ui_Temperature = 40; ui_Tint = -25;
                float3 xc = glx_work_to_di(glx_corrections(glx_di_to_work(x.x, x.y, x.z), 1.3f, 40.0f, -25.0f));
                check(same3(transform(0, 0, 0, 0, x.x, x.y, x.z), ref_lut(i, xc)), "corrections before the LUT");
                // 6. Secondary (Cine16) runs on the LUT result, unchanged.
                for (int sc = 1; sc < GLX_SEC_COUNT; sc += 5) {
                    filmlab_reset(); ui_Look = LUT_LOOK[i]; ui_SecLook = sc; ui_SecStrength = 0.8f;
                    C16Controls cc = { 0.8f, 1, 1, 1, 1, 1, 1 };
                    float3 e = c16_apply(L.x, L.y, L.z, sc - 1, cc, 0);
                    check(same3(transform(0, 0, 0, 0, x.x, x.y, x.z), e), "Cine16 after the LUT");
                }
                // 7. Grading runs after the LUT (Film Contrast and Highlight Roll-Off).
                filmlab_reset(); ui_Look = LUT_LOOK[i]; ui_Contrast = 40; ui_Rolloff = 60;
                {
                    GlxControls k = { 0 };
                    k.contrast = _clampf(40 * 0.01f, -1.0f, 1.0f); k.rolloff = _clampf(60 * 0.01f, 0.0f, 1.0f); k.intensity = 1.0f;
                    GlxFilmAdj adj = { 1.0f, 0.0f, 0.0f, 0.0f };
                    float3 w = glx_di_to_work(L.x, L.y, L.z);
                    w = glx_process_post(w.x, w.y, w.z, k, glx_profile_base(), glx_look_clean(), 0, adj, 0, 0);
                    check(same3(transform(0, 0, 0, 0, x.x, x.y, x.z), glx_work_to_di(w)), "grading after the LUT");
                }
            }
        }
        printf("interpolation %s: %ld checks, %ld failed\n", interp ? "tetrahedral" : "trilinear",
               n_checks - c0, n_fail - f0);
    }

    // 8. Finite output for every look, random settings, extended input.
    {
        long c0 = n_checks, f0 = n_fail;
        srand(77);
        for (int i = 0; i < 400000; i++) {
            filmlab_reset();
            filmlab_lut_interp = i & 1;
            ui_Look = rand() % GLX_LOOK_COUNT; ui_SecLook = rand() % GLX_SEC_COUNT;
            if (i % 3 == 0) ui_Look = LUT_LOOK[rand() % 3];
            ui_Exposure = frand(-5, 5); ui_Temperature = frand(-100, 100); ui_Tint = frand(-100, 100);
            ui_CreativeIntensity = frand(0, 200); ui_SecStrength = frand(0, 1.5f);
            ui_Contrast = frand(-100, 100); ui_Density = frand(-100, 100); ui_SubSat = frand(-100, 100);
            ui_Rolloff = frand(0, 100); ui_Shadow = frand(-100, 100); ui_Intensity = frand(0, 100);
            float3 x = make_float3(frand(-0.6f, 2.6f), frand(-0.6f, 2.6f), frand(-0.6f, 2.6f));
            // DI +-8 is about 10^30 x linear; beyond DI ~9.4 the decode itself exceeds float range.
            if (i % 50 == 0) x = make_float3(frand(-8, 8), frand(-8, 8), frand(-8, 8));
            check(finite3(transform(0, 0, 0, 0, x.x, x.y, x.z)), "finite output");
        }
        printf("random settings, all 22 looks: %ld checks, %ld failed\n", n_checks - c0, n_fail - f0);
    }

    // 9. Everything off: identity.
    {
        long c0 = n_checks, f0 = n_fail;
        filmlab_reset();
        for (int s = 0; s < nsamp; s++) check(same3(transform(0, 0, 0, 0, samp[s].x, samp[s].y, samp[s].z), samp[s]), "Off/Off identity");
        printf("Off / Off identity: %ld checks, %ld failed\n", n_checks - c0, n_fail - f0);
    }

    // 10. Rec.709 conversion against a double-precision reference (CST DWG /
    //     Intermediate -> Rec.709 Gamma 2.4), and middle grey.
    {
        long c0 = n_checks, f0 = n_fail;
        double M[3][3] = { { 1.8986148993, -0.7921761834, -0.1064387159 },
                           { -0.1689487865, 1.4889757541, -0.3200269676 },
                           { -0.1215391606, -0.3156758531, 1.4372150137 } };
        double maxe = 0, maxc = 0;
        srand(709);
        for (int i = 0; i < 200000; i++) {
            // DI -0.05 .. 0.62: linear up to ~2.6x display white, so the matrix and the
            // clamp at 1 are both exercised without float cancellation of huge values
            float in[3] = { frand(-0.05f, 0.62f), frand(-0.05f, 0.62f), frand(-0.05f, 0.62f) };
            double lin[3], out[3];
            for (int c = 0; c < 3; c++)
                lin[c] = (in[c] <= 0.02740668) ? in[c] / 10.44426855 : exp2(in[c] / 0.07329248 - 7.0) - 0.0075;
            for (int c = 0; c < 3; c++) {
                double v = M[c][0] * lin[0] + M[c][1] * lin[1] + M[c][2] * lin[2];
                v = v < 0 ? 0 : v > 1 ? 1 : v;
                out[c] = pow(v, 1.0 / 2.4);
            }
            float3 f = glx_di_to_rec709(make_float3(in[0], in[1], in[2]));
            // compared in linear Rec.709: the 1/2.4 power magnifies float
            // rounding of values within 1e-6 of black, which is not visible
            double d = fmax(fabs(pow(f.x, 2.4) - pow(out[0], 2.4)), fmax(fabs(pow(f.y, 2.4) - pow(out[1], 2.4)),
                                                                     fabs(pow(f.z, 2.4) - pow(out[2], 2.4))));
            if (d > maxe) maxe = d;
            double dc = fmax(fabs(f.x - out[0]), fmax(fabs(f.y - out[1]), fabs(f.z - out[2])));
            if (fmin(out[0], fmin(out[1], out[2])) > 0.05 && dc > maxc) maxc = dc;
        }
        float g = glx_lin_to_di(0.18f);
        float3 g7 = glx_di_to_rec709(make_float3(g, g, g));
        printf("Rec.709 conversion vs double reference: max error %.3g linear, %.3g code value (channels > 0.05);\n"
               "  18%% grey (DI %.5f) -> Rec.709 %.5f %.5f %.5f (expected %.5f)\n",
               maxe, maxc, g, g7.x, g7.y, g7.z, pow(0.18, 1 / 2.4));
        check(maxe < 1e-5 && maxc < 1e-5, "Rec.709 conversion accuracy");   // 1e-5 = 1/100 of a 10-bit step
        check(fabs(g7.x - pow(0.18, 1 / 2.4)) < 2e-6 && fabs(g7.y - g7.x) < 1e-6 && fabs(g7.z - g7.x) < 1e-6, "grey");
        printf("Rec.709 conversion: %ld checks, %ld failed\n", n_checks - c0, n_fail - f0);
    }

    printf("TOTAL: %ld checks, %ld failed\n", n_checks, n_fail);
    return n_fail != 0;
}
