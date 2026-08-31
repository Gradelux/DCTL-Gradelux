#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct { float x, y, z; } float3;
static float3 make_float3(float a,float b,float c){ float3 v; v.x=a;v.y=b;v.z=c; return v; }
#define __DEVICE__ static
static float _fabs(float x){ return fabsf(x); }
static float _fmaxf(float a,float b){ return fmaxf(a,b); }
static float _fminf(float a,float b){ return fminf(a,b); }
static float _clampf(float x,float a,float b){ return fminf(fmaxf(x,a),b); }
static float _saturatef(float x){ return fminf(fmaxf(x,0.0f),1.0f); }
static float _expf(float x){ return expf(x); }
static float _logf(float x){ return logf(x); }
static float _sqrtf(float x){ return sqrtf(x); }
static float _powf(float a,float b){ return powf(a,b); }
static float _copysignf(float a,float b){ return copysignf(a,b); }
#define DEFINE_UI_PARAMS(...)
/* texture stand-in: a small synthetic frame with a bright disc in the middle */
#define W 240
#define H 160
typedef struct { float p[H][W]; } Tex;
#define __TEXTURE__ const Tex *
static Tex TR, TG, TB;
static float _tex2D(const Tex *t, int x, int y){
    if(x<0)x=0; if(x>=W)x=W-1; if(y<0)y=0; if(y>=H)y=H-1; return t->p[y][x]; }
enum { CC_ENC_DI=0, CC_ENC_LIN=1 };
static int gInputEncoding=CC_ENC_DI, gBypass=0;
static float gExposure=0,gTemperature=0,gTint=0,gContrast=1,gPivot=0.336f,gSaturation=1;
static float gBlackPoint=0,gWhitePoint=0,gHighlightShoulder=0,gHighlightRollOff=0.5f;
static float gShadowToe=0,gShadowDepth=0,gFilmDensity=0,gDensityStrength=0.5f;
static float gSubSaturation=0,gRichness=0,gColorSeparation=0;
static float gRedDensity=0,gOrangeDensity=0,gYellowDensity=0,gGreenDensity=0;
static float gCyanDensity=0,gBlueDensity=0,gMagentaDensity=0;
static float gWarmHighlights=0,gCoolShadows=0,gSplitBalance=0,gBleachBypass=0,gBleachMix=1;
static float gHalation=0,gHalationWidth=0.5f,gHalationThresh=0.45f;
static float gBloom=0,gBloomWidth=0.5f,gBloomThresh=0.50f;
static float gVignette=0,gVignetteSize=0.5f,gVignetteSoft=0.5f,gMatte=0;
#include "CineCore.dctl"

static int fails=0;
static void check(const char*n,int ok,const char*i){ printf(ok?"  PASS  %s%s%s\n":"  FAIL  %s%s%s\n",n,i[0]?"   ":"",i); if(!ok)fails++; }
static void reset(void){ gExposure=0;gTemperature=0;gTint=0;gContrast=1;gPivot=0.336f;gSaturation=1;
  gBlackPoint=0;gWhitePoint=0;gHighlightShoulder=0;gHighlightRollOff=0.5f;gShadowToe=0;gShadowDepth=0;
  gFilmDensity=0;gDensityStrength=0.5f;gSubSaturation=0;gRichness=0;gColorSeparation=0;
  gRedDensity=0;gOrangeDensity=0;gYellowDensity=0;gGreenDensity=0;gCyanDensity=0;gBlueDensity=0;
  gMagentaDensity=0;gWarmHighlights=0;gCoolShadows=0;gSplitBalance=0;gBleachBypass=0;gBleachMix=1;
  gHalation=0;gHalationWidth=0.5f;gHalationThresh=0.45f;gBloom=0;gBloomWidth=0.5f;gBloomThresh=0.5f;
  gVignette=0;gVignetteSize=0.5f;gVignetteSoft=0.5f;gMatte=0;gBypass=0; }
static float3 T(int x,int y){ return transform(W,H,x,y,&TR,&TG,&TB); }
static float md(float3 a,float3 b){ return fmaxf(fmaxf(fabsf(a.x-b.x),fabsf(a.y-b.y)),fabsf(a.z-b.z)); }
static float3 IN(int x,int y){ return make_float3(TR.p[y][x],TG.p[y][x],TB.p[y][x]); }

int main(void){
    char buf[160];
    /* dim background, bright disc at centre */
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        float dx=(x-W/2)/12.0f, dy=(y-H/2)/12.0f;
        float bright = (dx*dx+dy*dy < 1.0f) ? 0.85f : 0.16f;
        TR.p[y][x]=bright; TG.p[y][x]=bright*0.92f; TB.p[y][x]=bright*0.86f; }

    printf("--- defaults ---\n");
    reset(); float worst=0;
    for(int y=0;y<H;y+=7) for(int x=0;x<W;x+=7) worst=fmaxf(worst,md(T(x,y),IN(x,y)));
    sprintf(buf,"max delta %.3e",worst);
    check("defaults return the input exactly", worst==0.0f, buf);

    printf("\n--- halation ---\n");
    int nearX=W/2+16, nearY=H/2;      /* just outside the disc */
    int farX=8, farY=8;               /* far corner, no bright light nearby */
    reset(); float3 b1=T(nearX,nearY), b2=T(farX,farY);
    reset(); gHalation=1.0f;
    float3 h1=T(nearX,nearY), h2=T(farX,farY);
    sprintf(buf,"near bright light %.4f, far away %.4f", md(h1,b1), md(h2,b2));
    check("halation glows next to bright areas, not far from them",
          md(h1,b1)>0.004f && md(h2,b2)<md(h1,b1)*0.25f, buf);
    check("halation only adds light, never subtracts", h1.x>=b1.x && h1.y>=b1.y && h1.z>=b1.z, "");
    sprintf(buf,"R %+.4f  G %+.4f  B %+.4f", h1.x-b1.x, h1.y-b1.y, h1.z-b1.z);
    check("halation is red-weighted", (h1.x-b1.x) > (h1.y-b1.y) && (h1.y-b1.y) > (h1.z-b1.z), buf);

    printf("\n--- bloom ---\n");
    /* bloom's radius is tight by design: on this 240x160 test frame it is under
       2 px, so it must be probed right at the edge of the bright area rather
       than 4 px away, or the test measures the probe distance. */
    int bx=W/2+13, by=H/2;
    reset(); float3 c1=T(bx,by);
    reset(); gBloom=1.0f; float3 d1=T(bx,by);
    sprintf(buf,"delta %.4f",md(d1,c1));
    check("bloom lifts next to bright areas", md(d1,c1)>0.003f, buf);
    sprintf(buf,"R %+.4f  G %+.4f  B %+.4f", d1.x-c1.x, d1.y-c1.y, d1.z-c1.z);
    check("bloom is achromatic", fabsf((d1.x-c1.x)-(d1.z-c1.z))<1e-5f, buf);

    printf("\n--- vignette ---\n");
    reset(); float3 e1=T(2,2), e2=T(W/2,H/2);
    reset(); gVignette=1.0f; float3 f1=T(2,2), f2=T(W/2,H/2);
    sprintf(buf,"corner %+.4f, centre %+.4f", f1.x-e1.x, f2.x-e2.x);
    check("vignette darkens corners and leaves centre alone",
          f1.x<e1.x-0.01f && fabsf(f2.x-e2.x)<1e-6f, buf);
    check("vignette is achromatic", fabsf((f1.x-e1.x)-(f1.z-e1.z))<1e-5f, "");
    reset(); gVignette=-1.0f; float3 g1=T(2,2);
    check("negative vignette brightens corners", g1.x>e1.x+0.01f, "");
    /* symmetry across the frame */
    reset(); gVignette=1.0f;
    float s1=T(2,2).x, s2=T(W-3,2).x, s3=T(2,H-3).x, s4=T(W-3,H-3).x;
    sprintf(buf,"spread %.2e", fmaxf(fmaxf(s1,s2),fmaxf(s3,s4))-fminf(fminf(s1,s2),fminf(s3,s4)));
    check("vignette is symmetric in all four corners",
          fmaxf(fmaxf(s1,s2),fmaxf(s3,s4))-fminf(fminf(s1,s2),fminf(s3,s4)) < 1e-5f, buf);

    printf("\n--- matte ---\n");
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){ float v=(float)x/(W-1);
        TR.p[y][x]=v; TG.p[y][x]=v; TB.p[y][x]=v; }
    reset(); float3 dark=T(10,H/2), mid=T(W/2,H/2), brt=T(W-10,H/2);
    reset(); gMatte=1.0f; float3 dark2=T(10,H/2), mid2=T(W/2,H/2), brt2=T(W-10,H/2);
    sprintf(buf,"shadow %+.4f  mid %+.4f  highlight %+.4f",
            dark2.x-dark.x, mid2.x-mid.x, brt2.x-brt.x);
    check("matte lifts shadows and softens highlights", dark2.x>dark.x+0.005f && brt2.x<brt.x, buf);
    check("matte is achromatic", fabsf((dark2.x-dark.x)-(dark2.z-dark.z))<1e-5f, "");
    /* monotonic ramp preserved */
    reset(); gMatte=1.0f; int nm=0; float prev=-9;
    for(int x=0;x<W;x++){ float v=T(x,H/2).x; if(v<prev-1e-6f) nm++; prev=v; }
    sprintf(buf,"%d reversals",nm);
    check("matte keeps the ramp monotonic", nm==0, buf);

    printf("\n--- stability ---\n");
    int bad=0;
    reset(); gHalation=1;gBloom=1;gVignette=1;gMatte=1;gContrast=2.5f;gExposure=5;
    gFilmDensity=1;gSubSaturation=1;gRichness=1;gColorSeparation=1;gBleachBypass=1;
    gHighlightShoulder=1;gShadowToe=1;gShadowDepth=1;gSaturation=2;
    for(int y=0;y<H;y+=3) for(int x=0;x<W;x+=3){ float3 o=T(x,y);
        if(o.x!=o.x||o.y!=o.y||o.z!=o.z||isinf(o.x)||isinf(o.y)||isinf(o.z)) bad++; }
    sprintf(buf,"%d non-finite",bad);
    check("no NaN or Inf with every effect at maximum", bad==0, buf);
    /* edge safety: no bright rim from out-of-frame taps */
    reset(); gHalation=1.0f;
    float rim=0; for(int y=0;y<H;y++){ rim=fmaxf(rim,fabsf(T(0,y).x-T(1,y).x)); }
    sprintf(buf,"largest edge step %.4f",rim);
    check("no edge artefact from clamped sampling", rim<0.02f, buf);

    printf("\n%s\n", fails?"FAILURES PRESENT":"ALL CHECKS PASSED");
    return fails?1:0; }
