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

enum { CC_Q_FULL=0, CC_Q_FAST=1 };
static int gBypass=0, gQuality=CC_Q_FULL;
static float gExposure=0,gTemperature=0,gTint=0,gContrast=1,gPivot=0.336f,gSaturation=1;
static float gBlackPoint=0,gWhitePoint=0,gHighlightShoulder=0,gHighlightRollOff=0.5f;
static float gShadowToe=0,gShadowDepth=0,gFilmDensity=0,gDensityStrength=0.5f;
static float gSubSaturation=0,gRichness=0,gColorSeparation=0;
static float gRedDensity=0,gOrangeDensity=0,gYellowDensity=0,gGreenDensity=0;
static float gCyanDensity=0,gBlueDensity=0,gMagentaDensity=0;
static float gWarmHighlights=0,gCoolShadows=0,gSplitBalance=0,gBleachBypass=0,gBleachMix=1;
static float gVignette=0,gVignetteSize=0.5f,gVignetteSoft=0.5f,gMatte=0;
static float gSharpen=0,gSharpenRadius=0.35f,gSharpenThresh=0.15f;
static float gPrintContrast=0,gPrintPivot=0.336f,gNegPrint=0;
static float gHiDesat=0,gLoDesat=0,gLoDesatStart=0.22f;
static float gCrossR=0,gCrossG=0,gCrossB=0,gCrossAmount=0;
static float gChanDensR=0,gChanDensG=0,gChanDensB=0;
static float gBlackDensity=0,gHighlightDensity=0,gFilmFade=0;
static float gFilmBias=0,gMidBias=0,gHiHueShift=0,gLoHueShift=0;
static float gVignetteWarm=0;
static float gChromAb=0,gChromAbRadius=0.4f,gChromAbFalloff=0.5f;
static float gFilmSoft=0,gFilmSoftRadius=0.4f,gEdgeSoft=0,gEdgeSoftStart=0.45f;
static float gMicroContrast=0,gMicroRadius=0.5f;
#include "CineCore.dctl"

static int fails=0;
static void check(const char*n,int ok,const char*i){ printf(ok?"  PASS  %s%s%s\n":"  FAIL  %s%s%s\n",n,i[0]?"   ":"",i); if(!ok)fails++; }
static void reset(void){ gExposure=0;gTemperature=0;gTint=0;gContrast=1;gPivot=0.336f;gSaturation=1;
  gBlackPoint=0;gWhitePoint=0;gHighlightShoulder=0;gHighlightRollOff=0.5f;gShadowToe=0;gShadowDepth=0;
  gFilmDensity=0;gDensityStrength=0.5f;gSubSaturation=0;gRichness=0;gColorSeparation=0;
  gRedDensity=0;gOrangeDensity=0;gYellowDensity=0;gGreenDensity=0;gCyanDensity=0;gBlueDensity=0;
  gMagentaDensity=0;gWarmHighlights=0;gCoolShadows=0;gSplitBalance=0;gBleachBypass=0;gBleachMix=1;
  gVignette=0;gVignetteSize=0.5f;gVignetteSoft=0.5f;gMatte=0;gBypass=0;
  gSharpen=0;gSharpenRadius=0.35f;gSharpenThresh=0.15f;
  gPrintContrast=0;gPrintPivot=0.336f;gNegPrint=0;
  gHiDesat=0;gLoDesat=0;gLoDesatStart=0.22f;
  gCrossR=0;gCrossG=0;gCrossB=0;gCrossAmount=0;gChanDensR=0;gChanDensG=0;gChanDensB=0;
  gBlackDensity=0;gHighlightDensity=0;gFilmFade=0;gFilmBias=0;gMidBias=0;
  gHiHueShift=0;gLoHueShift=0;gVignetteWarm=0;
  gChromAb=0;gChromAbRadius=0.4f;gChromAbFalloff=0.5f;gFilmSoft=0;gFilmSoftRadius=0.4f;
  gEdgeSoft=0;gEdgeSoftStart=0.45f;gMicroContrast=0;gMicroRadius=0.5f;gQuality=CC_Q_FULL; }
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

    printf("\n--- sharpening ---\n");
    /* hard vertical edge down the middle, plus a low-amplitude noise field */
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        float v = (x < W/2) ? 0.30f : 0.55f;
        if(y > H*3/4) v = 0.40f + (((x*7+y*13)%5)-2) * 0.0025f;   /* tiny noise */
        TR.p[y][x]=v; TG.p[y][x]=v; TB.p[y][x]=v; }
    /* probes sit ONE pixel either side of the edge: the default radius is under
       2 px, so a probe 2 px away correctly sees no detail at all. */
    int eL=W/2-1, eR=W/2+1, flatX=20, flatY=20, noiseY=H*3/4+10;
    reset(); float3 pL=T(eL,H/4), pR=T(eR,H/4), pF=T(flatX,flatY), pN=T(flatX,noiseY);
    reset(); gSharpen=1.0f;
    float3 sL=T(eL,H/4), sR=T(eR,H/4), sF=T(flatX,flatY), sN=T(flatX,noiseY);
    sprintf(buf,"dark side %+.4f, light side %+.4f", sL.x-pL.x, sR.x-pR.x);
    check("sharpening increases contrast across an edge", (sR.x-pR.x)>0.004f && (sL.x-pL.x)<-0.004f, buf);
    sprintf(buf,"R %+.5f  G %+.5f  B %+.5f", sR.x-pR.x, sR.y-pR.y, sR.z-pR.z);
    check("sharpening produces no colour fringing",
          fabsf((sR.x-pR.x)-(sR.y-pR.y))<1e-6f && fabsf((sR.y-pR.y)-(sR.z-pR.z))<1e-6f, buf);
    sprintf(buf,"delta %.2e", md(sF,pF));
    check("flat areas are untouched", md(sF,pF)<1e-6f, buf);
    sprintf(buf,"noise delta %.5f vs edge delta %.5f", md(sN,pN), fabsf(sR.x-pR.x));
    check("threshold suppresses noise while keeping edges",
          md(sN,pN) < fabsf(sR.x-pR.x)*0.2f, buf);
    reset(); gSharpen=1.0f; gSharpenThresh=0.0f;
    float3 nT=T(flatX,noiseY);
    check("threshold at 0 does let fine detail through", md(nT,pN) > md(sN,pN), "");
    /* overshoot is bounded */
    reset(); gSharpen=1.0f; gSharpenRadius=1.0f;
    float mx2=0; for(int y=0;y<H/2;y++) for(int x=0;x<W;x++) mx2=fmaxf(mx2,fabsf(T(x,y).x-IN(x,y).x));
    sprintf(buf,"largest excursion %.4f",mx2);
    check("overshoot is soft-limited, not unbounded", mx2 < 0.11f, buf);

    printf("\n--- stability ---\n");
    int bad=0;
    reset(); gVignette=1;gMatte=1;gContrast=2.5f;gExposure=5;
    gFilmDensity=1;gSubSaturation=1;gRichness=1;gColorSeparation=1;gBleachBypass=1;
    gHighlightShoulder=1;gShadowToe=1;gShadowDepth=1;gSaturation=2;gSharpen=1;gSharpenRadius=1;
    for(int y=0;y<H;y+=3) for(int x=0;x<W;x+=3){ float3 o=T(x,y);
        if(o.x!=o.x||o.y!=o.y||o.z!=o.z||isinf(o.x)||isinf(o.y)||isinf(o.z)) bad++; }
    sprintf(buf,"%d non-finite",bad);
    check("no NaN or Inf with every effect at maximum", bad==0, buf);
    /* edge safety: no bright rim from out-of-frame taps */
    reset(); gSharpen=1.0f;
    float rim=0; for(int y=0;y<H;y++){ rim=fmaxf(rim,fabsf(T(0,y).x-T(1,y).x)); }
    sprintf(buf,"largest edge step %.4f",rim);
    check("no edge artefact from clamped sampling", rim<0.02f, buf);

    printf("\n--- film response ---\n");
    /* flat grey ramp frame for tonal work, plus a saturated patch */
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){ float v=(float)x/(W-1);
        TR.p[y][x]=v; TG.p[y][x]=v; TB.p[y][x]=v; }
    /* mid probe sits AT the pivot value, not at the middle of the frame */
    int lo=12, mmid=(int)(0.336f*(W-1)), hi=W-14;

    /* An S-curve steepens the middle and compresses BOTH ends. Asserting that
       it raises everything would be asserting it is not an S-curve. Probes:
       shadow, the pivot itself, the upper midtones, and an extreme specular. */
    int umid=(int)(0.60f*(W-1));
    reset(); float3 a0=T(lo,H/2), b0=T(mmid,H/2), u0=T(umid,H/2), c0=T(hi,H/2);
    reset(); gPrintContrast=1.0f;
    float3 a1=T(lo,H/2), bq=T(mmid,H/2), u1=T(umid,H/2), cq=T(hi,H/2);
    sprintf(buf,"shadow %+.4f  pivot %+.4f  upper mid %+.4f  specular %+.4f",
            a1.x-a0.x, bq.x-b0.x, u1.x-u0.x, cq.x-c0.x);
    check("print curve: shadows down, pivot held, upper mids up, speculars compressed",
          a1.x < a0.x - 0.005f && fabsf(bq.x-b0.x) < 0.01f &&
          u1.x > u0.x + 0.005f && cq.x < c0.x, buf);
    reset(); gPrintContrast=1.0f; int nm2=0; float pv=-9;
    for(int x=0;x<W;x++){ float v=T(x,H/2).x; if(v<pv-1e-6f) nm2++; pv=v; }
    sprintf(buf,"%d reversals",nm2);
    check("print curve stays monotonic", nm2==0, buf);
    reset(); gNegPrint=1.0f; float3 n1=T(hi,H/2);
    reset(); gNegPrint=-1.0f; float3 n2=T(hi,H/2);
    sprintf(buf,"print %+.4f vs negative %+.4f", n1.x-c0.x, n2.x-c0.x);
    check("negative and print pull in opposite directions", (n1.x-c0.x)*(n2.x-c0.x)<0.0f, buf);

    /* saturated colour frame */
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){ float t=(float)x/(W-1);
        TR.p[y][x]=0.15f+t*0.55f; TG.p[y][x]=0.10f+t*0.30f; TB.p[y][x]=0.08f+t*0.22f; }
    int dk=10, br=W-12;
    /* Highlight desat now starts at a fixed 0.45, so the bright end of the ramp
       has to reach above that. */
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){ float t=(float)x/(W-1);
        TR.p[y][x]=0.15f+t*0.75f; TG.p[y][x]=0.10f+t*0.45f; TB.p[y][x]=0.08f+t*0.34f; }
    reset(); float3 s0=T(br,H/2), t0=T(dk,H/2);
    float ch0=fmaxf(fmaxf(s0.x,s0.y),s0.z)-fminf(fminf(s0.x,s0.y),s0.z);
    float cl0=fmaxf(fmaxf(t0.x,t0.y),t0.z)-fminf(fminf(t0.x,t0.y),t0.z);
    reset(); gHiDesat=1.0f; float3 sq=T(br,H/2), t1=T(dk,H/2);
    float ch1=fmaxf(fmaxf(sq.x,sq.y),sq.z)-fminf(fminf(sq.x,sq.y),sq.z);
    float cl1=fmaxf(fmaxf(t1.x,t1.y),t1.z)-fminf(fminf(t1.x,t1.y),t1.z);
    sprintf(buf,"highlight chroma %.4f->%.4f, shadow %.4f->%.4f",ch0,ch1,cl0,cl1);
    check("highlight desat removes chroma from highlights only", ch1<ch0*0.5f && fabsf(cl1-cl0)<1e-4f, buf);
    sprintf(buf,"luma delta %.2e", fabsf((sq.x*0.2126f+sq.y*0.7152f+sq.z*0.0722f)-(s0.x*0.2126f+s0.y*0.7152f+s0.z*0.0722f)));
    check("highlight desat preserves luminance", fabsf((sq.x*0.2126f+sq.y*0.7152f+sq.z*0.0722f)-(s0.x*0.2126f+s0.y*0.7152f+s0.z*0.0722f))<1e-5f, buf);
    reset(); gLoDesat=1.0f; float3 t2=T(dk,H/2);
    float cl2=fmaxf(fmaxf(t2.x,t2.y),t2.z)-fminf(fminf(t2.x,t2.y),t2.z);
    sprintf(buf,"shadow chroma %.4f->%.4f",cl0,cl2);
    check("shadow desat removes chroma from shadows", cl2<cl0*0.85f, buf);

    /* crosstalk and channel density must leave neutrals alone / not */
    float ntint=0, ctint=0;
    for(int g=1;g<10;g++){ float v=g*0.09f;
      for(int y=0;y<H;y++) for(int x=0;x<W;x++){ TR.p[y][x]=v;TG.p[y][x]=v;TB.p[y][x]=v; }
      reset(); gCrossR=1;gCrossG=-1;gCrossB=1;gCrossAmount=1;
      float3 o=T(mmid,H/2); ntint=fmaxf(ntint, fmaxf(fmaxf(o.x,o.y),o.z)-fminf(fminf(o.x,o.y),o.z));
      reset(); gChanDensR=1;gChanDensB=-1;
      float3 o2=T(mmid,H/2); ctint=fmaxf(ctint, fmaxf(fmaxf(o2.x,o2.y),o2.z)-fminf(fminf(o2.x,o2.y),o2.z)); }
    sprintf(buf,"largest neutral spread %.2e",ntint);
    check("crosstalk leaves neutrals exactly neutral", ntint<1e-6f, buf);
    sprintf(buf,"neutral spread %.4f",ctint);
    check("channel density does shift colour, as intended", ctint>0.001f, buf);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){ float t=(float)x/(W-1);
        TR.p[y][x]=0.15f+t*0.55f; TG.p[y][x]=0.10f+t*0.30f; TB.p[y][x]=0.08f+t*0.22f; }
    reset(); float3 x0=T(mmid,H/2);
    reset(); gCrossR=1;gCrossG=1;gCrossB=1;gCrossAmount=1; float3 x1=T(mmid,H/2);
    sprintf(buf,"delta %.4f",md(x1,x0));
    check("crosstalk does affect coloured pixels", md(x1,x0)>0.002f, buf);

    /* tonal density, fade, bias, hue shift */
    reset(); gBlackDensity=1.0f; float3 bd=T(dk,H/2);
    check("black density darkens shadows", bd.x<t0.x-0.002f, "");
    reset(); gHighlightDensity=1.0f; float3 hd=T(br,H/2);
    check("highlight density darkens highlights", hd.x<s0.x-0.002f, "");
    reset(); gFilmFade=1.0f; float3 fd=T(dk,H/2), fb=T(br,H/2);
    float fdc=fmaxf(fmaxf(fb.x,fb.y),fb.z)-fminf(fminf(fb.x,fb.y),fb.z);
    sprintf(buf,"shadow %+.4f, highlight %+.4f, chroma %.4f->%.4f",fd.x-t0.x,fb.x-s0.x,ch0,fdc);
    check("film fade lifts shadows, compresses and desaturates",
          fd.x>t0.x && fb.x<s0.x && fdc<ch0, buf);
    reset(); gFilmBias=1.0f; float3 w1=T(mmid,H/2);
    reset(); gFilmBias=-1.0f; float3 w2=T(mmid,H/2);
    sprintf(buf,"warm R%+.4f B%+.4f / cool R%+.4f B%+.4f",w1.x-x0.x,w1.z-x0.z,w2.x-x0.x,w2.z-x0.z);
    check("film bias moves warm and cool oppositely", (w1.x-x0.x)>0 && (w1.z-x0.z)<0 && (w2.x-x0.x)<0, buf);
    float ly0=x0.x*0.2126f+x0.y*0.7152f+x0.z*0.0722f;
    float ly1=w1.x*0.2126f+w1.y*0.7152f+w1.z*0.0722f;
    sprintf(buf,"luma delta %.2e",fabsf(ly1-ly0));
    check("film bias preserves luminance", fabsf(ly1-ly0)<1e-5f, buf);
    /* baseline re-measured on the CURRENT frame: the crosstalk block above
       rebuilds the ramp, so ch0 from earlier belongs to a different image. */
    reset(); float3 hb=T(br,H/2), hbd=T(dk,H/2);
    float chB=fmaxf(fmaxf(hb.x,hb.y),hb.z)-fminf(fminf(hb.x,hb.y),hb.z);
    reset(); gHiHueShift=1.0f; float3 h3=T(br,H/2), h4=T(dk,H/2);
    float cA=fmaxf(fmaxf(h3.x,h3.y),h3.z)-fminf(fminf(h3.x,h3.y),h3.z);
    sprintf(buf,"highlight moved %.4f, shadow moved %.4f, chroma %.4f->%.4f",
            md(h3,hb), md(h4,hbd), chB, cA);
    check("highlight hue shift moves highlights not shadows, chroma intact",
          md(h3,hb)>0.001f && md(h4,hbd)<md(h3,hb)*0.3f && fabsf(cA-chB)<1e-4f, buf);

    printf("\n--- sampled optics ---\n");
    /* Detail EVERYWHERE, including the corners: CA, softness and edge softness
       all act on local detail, so probing them in a flat region measures
       nothing. Top quarter left flat on purpose, for the flat-area test.
       Bright disc at centre so the glows have a source. */
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        float dx2=(x-W/2)/10.0f, dy2=(y-H/2)/10.0f;
        float v;
        if(dx2*dx2+dy2*dy2 < 1.0f)      v = 0.88f;
        else if(y < H/4)                v = 0.40f;              /* flat band */
        else                            v = ((x/2)%2) ? 0.50f : 0.28f;  /* fine stripes:
                                    period 4 px, finer than the CA offset so the
                                    three channels land on different stripes */
        TR.p[y][x]=v; TG.p[y][x]=v; TB.p[y][x]=v; }

    /* chromatic aberration: zero at centre, present at the edge, splits channels */
    int cay=H*3/4;
    reset(); float3 ca0c=T(W/2,H/2), ca0e=T(W-6,cay);   /* true optical centre */
    reset(); gChromAb=1.0f;
    float3 ca1c=T(W/2,H/2), ca1e=T(W-6,cay);
    sprintf(buf,"centre %.2e, edge %.4f", md(ca1c,ca0c), md(ca1e,ca0e));
    check("CA is zero at the optical centre and present at the edge",
          md(ca1c,ca0c)<1e-6f && md(ca1e,ca0e)>0.002f, buf);
    /* on a neutral input it must SEPARATE channels, i.e. produce colour */
    int cax=W-10;
    reset(); gChromAb=1.0f;
    float3 sp=T(cax,cay);
    sprintf(buf,"R-B separation %.4f", fabsf(sp.x-sp.z));
    check("CA separates the channels rather than tinting uniformly", fabsf(sp.x-sp.z)>0.001f, buf);
    reset(); gChromAb=-1.0f; float3 spn=T(cax,cay);
    check("CA reverses direction with a negative amount",
          (sp.x-sp.z)*(spn.x-spn.z) < 0.0f, "");

    /* film softness: reduces local detail, no colour fringing, edges keep position */
    /* one pixel from a stripe boundary, so both radii reach across it */
    int ex=W/2+1, ey=H*3/4;
    reset(); float3 fs0=T(ex,ey), flat0=T(20,H/8);
    reset(); gFilmSoft=1.0f; float3 fs1=T(ex,ey), flat1=T(20,H/8);
    sprintf(buf,"edge %+.4f, flat %.2e", fs1.x-fs0.x, md(flat1,flat0));
    check("film softness reduces edge detail and leaves flat areas alone",
          fabsf(fs1.x-fs0.x)>0.002f && md(flat1,flat0)<1e-6f, buf);
    sprintf(buf,"R %+.5f G %+.5f B %+.5f", fs1.x-fs0.x, fs1.y-fs0.y, fs1.z-fs0.z);
    check("film softness produces no colour fringing",
          fabsf((fs1.x-fs0.x)-(fs1.z-fs0.z))<1e-6f, buf);
    /* softness and sharpening must oppose each other */
    reset(); gSharpen=1.0f; float3 sh1=T(ex,ey);
    sprintf(buf,"soften %+.4f vs sharpen %+.4f", fs1.x-fs0.x, sh1.x-fs0.x);
    check("softness and sharpening move detail in opposite directions",
          (fs1.x-fs0.x)*(sh1.x-fs0.x) < 0.0f, buf);

    /* edge softness: nothing at centre, softening at the frame edge */
    reset(); gEdgeSoft=1.0f;
    float3 es_c=T(W/2+3,cay), es_e=T(W-4,H-4);
    reset(); float3 es_c0=T(W/2+3,cay), es_e0=T(W-4,H-4);
    sprintf(buf,"centre %.2e, corner %.4f", md(es_c,es_c0), md(es_e,es_e0));
    check("edge softness acts at the frame edge, not the centre",
          md(es_c,es_c0)<1e-6f && md(es_e,es_e0)>0.002f, buf);

    /* micro contrast: adds structure, reverses sign, bounded */
    reset(); float3 mc0=T(ex,ey);
    reset(); gMicroContrast=1.0f; float3 mc1=T(ex,ey);
    reset(); gMicroContrast=-1.0f; float3 mc2=T(ex,ey);
    sprintf(buf,"positive %+.4f, negative %+.4f", mc1.x-mc0.x, mc2.x-mc0.x);
    check("micro contrast reverses cleanly with sign", (mc1.x-mc0.x)*(mc2.x-mc0.x)<0.0f, buf);
    sprintf(buf,"R %+.5f B %+.5f", mc1.x-mc0.x, mc1.z-mc0.z);
    check("micro contrast produces no colour fringing",
          fabsf((mc1.x-mc0.x)-(mc1.z-mc0.z))<1e-6f, buf);
    reset(); gMicroContrast=1.0f; gMicroRadius=1.0f;
    float mmax=0; for(int y=0;y<H;y+=2) for(int x=0;x<W;x+=2) mmax=fmaxf(mmax,fabsf(T(x,y).x-IN(x,y).x));
    sprintf(buf,"largest excursion %.4f",mmax);
    check("micro contrast is soft-limited", mmax<0.16f, buf);

    /* everything on at once */
    reset(); gChromAb=1;gFilmSoft=1;gEdgeSoft=1;gMicroContrast=1;gSharpen=1;gVignette=1;gMatte=1;gPrintContrast=1;gFilmFade=1;
    gHiDesat=1;gLoDesat=1;gCrossAmount=1;gCrossR=1;gBlackDensity=1;gHighlightDensity=1;
    int bad2=0;
    for(int y=0;y<H;y+=2) for(int x=0;x<W;x+=2){ float3 o=T(x,y);
        if(o.x!=o.x||o.y!=o.y||o.z!=o.z||isinf(o.x)||isinf(o.y)||isinf(o.z)) bad2++; }
    sprintf(buf,"%d non-finite",bad2);
    check("no NaN or Inf with every effect in the file at maximum", bad2==0, buf);

    printf("\n--- quality modes ---\n");
    /* A LARGE bright region, not a 10 px disc. With four directions instead of
       eight, whether a tap lands on a tiny highlight is luck; on a broad source
       the sampling density is what is actually being compared. The small-source
       case is a genuine limitation of the reduced modes, noted rather than
       tuned away. */
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        float dx2=(x-W/2)/30.0f, dy2=(y-H/2)/30.0f;
        float v = (dx2*dx2+dy2*dy2 < 1.0f) ? 0.88f : 0.24f;
        TR.p[y][x]=v; TG.p[y][x]=v; TB.p[y][x]=v; }
    int qx=W/2+33;
    reset(); float3 q0=T(qx,H/2);
    float qd[2];
    for(int q=0;q<2;q++){ reset(); gQuality=q; gSharpen=1;gMicroContrast=1;
        qd[q]=md(T(qx,H/2),q0); }
    sprintf(buf,"full %.4f  fast %.4f",qd[0],qd[1]);
    check("both quality modes produce the effect", qd[0]>0.002f&&qd[1]>0.002f, buf);
    sprintf(buf,"fast within %.1f%% of full", fabsf(qd[1]-qd[0])/qd[0]*100.0f);
    check("Fast stays close to Full", fabsf(qd[1]-qd[0])/qd[0]<0.20f, buf);
    int qbad=0;
    for(int q=0;q<2;q++){ reset(); gQuality=q;
        gSharpen=1;gFilmSoft=1;gEdgeSoft=1;
        gMicroContrast=1;gChromAb=1;
        for(int y=0;y<H;y+=3) for(int x=0;x<W;x+=3){ float3 o=T(x,y);
            if(o.x!=o.x||isinf(o.x)) qbad++; } }
    sprintf(buf,"%d non-finite",qbad);
    check("all quality modes are numerically safe", qbad==0, buf);
    reset(); gQuality=CC_Q_FAST;
    float dflt=0; for(int y=0;y<H;y+=5) for(int x=0;x<W;x+=5) dflt=fmaxf(dflt,md(T(x,y),IN(x,y)));
    sprintf(buf,"max delta %.2e",dflt);
    check("quality changes nothing when the effects are off", dflt==0.0f, buf);

    printf("\n--- CA must not behave like a vignette ---\n");
    /* radial falloff, exactly the case where unequal R/B luma weights leave a
       net darkening that grows with radius */
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        float dx3=(x-(W-1)*0.5f)/((W-1)*0.5f), dy3=(y-(H-1)*0.5f)/((H-1)*0.5f);
        float r3=sqrtf(dx3*dx3+dy3*dy3)*0.7071f;
        float v=0.70f-0.45f*r3;
        TR.p[y][x]=v; TG.p[y][x]=v; TB.p[y][x]=v; }
    float worstL=0;
    for(int q=0;q<2;q++){
      for(int i=0;i<6;i++){
        int px=(W-4)-i*10, py=H/2;
        reset(); gQuality=q; float3 base=T(px,py);
        reset(); gQuality=q; gChromAb=1.0f; float3 ca=T(px,py);
        float l0=base.x*0.2126f+base.y*0.7152f+base.z*0.0722f;
        float l1=ca.x*0.2126f+ca.y*0.7152f+ca.z*0.0722f;
        worstL=fmaxf(worstL,fabsf(l1-l0)); } }
    sprintf(buf,"largest luminance change %.2e",worstL);
    check("CA changes colour without changing brightness", worstL<1e-5f, buf);
    /* and it must still separate the channels on a gradient */
    reset(); gChromAb=1.0f; float3 cg=T(W-8,H/2);
    reset(); float3 cb=T(W-8,H/2);
    sprintf(buf,"R %+.5f  B %+.5f", cg.x-cb.x, cg.z-cb.z);
    check("CA still separates red and blue oppositely", (cg.x-cb.x)*(cg.z-cb.z)<0.0f, buf);

    printf("\n%s\n", fails?"FAILURES PRESENT":"ALL CHECKS PASSED");
    return fails?1:0; }
