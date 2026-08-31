/* Minimal stand-in for the DCTL environment, used only to syntax-check the
   .dctl as plain C. float3 is a bare struct with no operators, so if the file
   compiles here it uses no vector arithmetic that a backend might not support. */
#include <math.h>
#include <stdio.h>

typedef struct { float x, y, z; } float3;
static float3 make_float3(float a, float b, float c){ float3 v; v.x=a; v.y=b; v.z=c; return v; }

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

/* UI params become plain globals; the macro itself is discarded. */
#define DEFINE_UI_PARAMS(...)
enum { CC_ENC_DI = 0, CC_ENC_LIN = 1 };
static int   gInputEncoding = CC_ENC_DI;
static int   gBypass = 0;
static float gExposure=0.0f, gTemperature=0.0f, gTint=0.0f, gContrast=1.0f;
static float gPivot=0.336f, gSaturation=1.0f, gBlackPoint=0.0f, gWhitePoint=0.0f;
static float gHighlightShoulder=0.0f, gHighlightRollOff=0.5f;
static float gShadowToe=0.0f, gShadowDepth=0.0f;
static float gFilmDensity=0.0f, gDensityStrength=0.5f, gSubSaturation=0.0f;
static float gRichness=0.0f, gColorSeparation=0.0f;
static float gRedDensity=0.0f, gOrangeDensity=0.0f, gYellowDensity=0.0f;
static float gGreenDensity=0.0f, gCyanDensity=0.0f, gBlueDensity=0.0f, gMagentaDensity=0.0f;
static float gWarmHighlights=0.0f, gCoolShadows=0.0f, gSplitBalance=0.0f;
static float gBleachBypass=0.0f, gBleachMix=1.0f;
/* Stand-ins for the Resolve LUT macros: enough to check the C structure.
   Resolve's real DEFINE_LUT loads the .cube at compile time. */


enum { CC_LOOK_REFERENCE=0, CC_LOOK_MERIDIAN, CC_LOOK_EMBER, CC_LOOK_HALIDE, CC_LOOK_VERDANCE, CC_LOOK_CINDER, CC_LOOK_SOLSTICE, CC_LOOK_TUNDRA, CC_LOOK_BASALT, CC_LOOK_AURIC, CC_LOOK_NOCTURNE, CC_LOOK_LUMEN, CC_LOOK_PRALINE, CC_LOOK_COBALT, CC_LOOK_SABLE, CC_LOOK_MARIGOLD, CC_LOOK_SLATE, CC_LOOK_VERMILLION, CC_LOOK_DRIFTWOOD, CC_LOOK_APERTURE };
static int gLook = 0;
static float gLookMix = 1.0f;

#include "CineCore.dctl"


static const char *LOOKNAME[20] = {"Reference","Meridian","Ember","Halide","Verdance","Cinder",
  "Solstice","Tundra","Basalt","Auric","Nocturne","Lumen","Praline","Cobalt","Sable","Marigold",
  "Slate","Vermillion","Driftwood","Aperture"};

static int fails = 0;
static void check(const char *n, int ok, const char *info)
{ printf(ok?"  PASS  %s%s%s\n":"  FAIL  %s%s%s\n", n, info[0]?"   ":"", info); if(!ok) fails++; }

static void reset(void)
{ gExposure=0;gTemperature=0;gTint=0;gContrast=1;gPivot=0.336f;gSaturation=1;
  gBlackPoint=0;gWhitePoint=0;gHighlightShoulder=0;gHighlightRollOff=0.5f;
  gShadowToe=0;gShadowDepth=0;gFilmDensity=0;gDensityStrength=0.5f;gSubSaturation=0;
  gRichness=0;gColorSeparation=0;gRedDensity=0;gOrangeDensity=0;gYellowDensity=0;
  gGreenDensity=0;gCyanDensity=0;gBlueDensity=0;gMagentaDensity=0;gWarmHighlights=0;
  gCoolShadows=0;gSplitBalance=0;gBleachBypass=0;gBleachMix=1;gLook=0;gLookMix=1;gBypass=0; }

static float3 T(float r,float g,float b){ return transform(1920,1080,0,0,r,g,b); }
static float md(float3 a, float3 b){ float d=fabsf(a.x-b.x); d=fmaxf(d,fabsf(a.y-b.y));
  return fmaxf(d,fabsf(a.z-b.z)); }

int main(void)
{
    char buf[256];
    /* representative colours: neutral, skin, sky, foliage, saturated red */
    float3 P[5]; P[0]=make_float3(0.336f,0.336f,0.336f); P[1]=make_float3(0.3733f,0.3455f,0.3106f);
    P[2]=make_float3(0.30f,0.36f,0.45f); P[3]=make_float3(0.25f,0.34f,0.24f);
    P[4]=make_float3(0.4324f,0.2205f,0.1683f);

    printf("--- 1. defaults ---\n");
    reset(); float worst=0;
    for(int i=0;i<5;i++){ float3 o=T(P[i].x,P[i].y,P[i].z); worst=fmaxf(worst,md(o,P[i])); }
    sprintf(buf,"max delta %.3e",worst);
    check("default settings return the input exactly", worst==0.0f, buf);

    printf("\n--- 2. Reference Identity ---\n");
    reset(); gLook=CC_LOOK_REFERENCE; worst=0;
    for(int i=0;i<5;i++){ float3 o=T(P[i].x,P[i].y,P[i].z); worst=fmaxf(worst,md(o,P[i])); }
    sprintf(buf,"max delta %.3e",worst);
    check("Reference Identity is neutral", worst==0.0f, buf);
    reset(); gLook=CC_LOOK_REFERENCE; gLookMix=0.37f; worst=0;
    for(int i=0;i<5;i++){ float3 o=T(P[i].x,P[i].y,P[i].z); worst=fmaxf(worst,md(o,P[i])); }
    check("Reference Identity neutral at any Look Mix", worst==0.0f, "");

    printf("\n--- 3. every look changes the image ---\n");
    float mn=9e9f, mx=0; int weak=-1;
    for(int L=1;L<20;L++){
        reset(); gLook=L; float d=0;
        for(int i=0;i<5;i++){ float3 o=T(P[i].x,P[i].y,P[i].z); d=fmaxf(d,md(o,P[i])); }
        if(d<mn){mn=d;weak=L;} if(d>mx)mx=d;
    }
    sprintf(buf,"weakest %s at %.4f, strongest %.4f", LOOKNAME[weak], mn, mx);
    check("all 19 looks alter the image", mn>0.005f, buf);

    printf("\n--- 4. looks are distinct from each other ---\n");
    float3 sig[20][5]; 
    for(int L=0;L<20;L++){ reset(); gLook=L;
        for(int i=0;i<5;i++) sig[L][i]=T(P[i].x,P[i].y,P[i].z); }
    float closest=9e9f; int a1=0,b1=0;
    for(int a=1;a<20;a++) for(int b=a+1;b<20;b++){
        float d=0; for(int i=0;i<5;i++) d=fmaxf(d,md(sig[a][i],sig[b][i]));
        if(d<closest){closest=d;a1=a;b1=b;} }
    sprintf(buf,"closest pair %s / %s at %.4f", LOOKNAME[a1], LOOKNAME[b1], closest);
    check("no two looks are near-duplicates", closest>0.004f, buf);

    printf("\n--- 5. Look Mix interpolates ---\n");
    int bad=0; float worstmono=0;
    for(int L=1;L<20;L++){
        reset(); gLook=L; gLookMix=0.0f;
        float3 z=T(P[1].x,P[1].y,P[1].z);
        if(md(z,P[1])!=0.0f) bad++;
        float prev=-1;
        for(int k=0;k<=10;k++){ reset(); gLook=L; gLookMix=k/10.0f;
            float3 o=T(P[4].x,P[4].y,P[4].z); float d=md(o,P[4]);
            if(prev>=0 && d<prev-1e-4f) worstmono=fmaxf(worstmono,prev-d);
            prev=d; } }
    sprintf(buf,"%d looks non-neutral at mix 0",bad);
    check("Look Mix 0 is neutral for every look", bad==0, buf);
    sprintf(buf,"largest reversal %.2e",worstmono);
    check("Look Mix ramps monotonically from 0 to 1", worstmono<1e-3f, buf);

    printf("\n--- 6. every control works under every look ---\n");
    /* each control is probed on a pixel where it is SUPPOSED to act: highlight
       controls on a bright pixel, shadow controls on a dark one, colour
       controls on a saturated one. Probing a shoulder on a dark pixel measures
       the probe, not the control. */
    float3 BRIGHT = make_float3(0.78f, 0.70f, 0.62f);
    float3 DARK   = make_float3(0.11f, 0.09f, 0.075f);
    float3 SATRED = make_float3(0.4324f, 0.2205f, 0.1683f);
    /* A control counts as live if it moves ANY of a spread of pixels. Judging
       it on one probe measures the probe: a midtone-weighted control has
       nothing to do on a pixel a look has pushed out of the midtones, which is
       correct behaviour, not a dead control. */
    float3 PROBE[6] = { make_float3(0.11f,0.09f,0.075f), make_float3(0.22f,0.19f,0.17f),
                        make_float3(0.336f,0.30f,0.27f), make_float3(0.4324f,0.2205f,0.1683f),
                        make_float3(0.55f,0.48f,0.42f), make_float3(0.78f,0.70f,0.62f) };
    int dead=0; char deadname[128]="";
    for(int L=0;L<20;L++){
      for(int c=0;c<14;c++){
        float moved=0;
        for(int q=0;q<6;q++){
          reset(); gLook=L; float3 base=T(PROBE[q].x,PROBE[q].y,PROBE[q].z);
          reset(); gLook=L;
          switch(c){
            case 0: gExposure=1.0f; break;      case 1: gTemperature=0.5f; break;
            case 2: gTint=0.5f; break;          case 3: gContrast=1.4f; break;
            case 4: gSaturation=1.5f; break;    case 5: gBlackPoint=0.03f; break;
            case 6: gWhitePoint=0.1f; break;    case 7: gHighlightShoulder=0.7f; break;
            case 8: gShadowToe=0.6f; break;     case 9: gFilmDensity=0.7f; break;
            case 10: gSubSaturation=0.7f; break;case 11: gRichness=0.7f; break;
            case 12: gColorSeparation=0.8f; break; case 13: gBleachBypass=0.6f; break; }
          moved = fmaxf(moved, md(T(PROBE[q].x,PROBE[q].y,PROBE[q].z), base));
        }
        if(moved<1e-5f){ dead++;
          if(!deadname[0]) sprintf(deadname,"%s ctrl %d (moved %.2e)",LOOKNAME[L],c,moved); }
      } }
    sprintf(buf,"%d dead of %d combinations%s%s",dead,20*14,dead?", first: ":"",deadname);
    check("all 14 sampled controls live under all 20 looks", dead==0, buf);

    /* the remaining controls, probed on the colour they target */
    int dead2=0;
    for(int L=0;L<20;L++){
      float3 hues[7]={ make_float3(0.45f,0.20f,0.18f), make_float3(0.44f,0.30f,0.18f),
                       make_float3(0.44f,0.42f,0.18f), make_float3(0.20f,0.40f,0.20f),
                       make_float3(0.18f,0.40f,0.42f), make_float3(0.18f,0.22f,0.45f),
                       make_float3(0.42f,0.18f,0.40f) };
      for(int h=0;h<7;h++){
        reset(); gLook=L; float3 base=T(hues[h].x,hues[h].y,hues[h].z);
        reset(); gLook=L;
        float *hp[7]={&gRedDensity,&gOrangeDensity,&gYellowDensity,&gGreenDensity,
                      &gCyanDensity,&gBlueDensity,&gMagentaDensity};
        *hp[h]=0.8f;
        float3 o=T(hues[h].x,hues[h].y,hues[h].z);
        if(md(o,base)<1e-5f) dead2++;
      }
      reset(); gLook=L; float3 b2=T(BRIGHT.x,BRIGHT.y,BRIGHT.z);
      reset(); gLook=L; gWarmHighlights=0.8f;
      if(md(T(BRIGHT.x,BRIGHT.y,BRIGHT.z),b2)<1e-5f) dead2++;
      reset(); gLook=L; float3 b3=T(DARK.x,DARK.y,DARK.z);
      reset(); gLook=L; gCoolShadows=0.8f;
      if(md(T(DARK.x,DARK.y,DARK.z),b3)<1e-5f) dead2++;
    }
    sprintf(buf,"%d dead of %d",dead2,20*9);
    check("7 hue densities + split tone live under all 20 looks", dead2==0, buf);

    printf("\n--- 7. stability ---\n");
    int nan=0; 
    for(int L=0;L<20;L++){ reset(); gLook=L; gContrast=2.5f; gFilmDensity=1; gSubSaturation=1;
      gRichness=1; gColorSeparation=1; gBleachBypass=1; gHighlightShoulder=1; gShadowToe=1;
      gShadowDepth=1; gExposure=5; gSaturation=2;
      float v[7]={-0.3f,0.0f,0.15f,0.336f,0.5138f,0.9f,1.3f};
      for(int i=0;i<7;i++) for(int j=0;j<7;j++) for(int k=0;k<7;k++){
        float3 o=T(v[i],v[j],v[k]);
        if(o.x!=o.x||o.y!=o.y||o.z!=o.z||isinf(o.x)||isinf(o.y)||isinf(o.z)) nan++; } }
    sprintf(buf,"%d non-finite outputs over %d samples",nan,20*343);
    check("no NaN or Inf under extreme settings, any look", nan==0, buf);

    printf("\n--- 8. neutral axis stays neutral where it should ---\n");
    reset(); float tint=0;
    for(int L=0;L<20;L++){ gLook=L; reset(); gLook=L;
      for(int i=1;i<10;i++){ float g=i*0.09f; float3 o=T(g,g,g);
        float s=fmaxf(fmaxf(o.x,o.y),o.z)-fminf(fminf(o.x,o.y),o.z);
        tint=fmaxf(tint,s); } }
    sprintf(buf,"largest neutral spread across all looks %.4f",tint);
    check("looks tint neutrals only via split tone, bounded", tint<0.05f, buf);

    printf("\n%s\n", fails? "FAILURES PRESENT" : "ALL CHECKS PASSED");
    return fails?1:0;
}
