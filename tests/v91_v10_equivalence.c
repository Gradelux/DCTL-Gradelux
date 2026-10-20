// =============================================================================
//  Gradelux FilmLab: Version 9.1 vs Version 10 equivalence dump
// -----------------------------------------------------------------------------
//  Version 10 removes the film print engine. With
//  Film Print = None, every Version 9.1 configuration must render identically
//  in Version 10. This program runs one of the two DCTLs on identical inputs
//  with identical settings of every control Version 10 keeps (Version 9.1
//  keeps Film Print = None). The two dumps must be identical (bit for bit, %a):
//
//    cd tests
//    gcc -O2 -DSRC_V91 -o eq91 v91_v10_equivalence.c -lm && ./eq91 > v91.txt
//    gcc -O2           -o eq10 v91_v10_equivalence.c -lm && ./eq10 > v10.txt
//    cmp v91.txt v10.txt
//
//  Sections (each line starts with its section tag):
//    L   every film look alone (Secondary Off)
//    S   every secondary profile alone
//    LS  every look x secondary pair (including Off)
//    OFF every engine disabled (Off / Off, or Film Strength 0 with a look)
//    EV  exposure -5 .. +5 stops with every look
//    G   every grading slider alone at -100, -50, 50, 100 with every look
//    R   random settings of all eleven sliders and both dropdowns,
//        extended-range input
// =============================================================================
#if defined(SRC_V91)
#include "../tools/filmlab_shim.h"
#include "../Gradelux_FilmLab_v9_1.dctl"
#else
#include "../tools/filmlab_shim.h"
#include "../Gradelux_FilmLab_v10.dctl"
#endif

static float frand(float a, float b) { return a + (b - a) * (float)rand() / (float)RAND_MAX; }
static void out(const char *tag, float r, float g, float b) {
    float3 o = transform(0, 0, 0, 0, r, g, b);
    printf("%s %a %a %a\n", tag, o.x, o.y, o.z);
}
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
    const int NL = GLX_LOOK_COUNT;                  // Off + 18 film looks
    for (int l = 1; l < NL; l++) { filmlab_reset(); ui_Look = l; grid("L", 9); extras("L"); }
    for (int s = 1; s < GLX_SEC_COUNT; s++) { filmlab_reset(); ui_SecLook = s; grid("S", 9); extras("S"); }
    for (int l = 0; l < NL; l++)
        for (int s = 0; s < GLX_SEC_COUNT; s++) { filmlab_reset(); ui_Look = l; ui_SecLook = s; grid("LS", 5); }
    for (int c = 0; c < 2; c++) {
        filmlab_reset();
        if (c == 1) { ui_Look = 1; ui_CreativeIntensity = 0; }
        grid("OFF", 9); extras("OFF");
    }
    for (int e = -10; e <= 10; e++)
        for (int l = 0; l < NL; l++) {
            filmlab_reset(); ui_Exposure = 0.5f * e; ui_Look = l; ui_SecLook = (l * 7) % GLX_SEC_COUNT; grid("EV", 4);
        }
    const float GV[4] = { -100, -50, 50, 100 };
    for (int l = 0; l < NL; l++)
        for (int c = 0; c < 5; c++)
            for (int v = 0; v < 4; v++) {
                filmlab_reset(); ui_Look = l;
                float x = (c == 3) ? fabsf(GV[v]) : GV[v];
                switch (c) {
                    case 0: ui_Contrast = x; break;
                    case 1: ui_Density  = x; break;
                    case 2: ui_SubSat   = x; break;
                    case 3: ui_Rolloff  = x; break;
                    default: ui_Shadow  = x; break;
                }
                grid("G", 4);
            }
    srand(10);
    for (int i = 0; i < 200000; i++) {
        filmlab_reset();
        ui_Look = rand() % NL; ui_SecLook = rand() % GLX_SEC_COUNT;
        ui_Exposure = frand(-5, 5); ui_Temperature = frand(-100, 100); ui_Tint = frand(-100, 100);
        ui_CreativeIntensity = frand(0, 200); ui_SecStrength = frand(0, 1.5f);
        ui_Contrast = frand(-100, 100); ui_Density = frand(-100, 100); ui_SubSat = frand(-100, 100);
        ui_Rolloff = frand(0, 100); ui_Shadow = frand(-100, 100); ui_Intensity = frand(0, 100);
        if (i % 4 == 0) { ui_Exposure = 0; ui_Temperature = 0; ui_Tint = 0; }
        if (i % 5 == 0) { ui_Contrast = ui_Density = ui_SubSat = ui_Rolloff = ui_Shadow = 0; ui_Intensity = 100; }
        if (i % 7 == 0) ui_SecLook = 0;
        out("R", frand(-0.6f, 2.6f), frand(-0.6f, 2.6f), frand(-0.6f, 2.6f));
    }
    return 0;
}
