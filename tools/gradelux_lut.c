// =============================================================================
//  Gradelux FilmLab — reference LUT generator / validator (Version 6.1; -DFILMLAB_V4 for Version 4)
// -----------------------------------------------------------------------------
//  Compiles Gradelux_FilmLab_v6_1.dctl (or v4 with -DFILMLAB_V4) itself (through a small C shim) and
//  samples its transform() in one of three reference configurations:
//      A. Film Look only      : Look = <look>,  Print = None
//      B. Film Print only     : Look = Off,     Print = <print>
//      C. Film Look + Print   : Look = <look>,  Print = <print>
//  with Creative Intensity 100%, Print Strength 100%, print controls 0 and
//  every other control at its default (Global Intensity 100%). That IS the
//  reference transform. Because the generator runs the same source code as
//  the DCTL, a LUT can never drift from the DCTL.
//
//  BUILD
//      gcc -O2 -o gradelux_lut tools/gradelux_lut.c -lm      (from repo root; add -DFILMLAB_V4 for v4)
//
//  USAGE
//      gradelux_lut list
//      gradelux_lut cube     <look #> <print 0-5> <dwg|rec709> <33|65>  > file.cube
//      gradelux_lut validate <look #> <print 0-5> <dwg|rec709> <33|65>
//      gradelux_lut all      <print 0-5> <dwg|rec709> <33|65> <output dir>
//    look 0 = Off, print 0 = None (not both). "all" writes every look with
//    the given print (0 = look-only LUTs).
//
//  PIPELINES
//    dwg    : DaVinci Intermediate (DWG) in -> look -> DaVinci Intermediate out.
//             Domain: DI code values 0..1 on each axis (linear 0 .. ~100,
//             i.e. up to ~+9 stops over 18% grey). Output is written
//             unclamped (it may fall slightly outside 0..1 for out-of-gamut
//             colours).
//    rec709 : Rec.709 / gamma 2.4 code values in -> BT.1886 EOTF (V^2.4,
//             black 0) -> linear Rec.709 -> 3x3 matrix -> linear DWG ->
//             DaVinci Intermediate -> look -> DaVinci Intermediate -> linear
//             DWG -> 3x3 matrix -> linear Rec.709 -> clip each channel to
//             0..1 -> inverse EOTF (L^(1/2.4)).
//             Domain: 0..1. Output: 0..1 (hard per-channel clip, see docs).
//
//  .cube format: TITLE, LUT_3D_SIZE N, then N^3 lines "R G B", red index
//  varying fastest, blue slowest; no DOMAIN lines (default 0..1).
// =============================================================================
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
#ifdef FILMLAB_V4
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
static const char *PRINT_NAMES[] = { "None", "Print2383", "Print2393", "Print3513", "Print3510", "Print2383Soft" };
#define FILMLAB_SOURCE "../Gradelux_FilmLab_v4.dctl"
#else   // Version 6 (8 film stocks)
enum { GLX_LOOK_OFF, GLX_LOOK_VERONA_250D, GLX_LOOK_VERDANT_250D, GLX_LOOK_MIDNIGHT_800T, GLX_LOOK_AURELIA_400,
       GLX_LOOK_SOLARA_200, GLX_LOOK_MOSS_250D, GLX_LOOK_CHROMA_100, GLX_LOOK_SILVERGRAIN_100, GLX_LOOK_COUNT };
enum { GLX_PRINT_NONE, GLX_PRINT_CINECHROME_2383, GLX_PRINT_VELACHROME_2393, GLX_PRINT_LUMACHROME_3513,
       GLX_PRINT_RETROCHROME_3510, GLX_PRINT_SILKCHROME_2383, GLX_PRINT_COUNT };
enum { GLX_DIAG_NORMAL, GLX_DIAG_COLOR, GLX_DIAG_TONE, GLX_DIAG_FULL };
static int ui_Diag = 0;
static const char *LOOK_NAMES[] = { "Off", "Verona250D", "Verdant250D", "Midnight800T", "Aurelia400",
                                    "Solara200", "Moss250D", "Chroma100", "Silvergrain100" };
static const char *PRINT_NAMES[] = { "None", "Cinechrome2383", "Velachrome2393", "Lumachrome3513",
                                     "Retrochrome3510", "Silkchrome2383" };
#ifndef FILMLAB_SOURCE   /* Version 6.1 by default; -DFILMLAB_SOURCE='"../Gradelux_FilmLab_v6.dctl"' for 6.0 */
#define FILMLAB_SOURCE "../Gradelux_FilmLab_v6_1.dctl"
#endif
#endif

// UI globals: reference configuration (defaults; Film Look / Film Print set per call)
static int   ui_Look = 0, ui_Print = 0;
static float ui_CreativeIntensity = 100, ui_PrintStrength = 100, ui_PrintContrast = 0, ui_PrintDensity = 0,
             ui_PrintSaturation = 0, ui_PrintRolloff = 0;
static float ui_ColorDensity = 0, ui_Separation = 50, ui_HlWarmth = 0, ui_ShadowTone = 0;
static float ui_Density = 0, ui_SubSat = 0, ui_Contrast = 0, ui_Shadow = 0, ui_Rolloff = 0, ui_Intensity = 100;

#include FILMLAB_SOURCE

// ---- Rec.709 <-> DWG (both D65; derived from primaries in double precision) --
static void rec709_to_dwg(const double *i, double *o) {
    o[0] = 0.5627674560 * i[0] + 0.3235165887 * i[1] + 0.1137159553 * i[2];
    o[1] = 0.0777546353 * i[0] + 0.7495773462 * i[1] + 0.1726680186 * i[2];
    o[2] = 0.0646691999 * i[0] + 0.1919986920 * i[1] + 0.7433321080 * i[2];
}
static void dwg_to_rec709(const double *i, double *o) {
    o[0] =  1.8986148993 * i[0] - 0.7921761834 * i[1] - 0.1064387159 * i[2];
    o[1] = -0.1689487865 * i[0] + 1.4889757541 * i[1] - 0.3200269676 * i[2];
    o[2] = -0.1215391606 * i[0] - 0.3156758531 * i[1] + 1.4372150137 * i[2];
}

// A configuration packs look and print: cfg = look * 16 + print.
static int  cfg_look(int cfg)  { return cfg >> 4; }
static int  cfg_print(int cfg) { return cfg & 15; }
static void cfg_name(int cfg, char *buf, size_t len) {
    int l = cfg_look(cfg), p = cfg_print(cfg);
    if (l && p) snprintf(buf, len, "%s + %s", LOOK_NAMES[l], PRINT_NAMES[p]);
    else if (l)  snprintf(buf, len, "%s", LOOK_NAMES[l]);
    else         snprintf(buf, len, "%s", PRINT_NAMES[p]);
}

// Reference transform on DaVinci Intermediate code values.
static void look_di(int look, const double *in, double *out) {
    ui_Look = cfg_look(look); ui_Print = cfg_print(look);
    float3 o = transform(0, 0, 0, 0, (float)in[0], (float)in[1], (float)in[2]);
    out[0] = o.x; out[1] = o.y; out[2] = o.z;
}

// Full pipeline for one LUT input triplet (code values 0..1).
static void eval(int look, int rec709, const double *in, double *out) {
    if (!rec709) { look_di(look, in, out); return; }
    double l709[3], dwg[3], di[3], o[3];
    for (int c = 0; c < 3; c++) l709[c] = pow(fmax(in[c], 0.0), 2.4);          // BT.1886, black 0
    rec709_to_dwg(l709, dwg);
    for (int c = 0; c < 3; c++) di[c] = glx_lin_to_di((float)dwg[c]);
    look_di(look, di, o);
    for (int c = 0; c < 3; c++) dwg[c] = glx_di_to_lin((float)o[c]);
    dwg_to_rec709(dwg, l709);
    for (int c = 0; c < 3; c++) out[c] = pow(fmin(fmax(l709[c], 0.0), 1.0), 1.0 / 2.4);
}

static double *build(int look, int rec709, int n) {
    double *lut = malloc(sizeof(double) * 3 * n * n * n);
    for (int b = 0; b < n; b++)
        for (int g = 0; g < n; g++)
            for (int r = 0; r < n; r++) {
                double in[3] = { (double)r / (n - 1), (double)g / (n - 1), (double)b / (n - 1) };
                eval(look, rec709, in, lut + 3 * ((b * n + g) * n + r));
            }
    return lut;
}

static void write_cube(FILE *f, int look, int rec709, int n, const double *lut) {
    char name[128]; cfg_name(look, name, sizeof name);
    fprintf(f, "TITLE \"Gradelux %s (%s)\"\n", name, rec709 ? "Rec.709 Gamma 2.4" : "DWG Intermediate");
    fprintf(f, "# Gradelux FilmLab v4 reference transform: %s (Creative Intensity 100%%, Print Strength 100%%, print controls 0)\n", name);
    if (rec709)
        fprintf(f, "# Input and output: Rec.709 / Gamma 2.4 code values (output clipped to 0..1)\n");
    else
        fprintf(f, "# Input and output: DaVinci Wide Gamut / DaVinci Intermediate code values\n");
    fprintf(f, "LUT_3D_SIZE %d\n", n);
    for (int i = 0; i < n * n * n; i++)
        fprintf(f, "%.6f %.6f %.6f\n", lut[3 * i], lut[3 * i + 1], lut[3 * i + 2]);
}

// Tetrahedral interpolation of the LUT at input in[] (0..1, clamped).
static void tetra(const double *lut, int n, const double *in, double *out) {
    double p[3]; int i0[3]; double f[3];
    for (int c = 0; c < 3; c++) {
        p[c] = fmin(fmax(in[c], 0.0), 1.0) * (n - 1);
        i0[c] = (int)floor(p[c]); if (i0[c] >= n - 1) i0[c] = n - 2;
        f[c] = p[c] - i0[c];
    }
#define L(r, g, b, c) lut[3 * (((i0[2] + (b)) * n + (i0[1] + (g))) * n + (i0[0] + (r))) + (c)]
    double fr = f[0], fg = f[1], fb = f[2];
    for (int c = 0; c < 3; c++) {
        double c000 = L(0,0,0,c), c111 = L(1,1,1,c), v;
        if (fr > fg) {
            if (fg > fb)      v = (1-fr)*c000 + (fr-fg)*L(1,0,0,c) + (fg-fb)*L(1,1,0,c) + fb*c111;
            else if (fr > fb) v = (1-fr)*c000 + (fr-fb)*L(1,0,0,c) + (fb-fg)*L(1,0,1,c) + fg*c111;
            else              v = (1-fb)*c000 + (fb-fr)*L(0,0,1,c) + (fr-fg)*L(1,0,1,c) + fg*c111;
        } else {
            if (fb > fg)      v = (1-fb)*c000 + (fb-fg)*L(0,0,1,c) + (fg-fr)*L(0,1,1,c) + fr*c111;
            else if (fb > fr) v = (1-fg)*c000 + (fg-fb)*L(0,1,0,c) + (fb-fr)*L(0,1,1,c) + fr*c111;
            else              v = (1-fg)*c000 + (fg-fr)*L(0,1,0,c) + (fr-fb)*L(1,1,0,c) + fb*c111;
        }
        out[c] = v;
    }
#undef L
}

static double frand(void) { return (double)rand() / RAND_MAX; }

// Error of the LUT vs the exact engine, in output code values.
static void validate(int look, int rec709, int n) {
    double *lut = build(look, rec709, n);
    struct { const char *name; int count; double maxe, sume; } set[5] = {
        {"full cube", 0, 0, 0}, {"real colours", 0, 0, 0}, {"grey ramp", 0, 0, 0}, {"skin tones", 0, 0, 0}, {"hue sweep", 0, 0, 0} };
    srand(2024);
    for (int k = 0; k < 5; k++) {
        int N = (k <= 1) ? 200000 : 20000;
        for (int i = 0; i < N; i++) {
            double in[3];
            if (k == 0) { in[0] = frand(); in[1] = frand(); in[2] = frand(); }   // whole domain, incl. imaginary colours
            else if (k == 1) {
                if (rec709) { in[0] = frand(); in[1] = frand(); in[2] = frand(); }
                else {   // physically real colours: inside Rec.2020, -8 .. +6 stops around 18% grey
                    double w[3] = { frand(), frand(), frand() }, d[3];
                    double mx = fmax(w[0], fmax(w[1], w[2])) + 1e-9, sc = 0.18 * pow(2.0, -8.0 + 14.0 * frand()) / mx;
                    float3 dw = glx_work_to_dwg((float)(w[0] * sc), (float)(w[1] * sc), (float)(w[2] * sc));
                    d[0] = dw.x; d[1] = dw.y; d[2] = dw.z;
                    for (int c = 0; c < 3; c++) in[c] = glx_lin_to_di((float)d[c]);
                }
            }
            else if (k == 2) { in[0] = in[1] = in[2] = (double)i / (N - 1); }
            else if (k == 3) {   // skin: warm, moderate saturation, across brightness (Rec.709 code values)
                double v = 0.15 + 0.75 * frand(), s = 0.15 + 0.35 * frand(), h = 0.04 + 0.06 * frand();
                in[0] = v; in[1] = v * (1 - s * (1 - h * 6)); in[2] = v * (1 - s);
            } else {             // saturated hue sweep (85% HSV saturation) at mid brightness (Rec.709 code values)
                double h = 6.0 * i / N, x = h - floor(h), v = 0.6, lo = 0.15; int s = (int)h % 6;
                double R[] = {1, 1 - x, 0, 0, x, 1}, G[] = {x, 1, 1, 1 - x, 0, 0}, B[] = {0, 0, x, 1, 1, 1 - x};
                in[0] = (lo + (1 - lo) * R[s]) * v; in[1] = (lo + (1 - lo) * G[s]) * v; in[2] = (lo + (1 - lo) * B[s]) * v;
            }
            // For the DWG pipeline, express skin and sweep as the same real
            // colours: Rec.709 code -> display light -> linear DWG -> DI.
            if (!rec709 && k >= 3) {
                double l[3], d[3];
                for (int c = 0; c < 3; c++) l[c] = pow(in[c], 2.4);
                rec709_to_dwg(l, d);
                for (int c = 0; c < 3; c++) in[c] = glx_lin_to_di((float)d[c]);
            }
            double ex[3], lu[3];
            eval(look, rec709, in, ex);
            tetra(lut, n, in, lu);
            double e = fmax(fabs(ex[0] - lu[0]), fmax(fabs(ex[1] - lu[1]), fabs(ex[2] - lu[2])));
            set[k].maxe = fmax(set[k].maxe, e); set[k].sume += e; set[k].count++;
        }
    }
    char name[128]; cfg_name(look, name, sizeof name);
    printf("%-22s %-6s %2d^3 :", name, rec709 ? "rec709" : "dwg", n);
    for (int k = 0; k < 5; k++)
        printf(" %s max %.4f mean %.5f |", set[k].name, set[k].maxe, set[k].sume / set[k].count);
    printf("\n");
    free(lut);
}

static int parse_range(const char *s, int lo, int hi, const char *what) {
    int id = atoi(s);
    if (id < lo || id > hi) { fprintf(stderr, "%s must be %d..%d\n", what, lo, hi); exit(1); }
    return id;
}

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "list")) {
        for (int i = 1; i < GLX_LOOK_COUNT; i++) printf("look  %2d %s\n", i, LOOK_NAMES[i]);
        for (int i = 1; i < GLX_PRINT_COUNT; i++) printf("print %2d %s\n", i, PRINT_NAMES[i]);
        return 0;
    }
    if (argc == 6 && (!strcmp(argv[1], "cube") || !strcmp(argv[1], "validate"))) {
        int look = parse_range(argv[2], 0, GLX_LOOK_COUNT - 1, "look");
        int pr   = parse_range(argv[3], 0, GLX_PRINT_COUNT - 1, "print");
        int rec = !strcmp(argv[4], "rec709"), n = atoi(argv[5]);
        if (!look && !pr) { fprintf(stderr, "choose a look, a print, or both\n"); return 1; }
        if (n < 2 || n > 129) { fprintf(stderr, "size must be 2..129\n"); return 1; }
        int cfg = look * 16 + pr;
        if (!strcmp(argv[1], "validate")) { validate(cfg, rec, n); return 0; }
        double *lut = build(cfg, rec, n);
        write_cube(stdout, cfg, rec, n, lut);
        free(lut);
        return 0;
    }
    if (argc == 6 && !strcmp(argv[1], "all")) {
        int pr = parse_range(argv[2], 0, GLX_PRINT_COUNT - 1, "print");
        int rec = !strcmp(argv[3], "rec709"), n = atoi(argv[4]);
        for (int look = 1; look < GLX_LOOK_COUNT; look++) {
            char path[1024];
            snprintf(path, sizeof path, "%s/Gradelux_%02d_%s%s%s_%s_%d.cube", argv[5], look, LOOK_NAMES[look],
                     pr ? "_" : "", pr ? PRINT_NAMES[pr] : "", rec ? "Rec709" : "DWG", n);
            FILE *f = fopen(path, "w");
            if (!f) { perror(path); return 1; }
            int cfg = look * 16 + pr;
            double *lut = build(cfg, rec, n);
            write_cube(f, cfg, rec, n, lut);
            fclose(f); free(lut);
            fprintf(stderr, "wrote %s\n", path);
        }
        return 0;
    }
    fprintf(stderr, "usage: gradelux_lut list | cube <look #> <print 0-5> <dwg|rec709> <size>\n"
                    "       | validate <look> <print> <dwg|rec709> <size> | all <print 0-5> <dwg|rec709> <size> <dir>\n");
    return 1;
}
