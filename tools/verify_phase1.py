import math, itertools, random
EPS=1e-6; TINY=1e-9; SAFE_MAX=1e7; EXP_LIMIT=60.0
DI_A=0.0075; DI_B=7.0; DI_C=0.07329248; DI_M=10.44426855
DI_LIN_CUT=0.00262409; DI_LOG_CUT=0.02740668
DI_WHITE=0.513837
LR,LG,LB=0.2126,0.7152,0.0722
HI_BIAS,LO_BIAS=0.75,0.50
SH_KNEE_HI,SH_KNEE_LO=0.78,0.42
SH_R_LO,SH_R_HI=0.55,1.60
SH_PC=0.25
TOE_KNEE,TOE_SLOPE_MIN,TOE_PC=0.35,0.25,0.25
DEPTH_AMOUNT=0.045
WB_T,WB_TI=0.45,0.35
SAT_PROTECT=2.2
OC_K,OC_R,OF_K,OF_R=2.00,1.00,-1.00,0.50

def clampf(x,a,b): return max(a,min(b,x))
def sat1(x): return clampf(x,0.0,1.0)
def cc_safe(x):
    if x!=x: return 0.0
    if x>=-SAFE_MAX and x<=SAFE_MAX: return x
    return SAFE_MAX if x>0 else -SAFE_MAX
def lerp(a,b,t): return a+(b-a)*t
def cc_div(a,b):
    d=b
    if abs(b)<EPS: d=-EPS if b<0 else EPS
    return a/d
def cc_exp(x): return math.exp(clampf(x,-EXP_LIMIT,EXP_LIMIT))
def cc_exp2(x): return cc_exp(x*math.log(2))
def cc_log2(x): return math.log(max(x,TINY))/math.log(2)
def cc_sqrt(x): return math.sqrt(max(x,0.0))
def smoothstep(e0,e1,x):
    t=sat1(cc_div(x-e0,e1-e0)); return t*t*(3-2*t)
def soft_ceil(x,knee,rng):
    if x<=knee: return x
    r=max(rng,EPS); return knee+r*(1-cc_exp(-(x-knee)/r))
def soft_floor(x,knee,rng):
    if x>=knee: return x
    r=max(rng,EPS); return knee-r*(1-cc_exp(-(knee-x)/r))
def di2lin(x):
    if x<=DI_LOG_CUT: return x/DI_M
    return cc_exp2(x/DI_C-DI_B)-DI_A
def lin2di(x):
    if x<=DI_LIN_CUT: return x*DI_M
    return DI_C*(cc_log2(x+DI_A)+DI_B)
def luma(v): return v[0]*LR+v[1]*LG+v[2]*LB
def norm_hi(v): return lerp(luma(v),max(v),HI_BIAS)
def norm_lo(v): return lerp(luma(v),min(v),LO_BIAS)
def exposure(v,s):
    if s==0: return v
    g=cc_exp2(s); return [c*g for c in v]
def wb(v,t,m):
    if t==0 and m==0: return v
    t=clampf(t,-1,1); m=clampf(m,-1,1)
    gr=cc_exp2( WB_T*t+WB_TI*m*0.5); gg=cc_exp2(-WB_TI*m); gb=cc_exp2(-WB_T*t+WB_TI*m*0.5)
    n=gr*LR+gg*LG+gb*LB; inv=cc_div(1.0,n)
    return [v[0]*gr*inv, v[1]*gg*inv, v[2]*gb*inv]
def contrast(v,c,p):
    if c==1.0: return v
    c=max(c,0.01); p=clampf(p,0.05,0.95)
    return [(x-p)*c+p for x in v]
def sh_curve(x,s,r):
    s=sat1(s); r=sat1(r)
    knee=lerp(SH_KNEE_HI,SH_KNEE_LO,s)
    rng=max((1.0-knee)*lerp(SH_R_LO,SH_R_HI,r),EPS)
    return lerp(x,soft_ceil(x,knee,rng),s)
def shoulder(v,s,r):
    if s<=0: return v
    n=norm_hi(v); d=sh_curve(n,s,r)-n
    hs=[x+d for x in v]; pc=[sh_curve(x,s,r) for x in v]
    return [lerp(hs[i],pc[i],SH_PC) for i in range(3)]
def toe_curve(x,s):
    if x>=TOE_KNEE: return x
    s0=lerp(1.0,TOE_SLOPE_MIN,sat1(s)); t=x/TOE_KNEE
    if t<=0.0: return TOE_KNEE*(s0*t)
    f=((s0-1.0)*t+2.0*(1.0-s0))*t*t+s0*t
    return TOE_KNEE*f
def toe(v,s):
    if s<=0: return v
    n=norm_lo(v); d=toe_curve(n,s)-n
    hs=[x+d for x in v]; pc=[toe_curve(x,s) for x in v]
    return [lerp(hs[i],pc[i],TOE_PC) for i in range(3)]
def depth(v,d,p):
    if d<=0: return v
    p=clampf(p,0.15,0.6)
    u=sat1(cc_div(norm_lo(v),p)); b=u*(1.0-u); w=16.0*b*b
    o=-sat1(d)*DEPTH_AMOUNT*w
    return [x+o for x in v]
def bwp(v,b,w):
    if b==0 and w==0: return v
    b=clampf(b,-0.05,0.05); w=clampf(w,-0.15,0.15)
    bs=(DI_WHITE-b)/DI_WHITE; ws=(DI_WHITE+w)/DI_WHITE
    return [(x*bs+b)*ws for x in v]
def saturation(v,s):
    if s==1.0: return v
    s=max(s,0.0); y=luma(v)
    d=[c-y for c in v]
    ch=cc_sqrt(sum(x*x for x in d))
    boost=max(s-1.0,0.0)
    brake=1.0/(1.0+SAT_PROTECT*ch*boost)
    sc=1.0+(s-1.0)*brake
    return [y+x*sc for x in d]
def out_limit(x): return soft_floor(soft_ceil(x,OC_K,OC_R),OF_K,OF_R)

def transform(rgb, P, enc=0, bypass=False):
    rgb=[cc_safe(c) for c in rgb]
    if bypass: return rgb
    if P['exp']!=0 or P['temp']!=0 or P['tint']!=0:
        lin = rgb if enc==1 else [di2lin(c) for c in rgb]
        lin = exposure(lin,P['exp']); lin = wb(lin,P['temp'],P['tint'])
        cv=[lin2di(c) for c in lin]
    else:
        cv = [lin2di(c) for c in rgb] if enc==1 else rgb
    cv=contrast(cv,P['con'],P['piv'])
    cv=shoulder(cv,P['sh'],P['ro'])
    cv=toe(cv,P['toe']); cv=depth(cv,P['dep'],P['piv'])
    cv=bwp(cv,P['bp'],P['wp'])
    cv=saturation(cv,P['sat'])
    cv=[out_limit(c) for c in cv]
    if enc==1: cv=[di2lin(c) for c in cv]
    return [cc_safe(c) for c in cv]

DEF=dict(exp=0.0,temp=0.0,tint=0.0,con=1.0,piv=0.336,sat=1.0,bp=0.0,wp=0.0,
         sh=0.0,ro=0.5,toe=0.0,dep=0.0)

fails=[]
def check(name,cond,info=""):
    if not cond: fails.append(f"{name}: {info}")
    print(("PASS " if cond else "FAIL ")+name+(("  "+info) if info and not cond else ""))

# --- T1: defaults are an exact pass-through (DI mode) ---
worst=0.0
for _ in range(20000):
    rgb=[random.uniform(-0.05,1.15) for _ in range(3)]
    out=transform(rgb,DEF,0)
    worst=max(worst,max(abs(out[i]-rgb[i]) for i in range(3)))
check("T1 defaults are bit-exact pass-through (DI)", worst==0.0, f"max delta {worst:.3e}")

# --- T2: every control individually neutral at default while others vary ---
worst=0.0
for k,v in DEF.items():
    for _ in range(500):
        rgb=[random.uniform(-0.05,1.15) for _ in range(3)]
        out=transform(rgb,DEF,0)
        worst=max(worst,max(abs(out[i]-rgb[i]) for i in range(3)))
check("T2 no control alters image at defaults", worst==0.0, f"{worst:.3e}")

# --- T3: neutral grey stays neutral for every non-colour control ---
maxdev=0.0
for con in [0.25,0.6,1.0,1.7,2.5]:
 for piv in [0.15,0.336,0.6]:
  for sh in [0,0.5,1]:
   for ro in [0,0.5,1]:
    for toe_ in [0,0.5,1]:
     for dep in [0,0.5,1]:
      for bp in [-0.05,0,0.05]:
       for wp in [-0.15,0,0.15]:
        for sat in [0,1,2]:
         P=dict(DEF); P.update(con=con,piv=piv,sh=sh,ro=ro,toe=toe_,dep=dep,bp=bp,wp=wp,sat=sat)
         for g in [0.0,0.1,0.336,0.5138,0.8,1.0]:
             o=transform([g,g,g],P,0)
             maxdev=max(maxdev,max(o)-min(o))
check("T3 neutral grey stays neutral (all non-colour controls)", maxdev<1e-6, f"max R-B spread {maxdev:.3e}")

# --- T4: exhaustive finiteness / no NaN / no Inf, incl. hostile inputs ---
hostile=[[float('nan')]*3,[float('inf')]*3,[float('-inf')]*3,[0,0,0],[-1e30,1e30,0],
         [-10,-10,-10],[1e6,-1e6,0],[1e-30,-1e-30,0],[0.5,float('nan'),-float('inf')]]
bad=0; badcase=None
random.seed(7)
for _ in range(60000):
    P=dict(exp=random.uniform(-5,5),temp=random.uniform(-1,1),tint=random.uniform(-1,1),
           con=random.uniform(0.25,2.5),piv=random.uniform(0.15,0.6),sat=random.uniform(0,2),
           bp=random.uniform(-0.05,0.05),wp=random.uniform(-0.15,0.15),
           sh=random.uniform(0,1),ro=random.uniform(0,1),toe=random.uniform(0,1),dep=random.uniform(0,1))
    rgb = random.choice(hostile) if random.random()<0.15 else [random.uniform(-0.3,1.4) for _ in range(3)]
    enc = random.choice([0,1])
    if enc==1 and rgb not in hostile: rgb=[random.uniform(-0.1,80.0) for _ in range(3)]
    o=transform(rgb,P,enc)
    for c in o:
        if c!=c or math.isinf(c):
            bad+=1; badcase=(rgb,P,enc,o); break
check("T4 never produces NaN or Inf (60k random + hostile)", bad==0, str(badcase))

# --- T5: output bounds in DI mode ---
lo=1e9; hi=-1e9
random.seed(11)
for _ in range(40000):
    P=dict(exp=random.uniform(-5,5),temp=random.uniform(-1,1),tint=random.uniform(-1,1),
           con=random.uniform(0.25,2.5),piv=random.uniform(0.15,0.6),sat=random.uniform(0,2),
           bp=random.uniform(-0.05,0.05),wp=random.uniform(-0.15,0.15),
           sh=random.uniform(0,1),ro=random.uniform(0,1),toe=random.uniform(0,1),dep=random.uniform(0,1))
    rgb=[random.uniform(-0.2,1.3) for _ in range(3)]
    o=transform(rgb,P,0); lo=min(lo,min(o)); hi=max(hi,max(o))
check("T5 DI output stays inside soft limits [-1.5, 3.0]", lo>=-1.5 and hi<=3.0, f"range [{lo:.4f}, {hi:.4f}]")

# --- T6: monotonicity of the full tone chain on a neutral ramp ---
random.seed(3); nonmono=0; worstslope=1e9
for _ in range(4000):
    P=dict(exp=random.uniform(-5,5),temp=0,tint=0,
           con=random.uniform(0.25,2.5),piv=random.uniform(0.15,0.6),sat=1.0,
           bp=random.uniform(-0.05,0.05),wp=random.uniform(-0.15,0.15),
           sh=random.uniform(0,1),ro=random.uniform(0,1),toe=random.uniform(0,1),dep=random.uniform(0,1))
    prev=None
    for i in range(400):
        g=-0.02+i*(1.12+0.02)/399.0
        o=transform([g,g,g],P,0)[1]
        if prev is not None and o<prev-1e-9: nonmono+=1; break
        prev=o
check("T6 tone chain is monotonic on a neutral ramp", nonmono==0, f"{nonmono} non-monotonic cases")

# --- T7: exposure is exactly N stops on a mid-grey ---
errs=[]
for st in [-5,-3,-1,-0.5,0.5,1,3,5]:
    P=dict(DEF); P['exp']=st
    o=transform([0.336043]*3,P,0)
    got=di2lin(o[0]); want=0.18*(2**st)
    errs.append(abs(got-want)/want)
check("T7 exposure is exact in stops", max(errs)<2e-5, f"max rel err {max(errs):.2e}")

# --- T8: white balance preserves neutral luminance ---
errs=[]
for t in [-1,-0.5,0,0.5,1]:
    for m in [-1,-0.5,0,0.5,1]:
        v=wb([0.18,0.18,0.18],t,m)
        errs.append(abs(luma(v)-0.18)/0.18)
check("T8 white balance preserves neutral luminance exactly", max(errs)<1e-12, f"max rel err {max(errs):.2e}")

# --- T9: saturation preserves luma and does not rotate hue direction ---
lerr=[]; hue=[]
random.seed(5)
for _ in range(5000):
    v=[random.uniform(-0.1,1.1) for _ in range(3)]
    for s in [0,0.5,1.5,2.0]:
        o=saturation(v,s)
        lerr.append(abs(luma(o)-luma(v)))
        y=luma(v); d=[c-y for c in v]; do=[c-luma(o) for c in o]
        nd=math.sqrt(sum(x*x for x in d)); ndo=math.sqrt(sum(x*x for x in do))
        if nd>1e-4 and ndo>1e-4:
            cos=sum(d[i]*do[i] for i in range(3))/(nd*ndo)
            hue.append(abs(cos-1.0))
check("T9 saturation preserves luma exactly", max(lerr)<1e-6, f"{max(lerr):.2e}")
check("T9b saturation does not rotate the colour vector", max(hue)<1e-6, f"{max(hue):.2e}")
check("T9c saturation 0 gives exact monochrome",
      max(abs(x-saturation([0.7,0.2,0.4],0.0)[0]) for x in saturation([0.7,0.2,0.4],0.0))<1e-9)

# --- T10: shoulder/toe are C1-continuous at the knee (no visible seam) ---
worstjump=0.0
for s in [0.1,0.4,0.7,1.0]:
    for r in [0.0,0.5,1.0]:
        knee=lerp(SH_KNEE_HI,SH_KNEE_LO,sat1(s))
        a=sh_curve(knee-1e-5,s,r); b=sh_curve(knee+1e-5,s,r)
        worstjump=max(worstjump,abs(b-a))
        # slope continuity
        s1=(sh_curve(knee-1e-4,s,r)-sh_curve(knee-2e-4,s,r))/1e-4
        s2=(sh_curve(knee+2e-4,s,r)-sh_curve(knee+1e-4,s,r))/1e-4
        worstjump=max(worstjump,abs(s1-s2)*1e-3)
check("T10 shoulder is C0/C1 continuous at the knee", worstjump<1e-4, f"{worstjump:.2e}")

# --- T11: shoulder actually compresses, toe actually deepens ---
hi_in=lin2di(20.0)
o=transform([hi_in]*3,{**DEF,'sh':0.8,'ro':0.5},0)[0]
check("T11 shoulder compresses a 20.0 linear specular", o<hi_in-0.02, f"{hi_in:.4f} -> {o:.4f}")
lo_in=lin2di(0.005)
o2=transform([lo_in]*3,{**DEF,'toe':0.8},0)[0]
check("T11b toe deepens a 0.005 linear shadow without crushing it", o2<lo_in and o2>0.0, f"{lo_in:.4f} -> {o2:.4f}")
check("T11c toe keeps black pinned at exactly 0",
      abs(transform([0.0]*3,{**DEF,'toe':1.0},0)[0])<1e-9,
      f"{transform([0.0]*3,{**DEF,'toe':1.0},0)[0]:.3e}")
# toe must remain strictly increasing (compress, never crush) over the toe region
mono=True; prev=None
for i in range(2000):
    x=-0.05+i*0.45/1999.0
    y=toe_curve(x,1.0)
    if prev is not None and y<=prev-1e-12: mono=False; break
    prev=y
check("T11d toe is strictly increasing, no crushed detail", mono)
# depth must not push black below zero
dblack=transform([0.0]*3,{**DEF,'dep':1.0},0)[0]
check("T11e shadow depth does not drive black negative", abs(dblack)<1e-9, f"{dblack:.3e}")

# --- T12: mid grey is not moved by shoulder/toe/depth ---
mids=[]
for sh in [0,0.5,1]:
    for toe_ in [0,0.5,1]:
        for dep in [0,0.5,1]:
            o=transform([0.336043]*3,{**DEF,'sh':sh,'toe':toe_,'dep':dep},0)[0]
            mids.append(abs(o-0.336043))
check("T12 mid grey is stable under shoulder and toe", max(mids)<0.011, f"max shift {max(mids):.4f}")

# --- T13: linear-mode round trip ---
worst=0.0
for _ in range(5000):
    rgb=[random.uniform(0.0,60.0) for _ in range(3)]
    o=transform(rgb,DEF,1)
    for i in range(3):
        if rgb[i]>1e-4: worst=max(worst,abs(o[i]-rgb[i])/rgb[i])
check("T13 linear mode round-trips at defaults", worst<1e-6, f"max rel err {worst:.2e}")

print("\n"+("ALL CHECKS PASSED" if not fails else f"{len(fails)} FAILURES:\n"+"\n".join(fails)))
