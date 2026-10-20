// =============================================================================
//  Gradelux FilmLab — synthetic scenario checks (Version 9)
// -----------------------------------------------------------------------------
//  Runs every film stock x every print through the DCTL's own maths on
//  SYNTHETIC scene sets that stand in for the footage types a stock must
//  survive. This checks numerical behaviour; it is not a substitute for
//  looking at real footage in Resolve.
//
//    portrait     6 skin tones x 9 exposures, hair, lips, eyes
//    daylight     sky gradients, very bright clouds, grass, concrete, blue shade
//    foliage      greens 60-170 deg hue, 3 saturations, -4..+1 stops
//    interior     tungsten-lit scene (warm cast), skin under tungsten, lamps
//    night        ambient -9..-4 stops; amber / red / sodium practicals and
//                 blue / magenta / green / cyan neon at -1..+4 stops
//    saturated    Rec.2020 primaries / secondaries (spectral edge), saturated Rec.709
//    DWG-edge     DWG primaries (outside the spectral locus): reported for
//                 information only, not counted as failures
//    contrast     -10..+10 stop ramps (neutral and coloured), speculars
//
//  Per scene it reports, summed over all prints: NaN/Inf, negative output
//  channels from non-negative input, brightness reversals along exposure
//  series, hue jumps (>12 deg between 2-deg neighbours with >4% chroma), the worst skin
//  hue shift (skin at -2..+2 stops with real chroma), and the change in
//  display-clipped samples vs Off. VERBOSE=<stock index> lists each flagged sample.
//
//    gcc -O2 -o scenario_check tools/scenario_check.c -lm && ./scenario_check
// =============================================================================
#define HAS_SENS
#ifdef FILMLAB_V4   // legacy Version 4 build: its own enumerations and controls
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


// UI globals: reference configuration (Film Look / Film Print / Creative Intensity set per run)
static int   ui_Look = 0, ui_Print = 0;
static float ui_CreativeIntensity = 100, ui_PrintStrength = 100, ui_PrintContrast = 0, ui_PrintDensity = 0,
             ui_PrintSaturation = 0, ui_PrintRolloff = 0;
static float ui_ColorDensity = 0, ui_Separation = 50, ui_HlWarmth = 0, ui_ShadowTone = 0;
static float ui_Density = 0, ui_SubSat = 0, ui_Contrast = 0, ui_Shadow = 0, ui_Rolloff = 0, ui_Intensity = 100;
static void filmlab_reset(void) {}
#else               // Version 9: shared shim (every control at its DCTL default)
#include "filmlab_shim.h"
#ifndef FILMLAB_SOURCE
#define FILMLAB_SOURCE "../Gradelux_FilmLab_v9.dctl"
#endif
#endif

#include FILMLAB_SOURCE
// ---- helpers ----
static float3 to_work(const float*c){ return make_float3(0.6274040f*c[0]+0.3292820f*c[1]+0.0433136f*c[2],0.0690970f*c[0]+0.9195400f*c[1]+0.0113612f*c[2],0.0163916f*c[0]+0.0880132f*c[1]+0.8955950f*c[2]); }
static float3 run_dwg(float3 d){   // linear DWG in -> linear DWG out, through the full DCTL
  float3 o=transform(0,0,0,0,glx_lin_to_di(d.x),glx_lin_to_di(d.y),glx_lin_to_di(d.z));
  return make_float3(glx_di_to_lin(o.x),glx_di_to_lin(o.y),glx_di_to_lin(o.z)); }
static void disp(float3 d,double*l){ // linear DWG -> display-linear Rec.709 (viewing only)
  double x[3]={ 1.8986148993*d.x-0.7921761834*d.y-0.1064387159*d.z, -0.1689487865*d.x+1.4889757541*d.y-0.3200269676*d.z, -0.1215391606*d.x-0.3156758531*d.y+1.4372150137*d.z};
  for(int c=0;c<3;c++){ double v=fmax(x[c]*0.8,0); l[c]=fmin(fmax((v*(2.51*v+0.03))/(v*(2.43*v+0.59)+0.14),0),1);} }
static double hue_of(float3 c){ return atan2((c.y-c.z)*0.70710678,(2*c.x-c.y-c.z)*0.40824829)*57.2957795; }
static double hdiff(double a,double b){ double d=a-b; while(d>180)d-=360; while(d<-180)d+=360; return d; }
static double Y(float3 d){ return 0.27411851*d.x+0.8736319*d.y-0.14775041*d.z; } // DWG luminance (CIE Y)

// ---- scene sets: series of samples in linear DWG; a series is an exposure ladder
// (brightness must not reverse) or a hue sweep (hue must not jump) ----
#define MAXS 4096
typedef struct { float3 v[MAXS]; int series[MAXS]; int kind[MAXS]; float ev[MAXS]; int n; } Scene; // kind 0 ladder, 1 sweep, 2 skin ladder
static Scene SC[8]; static const char*SCN[8]={"portrait","daylight","foliage","interior","night","saturated","contrast","DWG-edge (info)"};
static float g_ev = 0.0f;   // exposure (stops) of the sample being added, for the skin metric
static void addw(Scene*s,float3 w,int ser,int k){ s->v[s->n]=glx_work_to_dwg(w.x,w.y,w.z); s->series[s->n]=ser; s->kind[s->n]=k; s->ev[s->n]=g_ev; s->n++; }
static void add709(Scene*s,float r,float g,float b,int ser,int k){ float c[3]={r,g,b}; addw(s,to_work(c),ser,k); }
static float s2l(float c){ c/=255.f; return c<=0.04045f? c/12.92f : powf((c+0.055f)/1.055f,2.4f); }
static void hsv(float h,float s,float v,float*o){ h=fmodf(h,360)/60; int k=(int)h; float f=h-k,p=v*(1-s),q=v*(1-s*f),t=v*(1-s*(1-f));
  float R[6]={v,q,p,p,t,v},G[6]={t,v,v,q,p,p},B[6]={p,p,t,v,v,q}; o[0]=R[k%6];o[1]=G[k%6];o[2]=B[k%6]; }
static void build(void){
  static const unsigned char SKIN[6][3]={{255,224,204},{241,194,167},{224,172,140},{194,150,130},{141,85,36},{92,58,40}};
  int ser=0; Scene*s;
  s=&SC[0]; for(int i=0;i<6;i++,ser++) for(int e=-8;e<=8;e++){ float m=0.75f*powf(2,e*0.5f); g_ev=e*0.5f; add709(s,s2l(SKIN[i][0])*m,s2l(SKIN[i][1])*m,s2l(SKIN[i][2])*m,ser,2); }
  float extra[4][3]={{0.04f,0.025f,0.015f},{0.35f,0.06f,0.08f},{0.10f,0.16f,0.22f},{0.012f,0.010f,0.009f}};
  for(int i=0;i<4;i++,ser++) for(int e=-4;e<=4;e++){ float m=powf(2,e*0.5f); add709(s,extra[i][0]*m,extra[i][1]*m,extra[i][2]*m,ser,0); }
  s=&SC[1]; for(int i=0;i<=40;i++){ float c[3]; hsv(205+i*0.5f,0.15f+0.012f*i,0.25f+0.03f*i,c); add709(s,c[0],c[1],c[2],ser,1);} ser++;
  for(int e=0;e<=24;e++) { add709(s,0.9f*powf(2,e*0.25f),0.92f*powf(2,e*0.25f),0.95f*powf(2,e*0.25f),ser,0); }
  ser++;
  float day[5][3]={{0.10f,0.22f,0.04f},{0.35f,0.33f,0.30f},{0.03f,0.045f,0.08f},{0.40f,0.28f,0.16f},{0.08f,0.14f,0.30f}};
  for(int i=0;i<5;i++,ser++) for(int e=-6;e<=6;e++){ float m=powf(2,e*0.5f); add709(s,day[i][0]*m,day[i][1]*m,day[i][2]*m,ser,0); }
  s=&SC[2]; for(int sat=0;sat<3;sat++) for(int e=-4;e<=1;e++,ser++) for(int h=60;h<=170;h+=2){ float c[3]; hsv((float)h,0.35f+0.25f*sat,0.18f*powf(2,(float)e),c); add709(s,c[0],c[1],c[2],ser,1); }
  for(int h=60;h<=170;h+=10,ser++) for(int e=-8;e<=2;e++){ float c[3]; hsv((float)h,0.6f,0.18f*powf(2,e*0.5f),c); add709(s,c[0],c[1],c[2],ser,0); }
  s=&SC[3]; { float T[3]={1.0f,0.72f,0.42f};
    for(int i=0;i<6;i++,ser++) for(int e=-6;e<=4;e++){ float m=0.6f*powf(2,e*0.5f); g_ev=e*0.5f; add709(s,s2l(SKIN[i][0])*m*T[0],s2l(SKIN[i][1])*m*T[1],s2l(SKIN[i][2])*m*T[2],ser,2); }
    float obj[5][3]={{0.18f,0.18f,0.18f},{0.30f,0.12f,0.06f},{0.10f,0.12f,0.20f},{0.08f,0.20f,0.10f},{0.50f,0.45f,0.40f}};
    for(int i=0;i<5;i++,ser++) for(int e=-8;e<=4;e++){ float m=powf(2,e*0.5f); add709(s,obj[i][0]*m*T[0],obj[i][1]*m*T[1],obj[i][2]*m*T[2],ser,0); }
    for(int e=0;e<=20;e++) { add709(s,1.0f*powf(2,e*0.25f),0.62f*powf(2,e*0.25f),0.25f*powf(2,e*0.25f),ser,0); }
    ser++; }
  s=&SC[4]; { float amb[3][3]={{0.004f,0.005f,0.008f},{0.006f,0.006f,0.006f},{0.003f,0.006f,0.009f}};
    for(int i=0;i<3;i++,ser++) for(int e=-12;e<=4;e++){ float m=powf(2,e*0.5f); add709(s,amb[i][0]*m,amb[i][1]*m,amb[i][2]*m,ser,0); }
    float pr[7][3]={{1.0f,0.45f,0.08f},{1.0f,0.12f,0.06f},{1.0f,0.75f,0.30f},{0.15f,0.35f,1.0f},{1.0f,0.10f,0.80f},{0.10f,1.0f,0.30f},{0.10f,0.90f,1.0f}};
    for(int i=0;i<7;i++,ser++) for(int e=-4;e<=16;e++){ float m=0.18f*powf(2,e*0.25f); add709(s,pr[i][0]*m,pr[i][1]*m,pr[i][2]*m,ser,0); }
    for(int e=-1;e<=3;e++,ser++) for(int h=0;h<360;h+=2){ float c[3]; hsv((float)h,0.9f,0.18f*powf(2,(float)e),c); add709(s,c[0],c[1],c[2],ser,1); } }
  s=&SC[5]; { float P[6][3]={{1,0,0},{0,1,0},{0,0,1},{1,1,0},{0,1,1},{1,0,1}};   // Rec.2020 primaries / secondaries (real, spectral-edge colours)
    for(int i=0;i<6;i++,ser++) for(int e=-8;e<=8;e++){ float m=0.18f*powf(2,e*0.5f); addw(s,make_float3(P[i][0]*m+0.002f,P[i][1]*m+0.002f,P[i][2]*m+0.002f),ser,0); }
    for(int e=-2;e<=2;e++,ser++) for(int h=0;h<360;h+=2){ float c[3]; hsv((float)h,1.0f,0.18f*powf(2,(float)e),c); add709(s,c[0],c[1],c[2],ser,1); } }
  s=&SC[7]; { float P[6][3]={{1,0,0},{0,1,0},{0,0,1},{1,1,0},{0,1,1},{1,0,1}};   // DWG primaries: outside the spectral locus (imaginary)
    for(int i=0;i<6;i++,ser++) for(int e=-8;e<=8;e++){ float m=0.18f*powf(2,e*0.5f);
      s->v[s->n]=make_float3(P[i][0]*m+0.002f,P[i][1]*m+0.002f,P[i][2]*m+0.002f); s->series[s->n]=ser; s->kind[s->n]=0; s->ev[s->n]=0; s->n++; } }
  s=&SC[6]; { float col[4][3]={{1,1,1},{1.0f,0.6f,0.4f},{0.4f,0.6f,1.0f},{0.5f,1.0f,0.5f}};
    for(int i=0;i<4;i++,ser++) for(int e=-40;e<=40;e++){ float m=0.18f*powf(2,e*0.25f); add709(s,col[i][0]*m,col[i][1]*m,col[i][2]*m,ser,0); }
    for(int e=0;e<=12;e++) { add709(s,16*powf(2,e*0.5f),16*powf(2,e*0.5f),16*powf(2,e*0.5f),ser,0); }
    ser++; }
}
int main(int argc,char**argv){
    filmlab_reset();
  build(); int fails=0; const char*vb=getenv("VERBOSE"); int vlook = vb? atoi(vb) : -1;
  float ci = argc>1 ? (float)atof(argv[1]) : 100.0f;
  printf("Synthetic scenario checks at Creative Intensity %.0f%%, summed over all 6 print options\n",ci);
  printf("cells: nan/neg/rev/jump | skin = worst skin hue shift (deg) | clip = change in display-clipped samples vs Off (%%)\n\n");
  printf("%-15s","stock"); for(int k=0;k<8;k++) printf(" %-21s",SCN[k]); printf("\n");
  static double clipOff[8];
  for(int l=0;l<GLX_LOOK_COUNT;l++){
    printf("%-15s",LOOK_NAMES[l]);
    for(int k=0;k<8;k++){ Scene*s=&SC[k]; int nan=0,neg=0,rev=0,jump=0,clip=0,tot=0; double skin=0;
      for(int pr=0;pr<GLX_PRINT_COUNT;pr++){ ui_Look=l; ui_Print=pr; ui_CreativeIntensity=ci;
        float3 prev=make_float3(0,0,0); int prevSer=-1;
        for(int i=0;i<s->n;i++){ float3 in=s->v[i], o=run_dwg(in);
          if(!isfinite(o.x)||!isfinite(o.y)||!isfinite(o.z)){nan++;continue;}
          if(in.x>=0&&in.y>=0&&in.z>=0&&(o.x<-1e-4f||o.y<-1e-4f||o.z<-1e-4f)){ neg++; if(l==vlook) printf("  [%s pr%d #%d] negative %.5g %.5g %.5g -> %.5g %.5g %.5g\n",SCN[k],pr,i,in.x,in.y,in.z,o.x,o.y,o.z); }
          double dl[3]; disp(o,dl); if(dl[0]>=0.999||dl[1]>=0.999||dl[2]>=0.999) clip++; tot++;
          if(s->series[i]==prevSer){
            if(s->kind[i]!=1 && Y(o) < Y(prev)*(1-1e-4)-1e-7){ rev++; if(l==vlook) printf("  [%s pr%d #%d] reversal in %.5g %.5g %.5g -> Y %.6g < %.6g\n",SCN[k],pr,i,in.x,in.y,in.z,Y(o),Y(prev)); }
            if(s->kind[i]==1){ float mx=fmaxf(o.x,fmaxf(o.y,o.z)),mn=fminf(o.x,fminf(o.y,o.z));
              if((mx-mn)>0.04f*mx && fabs(hdiff(hue_of(o),hue_of(prev)))>12){ jump++; if(l==vlook) printf("  [%s pr%d #%d] hue jump %.1f -> %.1f (in %.4g %.4g %.4g, chroma %.3f)\n",SCN[k],pr,i,hue_of(prev),hue_of(o),in.x,in.y,in.z,(mx-mn)/mx); } } }
          if(s->kind[i]==2 && fabsf(s->ev[i])<=2.0f && !glx_look_get(l).isMono){   // skin as skin: -2..+2 stops, real chroma
            float mx=fmaxf(o.x,fmaxf(o.y,o.z)),mn=fminf(o.x,fminf(o.y,o.z));
            if(mx-mn>0.05f*mx){ double d=hdiff(hue_of(o),hue_of(in)); if(fabs(d)>fabs(skin)) skin=d; } }
          prev=o; prevSer=s->series[i]; } }
      double cp=100.0*clip/tot; if(l==0) clipOff[k]=cp;
      int bad=nan+neg+rev+jump; if(bad && k<7) fails++;   // DWG-edge column is informational
      char cell[64]; if(k==0||k==3) snprintf(cell,64,"%d/%d/%d/%d sk%+.1f",nan,neg,rev,jump,skin); else snprintf(cell,64,"%d/%d/%d/%d c%+.1f",nan,neg,rev,jump,cp-clipOff[k]);
      printf(" %-21s",cell); }
    printf("\n"); }
  printf("\n%s\n", fails? "SCENARIO FAILURES PRESENT" : "ALL SCENARIO CHECKS PASSED (no NaN/Inf, negatives, brightness reversals or hue jumps)");
  return fails!=0; }
