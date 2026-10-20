// =============================================================================
//  Gradelux FilmLab — look visibility diagnostics
// -----------------------------------------------------------------------------
//  Measures how strongly each film stock changes the image AS SEEN: content
//  (colour checker, skin, hue sweep at three exposures, grey ramp) runs
//  through the stock's stages, then through a display transform to CIELAB.
//  Reports mean dE vs Off (about 2 = just noticeable, 10+ = obvious) for the
//  full stock, its colour and tone stages, with stages removed, and over a
//  print; plus dE between stocks (distinctness).
//
//    gcc -O2 -o look_visibility tools/look_visibility.c -lm && ./look_visibility   (Version 9)
// =============================================================================
#define HAS_SENS
#ifdef FILMLAB_V4
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
static const char *PRINT_NAMES[] = { "None", "Print2383", "Print2393", "Print3513", "Print3510", "Print2383Soft" };
#define FILMLAB_SOURCE "../Gradelux_FilmLab_v4.dctl"
// UI globals: reference configuration (defaults; Film Look / Film Print set per tile)
static int   ui_Look = 0, ui_Print = 0;
static float ui_CreativeIntensity = 100, ui_PrintStrength = 100, ui_PrintContrast = 0, ui_PrintDensity = 0,
             ui_PrintSaturation = 0, ui_PrintRolloff = 0;
static float ui_ColorDensity = 0, ui_Separation = 50, ui_HlWarmth = 0, ui_ShadowTone = 0;
static float ui_Density = 0, ui_SubSat = 0, ui_Contrast = 0, ui_Shadow = 0, ui_Rolloff = 0, ui_Intensity = 100;

static void filmlab_reset(void) {}
#else   // Version 9 (18 film stocks; shared shim with every control at its default)
#include "filmlab_shim.h"
#ifndef FILMLAB_SOURCE
#define FILMLAB_SOURCE "../Gradelux_FilmLab_v9.dctl"
#endif
#endif

#include FILMLAB_SOURCE
// ---- content: linear Rec.709 scene colours ----
static float s2l(float c){ c/=255.f; return c<=0.04045f? c/12.92f : powf((c+0.055f)/1.055f,2.4f); }
static const unsigned char MAC[18][3]={{115,82,68},{194,150,130},{98,122,157},{87,108,67},{133,128,177},{103,189,170},
 {214,126,44},{80,91,166},{193,90,99},{94,60,108},{157,188,64},{224,163,46},{56,61,150},{70,148,73},{175,54,60},{231,199,31},{187,86,149},{8,133,161}};
static const unsigned char SKIN[6][3]={{255,224,204},{241,194,167},{224,172,140},{194,150,130},{141,85,36},{92,58,40}};
#define NMAX 400
static float C709[NMAX][3]; static int NC=0, kind[NMAX]; // 0 colour 1 skin 2 grey
static void add(float r,float g,float b,int k){ C709[NC][0]=r;C709[NC][1]=g;C709[NC][2]=b;kind[NC++]=k; }
static void build(void){
  for(int e=-1;e<=1;e++){ float m=powf(2.f,(float)e);
    for(int i=0;i<18;i++) add(s2l(MAC[i][0])*m,s2l(MAC[i][1])*m,s2l(MAC[i][2])*m,0);
    for(int i=0;i<6;i++) add(s2l(SKIN[i][0])*m*0.8f,s2l(SKIN[i][1])*m*0.8f,s2l(SKIN[i][2])*m*0.8f,1);
    for(int h=0;h<24;h++){ float H=h/4.f, f=H-floorf(H); int k=(int)H; float s=0.6f,v=0.35f*m; float p=1-s,q=1-s*f,t=1-s*(1-f);
      float rr[]={1,q,p,p,t,1},gg[]={t,1,1,q,p,p},bb[]={p,p,t,1,1,q}; add(rr[k]*v,gg[k]*v,bb[k]*v,0);} }
  for(int i=0;i<11;i++){ float v=0.18f*powf(2.f,-5+i); add(v,v,v,2); }
}
// 709 lin -> work (2020) lin
static float3 to_work(const float*c){ return make_float3(0.6274040f*c[0]+0.3292820f*c[1]+0.0433136f*c[2],0.0690970f*c[0]+0.9195400f*c[1]+0.0113612f*c[2],0.0163916f*c[0]+0.0880132f*c[1]+0.8955950f*c[2]); }
// work lin -> display Lab (DRT for viewing: filmic curve on linear 709)
static void to_lab(float3 w,double*L){
  float3 d=glx_work_to_dwg(w.x,w.y,w.z);
  double l[3]={ 1.8986148993*d.x-0.7921761834*d.y-0.1064387159*d.z, -0.1689487865*d.x+1.4889757541*d.y-0.3200269676*d.z, -0.1215391606*d.x-0.3156758531*d.y+1.4372150137*d.z};
  for(int c=0;c<3;c++){ double x=fmax(l[c]*0.8,0); l[c]=fmin(fmax((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0),1); }  // display linear
  double X=0.4124*l[0]+0.3576*l[1]+0.1805*l[2], Y=0.2126*l[0]+0.7152*l[1]+0.0722*l[2], Z=0.0193*l[0]+0.1192*l[1]+0.9505*l[2];
  double xn=0.95047,zn=1.08883; double f[3]={X/xn,Y,Z/zn};
  for(int c=0;c<3;c++) f[c]= f[c]>0.008856? cbrt(f[c]) : 7.787*f[c]+16.0/116;
  L[0]=116*f[1]-16; L[1]=500*(f[0]-f[1]); L[2]=200*(f[1]-f[2]); }
static double dE(const double*a,const double*b){ return sqrt((a[0]-b[0])*(a[0]-b[0])+(a[1]-b[1])*(a[1]-b[1])+(a[2]-b[2])*(a[2]-b[2])); }
// stage toggles
enum { ST_SENS=1, ST_COLOR=2, ST_SUBSAT=4, ST_TONE=8, ST_DYE=16, ST_FADE=32, ST_ALL=63 };
static float3 look_stages(float3 c, GlxLook s, int mask){
  // Uses the DCTL's own stage function, so every stock family (colour and
  // monochrome) is measured exactly as the DCTL renders it.
  GlxProfile p=glx_profile_base();
  int parts = ((mask & ST_COLOR) ? 1 : 0) | ((mask & ST_TONE) ? 2 : 0);
  return glx_look_apply_parts(c.x,c.y,c.z,s,p,parts); }
static double LAB[8][NMAX][3];
static void eval_set(int slot, GlxLook s, int mask, int print){
  GlxProfile p=glx_profile_base(); GlxLook pr = print? glx_look_scale(glx_print_get(print),1.0f) : glx_look_clean();
  for(int i=0;i<NC;i++){ float3 w=to_work(C709[i]); float3 o = mask? look_stages(w,s,mask) : w;
    if(print) o=glx_print_apply(o.x,o.y,o.z,pr,p);
    to_lab(o,LAB[slot][i]); } }
static void report(const char*name,int slot,int ref){
  double s[3]={0},n[3]={0}, mx=0; for(int i=0;i<NC;i++){ double d=dE(LAB[slot][i],LAB[ref][i]); s[kind[i]]+=d; n[kind[i]]++; mx=fmax(mx,d);} 
  printf("  %-34s colours %5.2f  skin %5.2f  greys %5.2f  (max %5.1f)\n",name,s[0]/n[0],s[1]/n[1],s[2]/n[2],mx); }
int main(int argc,char**argv){
  filmlab_reset(); build(); int nl=GLX_LOOK_COUNT;
  eval_set(0,glx_look_clean(),0,0);   // Off
  printf("Visible change vs Off (mean CIELAB dE after a display transform; ~2 just noticeable, 10+ obvious)\n");
  for(int id=1; id<nl; id++){ GlxLook s=glx_look_scale(glx_look_get(id),1.0f);
    printf("%s\n",LOOK_NAMES[id]);
    eval_set(1,s,ST_ALL,0); report("full look",1,0);
    eval_set(2,s,ST_SENS|ST_COLOR|ST_SUBSAT|ST_DYE|ST_FADE,0); report("colour stages only (no tone)",2,0);
    eval_set(3,s,ST_TONE,0); report("tone stage only",3,0);
    eval_set(6,s,ST_ALL,1); eval_set(7,glx_look_clean(),0,1); report("look + print 2383 vs print alone",6,7);
  }
  printf("Distinctness between looks (mean dE, full looks; lower-left: alone, upper-right: both over print 1)\n%-15s","");
  for(int b=1;b<nl;b++) printf(" %8.8s",LOOK_NAMES[b]);
  printf("\n");
  static double M[32][32][2];
  for(int a=1;a<nl;a++) for(int b=a+1;b<nl;b++) for(int pr=0;pr<2;pr++){
    eval_set(1,glx_look_scale(glx_look_get(a),1.0f),ST_ALL,pr); eval_set(2,glx_look_scale(glx_look_get(b),1.0f),ST_ALL,pr);
    double s=0; for(int i=0;i<NC;i++) s+=dE(LAB[1][i],LAB[2][i]); M[a][b][pr]=M[b][a][pr]=s/NC; }
  double mn=1e9; int ma=0,mb=0;
  for(int a=1;a<nl;a++){ printf("%-15s",LOOK_NAMES[a]); for(int b=1;b<nl;b++){ if(a==b) printf(" %8s","-"); else printf(" %8.2f", a>b? M[a][b][0] : M[a][b][1]); if(a<b && M[a][b][0]<mn){mn=M[a][b][0];ma=a;mb=b;} } printf("\n"); }
  printf("Closest pair: %s vs %s, dE %.2f\n",LOOK_NAMES[ma],LOOK_NAMES[mb],mn);
  return 0; }
