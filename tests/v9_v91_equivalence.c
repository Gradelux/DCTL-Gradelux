// =============================================================================
//  Gradelux FilmLab: Version 9 vs Version 9.1 equivalence dump
// -----------------------------------------------------------------------------
//  Version 9.1 removes controls from the interface and fixes them at their
//  Version 9 defaults. This program runs one of the two DCTLs on identical
//  inputs with identical settings of every control Version 9.1 keeps; the
//  Version 9 build leaves every removed control at its Version 9 default.
//  The two dumps must be identical (bit-for-bit, %a):
//
//    cd tests
//    gcc -O2 -DSRC_V9 -o eq9  v9_v91_equivalence.c -lm && ./eq9  > v9.txt
//    gcc -O2          -o eq91 v9_v91_equivalence.c -lm && ./eq91 > v91.txt
//    cmp v9.txt v91.txt
//
//  Sections (each line starts with its section tag):
//    L  every primary look alone (Secondary Off, Print None)
//    S  every secondary profile alone
//    P  every print alone
//    LS every primary x secondary pair
//    LP every primary x print pair
//    LSP every primary x secondary x print triple
//    OFF every engine disabled (also checked against the input: identity)
//    EV exposure -5 .. +5 stops with every look
//    R  random settings of all twelve sliders and the three dropdowns,
//       extended-range input
// =============================================================================
#include "../tools/filmlab_shim.h"
#if defined(SRC_V9)
#include "../Gradelux_FilmLab_v9.dctl"
#else
#include "../Gradelux_FilmLab_v9_1.dctl"
#endif

static float frand(float a, float b) { return a + (b - a) * (float)rand() / (float)RAND_MAX; }
static void out(const char *tag, float r, float g, float b) {
    float3 o = transform(0, 0, 0, 0, r, g, b);
    printf("%s %a %a %a\n", tag, o.x, o.y, o.z);
}
// n^3 grid over DaVinci Intermediate -0.1 .. 1.1, plus a grey ramp and extended-range points
static void grid(const char *tag, int n) {
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) for (int k = 0; k < n; k++) {
        float s = 1.2f / (float)(n - 1);
        out(tag, -0.1f + s * i, -0.1f + s * j, -0.1f + s * k);
    }
}
static void extras(const char *tag) {
    for (int i = 0; i <= 64; i++) { float v = -0.1f + 1.4f * i / 64.0f; out(tag, v, v, v); }
    const float X[][3] = { {-0.5f, 0.3f, 0.2f}, {2.0f, 2.0f, 2.0f}, {1.6f, 0.1f, -0.3f}, {0.2f, 1.5f, 0.9f},
                           {-0.2f, -0.2f, -0.2f}, {0.9f, 0.05f, 1.3f}, {2.5f, 0.4f, 0.1f} };
    for (int i = 0; i < 7; i++) out(tag, X[i][0], X[i][1], X[i][2]);
}

int main(void)
{
    filmlab_reset();
    for (int l = 1; l < GLX_LOOK_COUNT; l++) { filmlab_reset(); ui_Look = l; grid("L", 9); extras("L"); }
    for (int s = 1; s < GLX_SEC_COUNT; s++) { filmlab_reset(); ui_SecLook = s; grid("S", 9); extras("S"); }
    for (int p = 1; p < GLX_PRINT_COUNT; p++) { filmlab_reset(); ui_Print = p; grid("P", 9); extras("P"); }
    for (int l = 1; l < GLX_LOOK_COUNT; l++)
        for (int s = 1; s < GLX_SEC_COUNT; s++) { filmlab_reset(); ui_Look = l; ui_SecLook = s; grid("LS", 5); }
    for (int l = 1; l < GLX_LOOK_COUNT; l++)
        for (int p = 1; p < GLX_PRINT_COUNT; p++) { filmlab_reset(); ui_Look = l; ui_Print = p; grid("LP", 5); }
    for (int l = 0; l < GLX_LOOK_COUNT; l++)
        for (int s = 0; s < GLX_SEC_COUNT; s++)
            for (int p = 0; p < GLX_PRINT_COUNT; p++) { filmlab_reset(); ui_Look = l; ui_SecLook = s; ui_Print = p; grid("LSP", 3); }
    // Every engine disabled: Off / Off / None (and Film Strength / Print Strength 0 with a selection).
    for (int c = 0; c < 3; c++) {
        filmlab_reset();
        if (c == 1) { ui_Look = 1; ui_CreativeIntensity = 0; }
        if (c == 2) { ui_Print = 1; ui_PrintStrength = 0; }
        grid("OFF", 9); extras("OFF");
    }
    for (int e = -10; e <= 10; e++)
        for (int l = 0; l < GLX_LOOK_COUNT; l++) {
            filmlab_reset(); ui_Exposure = 0.5f * e; ui_Look = l; ui_SecLook = (l * 7) % GLX_SEC_COUNT;
            ui_Print = l % GLX_PRINT_COUNT; grid("EV", 4);
        }
    srand(91);
    for (int i = 0; i < 200000; i++) {
        filmlab_reset();
        ui_Look = rand() % GLX_LOOK_COUNT; ui_SecLook = rand() % GLX_SEC_COUNT; ui_Print = rand() % GLX_PRINT_COUNT;
        ui_Exposure = frand(-5, 5); ui_Temperature = frand(-100, 100); ui_Tint = frand(-100, 100);
        ui_CreativeIntensity = frand(0, 200); ui_SecStrength = frand(0, 1.5f); ui_PrintStrength = frand(0, 100);
        ui_Contrast = frand(-100, 100); ui_Density = frand(-100, 100); ui_SubSat = frand(-100, 100);
        ui_Rolloff = frand(0, 100); ui_Shadow = frand(-100, 100); ui_Intensity = frand(0, 100);
        if (i % 4 == 0) { ui_Exposure = 0; ui_Temperature = 0; ui_Tint = 0; }        // also cover exact-default paths
        if (i % 5 == 0) { ui_Contrast = ui_Density = ui_SubSat = ui_Rolloff = ui_Shadow = 0; ui_Intensity = 100; }
        out("R", frand(-0.6f, 2.6f), frand(-0.6f, 2.6f), frand(-0.6f, 2.6f));
    }
    return 0;
}
