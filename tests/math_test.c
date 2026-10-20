// Host-side math test: compiles the DCTL as C via a small shim.
// Build: cd tests && gcc -O2 -o math_test math_test.c -lm && ./math_test
#include <math.h>
#include <stdio.h>
typedef struct {float x,y,z;} float3;
static float3 make_float3(float a,float b,float c){float3 r={a,b,c};return r;}
#define __DEVICE__ static
#define _exp2f exp2f
#define _log2f log2f
#define _powf powf
#define _fabs fabsf
#define _fmaxf fmaxf
#define _fminf fminf
static float _clampf(float x,float a,float b){return fminf(fmaxf(x,a),b);}
enum {GLX_IN_DWG_DI,GLX_IN_709_DISPLAY,GLX_IN_709_SCENE};
#define DEFINE_UI_PARAMS(...)
static int p_InputSpace; static float p_Density,p_SubSat,p_Contrast=1,p_Rolloff,p_ShadowDepth,p_Intensity=1;
#include "../Gradelux_FilmLab.dctl"
float enc(int m,float x){ return m==0?linear_to_di(x):m==1?linear_to_g24(x):linear_to_bt709oetf(x);}
float dec(int m,float v){ return m==0?di_to_linear(v):m==1?g24_to_linear(v):bt709oetf_to_linear(v);}
int main(){
 float vals[]={-0.2,-0.05,0.01,0.02,0.05,0.1,0.336,0.5,0.8,1,1.2,1.5};
 int bad=0;
 for(int m=0;m<3;m++){
  p_InputSpace=m; double maxe=0;
  // round trip with an effectively-neutral parameter that forces full chain: tiny contrast on? use rolloff with knee unreachable: instead call chain manually
  for(int i=0;i<12;i++)for(int j=0;j<12;j++)for(int k=0;k<12;k++){
    float r=vals[i],g=vals[j],b=vals[k];
    float3 w;
    if(m==0) w=make_float3(dec(0,r),dec(0,g),dec(0,b)); else w=rec709_to_dwg(dec(m,r),dec(m,g),dec(m,b));
    float3 o = m==0? w : dwg_to_rec709(w.x,w.y,w.z);
    double e=fabs(enc(m,o.x)-r)+fabs(enc(m,o.y)-g)+fabs(enc(m,o.z)-b); if(e>maxe)maxe=e;
    float3 id=transform(0,0,0,0,r,g,b); if(id.x!=r||id.y!=g||id.z!=b) bad++;
  }
  // grey position
  float greyIn = enc(m,0.18f);
  printf("mode %d: roundtrip max err %.2e, grey code %.4f\n",m,maxe,greyIn);
  // neutral preservation + grey pivot with contrast/shadow/sat/density
  p_Density=1; p_SubSat=1; p_Contrast=1.6; p_ShadowDepth=1; p_Rolloff=0;
  float3 gq=transform(0,0,0,0,greyIn,greyIn,greyIn);
  printf("  grey under contrast/shadow/sat/density: %.6f %.6f %.6f (in %.6f)\n",gq.x,gq.y,gq.z,greyIn);
  // monotonic ramp + NaN scan for extreme params
  float ps[][6]={{1,1,2,1,1,1},{-1,-1,0.5,1,-1,1},{1,-1,0.5,0,1,0.5},{-1,1,2,1,-1,1}};
  for(int q=0;q<4;q++){ p_Density=ps[q][0];p_SubSat=ps[q][1];p_Contrast=ps[q][2];p_Rolloff=ps[q][3];p_ShadowDepth=ps[q][4];p_Intensity=ps[q][5];
    float prev=-1e9; int nonmono=0,nan=0; float lo=m==0?-0.1:-0.2, hi=m==0?1.0:1.5;
    for(int s=0;s<=20000;s++){float v=lo+(hi-lo)*s/20000; float3 o=transform(0,0,0,0,v,v,v);
      if(isnan(o.x)||isinf(o.x)) nan++;
      if(o.x<prev-1e-6) nonmono++;
      prev=o.x;
      float3 c=transform(0,0,0,0,v,v*0.3f,-v*0.2f); if(isnan(c.x+c.y+c.z)||isinf(c.x+c.y+c.z)) nan++;}
    float3 top=transform(0,0,0,0,hi,hi,hi);
    printf("  preset %d: nonmono=%d nan=%d  top %.3f->%.3f  black->%.6f\n",q,nonmono,nan,hi,top.x, transform(0,0,0,0,0,0,0).x);
    bad+=nonmono+nan; }
  p_Density=0;p_SubSat=0;p_Contrast=1;p_Rolloff=0;p_ShadowDepth=0;p_Intensity=1;
 }
 printf("failures: %d\n",bad); return bad!=0;
}
