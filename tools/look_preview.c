// =============================================================================
//  Gradelux FilmLab — look preview renderer (Version 6; -DFILMLAB_V4 for Version 4)
// -----------------------------------------------------------------------------
//  Renders a synthetic test scene through any Film Look / Film Print
//  combinations (reference configuration) using the DCTL source itself.
//  Scene (built in linear Rec.709, converted to DWG / DaVinci Intermediate):
//    rows 1-2  : 24 colour-checker patches (sRGB reference values)
//    row 3     : six skin tones, pale to deep, lit and in shade
//    row 4     : hue sweep (85% saturation), -3 .. +2 stops top to bottom
//    row 5     : grey ramp, -7 .. +5 stops around 18% grey
//  Output: raw float32 DaVinci Intermediate RGB, 320x200 per tile. Tiles
//  are given as look:print pairs (default: all 24 looks with no print).
//  Turn it into a contact sheet with tools/look_preview.py.
//
//    gcc -O2 -o look_preview tools/look_preview.c -lm
//    ./look_preview looks.raw                  # all looks
//    ./look_preview combos.raw 1:0 1:1 1:2     # look 1 alone, with print 1, with print 2
//    python3 tools/look_preview.py looks.raw preview.png "Off|Pure|..." [columns]
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
#define FILMLAB_SOURCE "../Gradelux_FilmLab_v6.dctl"
#endif

// UI globals: reference configuration (defaults; Film Look / Film Print set per tile)
static int   ui_Look = 0, ui_Print = 0;
static float ui_CreativeIntensity = 100, ui_PrintStrength = 100, ui_PrintContrast = 0, ui_PrintDensity = 0,
             ui_PrintSaturation = 0, ui_PrintRolloff = 0;
static float ui_ColorDensity = 0, ui_Separation = 50, ui_HlWarmth = 0, ui_ShadowTone = 0;
static float ui_Density = 0, ui_SubSat = 0, ui_Contrast = 0, ui_Shadow = 0, ui_Rolloff = 0, ui_Intensity = 100;

#include FILMLAB_SOURCE

#define W 320
#define H 200
static float srgb2lin(float c){ c/=255.f; return c<=0.04045f? c/12.92f : powf((c+0.055f)/1.055f,2.4f); }
static void r709_to_dwg(float r,float g,float b,float*o){ // 709 -> XYZ -> DWG via work (709->2020->DWG)
  // 709 -> 2020
  float R=0.6274040f*r+0.3292820f*g+0.0433136f*b, G=0.0690970f*r+0.9195400f*g+0.0113612f*b, B=0.0163916f*r+0.0880132f*g+0.8955950f*b;
  float3 d=glx_work_to_dwg(R,G,B); o[0]=d.x;o[1]=d.y;o[2]=d.z; }
static const unsigned char MAC[24][3]={{115,82,68},{194,150,130},{98,122,157},{87,108,67},{133,128,177},{103,189,170},
 {214,126,44},{80,91,166},{193,90,99},{94,60,108},{157,188,64},{224,163,46},{56,61,150},{70,148,73},{175,54,60},{231,199,31},
 {187,86,149},{8,133,161},{243,243,242},{200,200,200},{160,160,160},{122,122,121},{85,85,85},{52,52,52}};
static const unsigned char SKIN[6][3]={{255,224,204},{241,194,167},{224,172,140},{194,150,130},{141,85,36},{92,58,40}};
static void scene(int x,int y,float*o){
  float r,g,b;
  if(y<70){ // macbeth 12x2
    int cx=x*12/W, cy=y/35; int i=cy*12+cx; r=srgb2lin(MAC[i][0]);g=srgb2lin(MAC[i][1]);b=srgb2lin(MAC[i][2]);
  } else if(y<100){ int i=x*6/W; r=srgb2lin(SKIN[i][0]);g=srgb2lin(SKIN[i][1]);b=srgb2lin(SKIN[i][2]);
    float sh = (y<85)?1.0f:0.35f; r*=sh;g*=sh;b*=sh; // lit and shaded skin
  } else if(y<170){ float h=(float)x/W*6.0f; float v=powf(2.0f,((169-y)/70.0f)*5.0f-3.0f)*0.18f; // -3..+2 stops
    float f=h-floorf(h); int k=(int)floorf(h)%6; float s=0.85f; float p=1-s,q=1-s*f,t=1-s*(1-f);
    float rr[]={1,q,p,p,t,1},gg[]={t,1,1,q,p,p},bb[]={p,p,t,1,1,q}; r=rr[k]*v*2.2f;g=gg[k]*v*2.2f;b=bb[k]*v*2.2f;
  } else { float st=-7.0f+12.0f*x/W; r=g=b=0.18f*powf(2.0f,st); }
  r709_to_dwg(r,g,b,o);
}
int main(int argc, char **argv){
  (void)LOOK_NAMES; (void)PRINT_NAMES;
  FILE*f=fopen(argc > 1 ? argv[1] : "looks.raw","wb");
  static float img[H][W][3];
  int tiles = (argc > 2) ? argc - 2 : GLX_LOOK_COUNT;
  for(int t=0;t<tiles;t++){
    int look = t, pr = 0;
    if (argc > 2) sscanf(argv[t + 2], "%d:%d", &look, &pr);
    ui_Look=look; ui_Print=pr;
    for(int y=0;y<H;y++)for(int x=0;x<W;x++){ float o[3]; scene(x,y,o);
      float3 c=transform(0,0,0,0,glx_lin_to_di(o[0]),glx_lin_to_di(o[1]),glx_lin_to_di(o[2]));
      img[y][x][0]=c.x;img[y][x][1]=c.y;img[y][x][2]=c.z; }
    fwrite(img,sizeof(img),1,f);
  }
  fclose(f); return 0;
}
