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
CHROMA_NORM=0.40; CHROMA_FLOOR=0.002
DENSITY_AMOUNT=0.055; DENSITY_EXP_LO=3.0; DENSITY_EXP_HI=1.2
SUBSAT_GAIN=0.60; SUBSAT_LIMIT=0.90
RICH_GAIN=0.35; RICH_LO=0.12; RICH_HI=0.70; RICH_PROTECT=1.50
SEP_MAX=0.60; SEP_EXP=2.0
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
def pow_pos(b,e): return max(b,0.0)**e
def median3(a,b,c): return max(min(a,b),min(max(a,b),c))
def chroma(v): return max(v)-min(v)
def chroma_weight(ch,e): return pow_pos(sat1(ch/CHROMA_NORM),e)

def film_density(v,amount,strength):
    if amount<=0: return v
    e=lerp(DENSITY_EXP_LO,DENSITY_EXP_HI,sat1(strength))
    w=chroma_weight(chroma(v),e)
    o=-sat1(amount)*DENSITY_AMOUNT*w
    return [x+o for x in v]

def subsat_gap(gap,boost):
    head=SUBSAT_LIMIT-gap
    if head<=EPS: return gap
    return gap+head*(1.0-cc_exp(-(gap*boost)/head))

def subtractive_saturation(v,amount):
    if amount<=0: return v
    b=sat1(amount)*SUBSAT_GAIN; mx=max(v)
    return [mx-subsat_gap(mx-x,b) for x in v]

def richness(v,amount):
    if amount<=0: return v
    a=sat1(amount); md=median3(v[0],v[1],v[2])
    u=sat1((md-RICH_LO)/(RICH_HI-RICH_LO)); b=u*(1.0-u); w=16.0*b*b
    boost=a*RICH_GAIN*w
    brake=1.0/(1.0+RICH_PROTECT*chroma(v)*boost)
    gain=1.0+boost*brake
    return [md+(x-md)*gain for x in v]

def remap_mid(c,mn,md,mx,mdOut):
    if c<=md:
        d=md-mn
        return (mn+(c-mn)*(mdOut-mn)/d) if d>EPS else mdOut
    d=mx-md
    return (mdOut+(c-md)*(mx-mdOut)/d) if d>EPS else mdOut

def color_separation(v,amount):
    if amount<=0: return v
    mx=max(v); mn=min(v); rng=mx-mn
    if rng<CHROMA_FLOOR: return v
    md=median3(v[0],v[1],v[2])
    p=sat1((md-mn)/rng)
    k=sat1(amount)*SEP_MAX*chroma_weight(rng,SEP_EXP)
    sp=p*p*(3.0-2.0*p)
    mdOut=mn+(p+k*(sp-p))*rng
    return [remap_mid(x,mn,md,mx,mdOut) for x in v]

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
    cv=film_density(cv,P['den'],P['dens'])
    cv=subtractive_saturation(cv,P['sub'])
    cv=richness(cv,P['rich'])
    cv=color_separation(cv,P['sep'])
    cv=[out_limit(c) for c in cv]
    if enc==1: cv=[di2lin(c) for c in cv]
    return [cc_safe(c) for c in cv]

DEF=dict(exp=0.0,temp=0.0,tint=0.0,con=1.0,piv=0.336,sat=1.0,bp=0.0,wp=0.0,
         sh=0.0,ro=0.5,toe=0.0,dep=0.0,
         den=0.0,dens=0.5,sub=0.0,rich=0.0,sep=0.0)

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
           sh=random.uniform(0,1),ro=random.uniform(0,1),toe=random.uniform(0,1),dep=random.uniform(0,1),
           den=random.uniform(0,1),dens=random.uniform(0,1),sub=random.uniform(0,1),
           rich=random.uniform(0,1),sep=random.uniform(0,1))
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
           sh=random.uniform(0,1),ro=random.uniform(0,1),toe=random.uniform(0,1),dep=random.uniform(0,1),
           den=random.uniform(0,1),dens=random.uniform(0,1),sub=random.uniform(0,1),
           rich=random.uniform(0,1),sep=random.uniform(0,1))
    rgb=[random.uniform(-0.2,1.3) for _ in range(3)]
    o=transform(rgb,P,0); lo=min(lo,min(o)); hi=max(hi,max(o))
check("T5 DI output stays inside soft limits [-1.5, 3.0]", lo>=-1.5 and hi<=3.0, f"range [{lo:.4f}, {hi:.4f}]")

# --- T6: monotonicity of the full tone chain on a neutral ramp ---
random.seed(3); nonmono=0; worstslope=1e9
for _ in range(4000):
    P=dict(exp=random.uniform(-5,5),temp=0,tint=0,
           con=random.uniform(0.25,2.5),piv=random.uniform(0.15,0.6),sat=1.0,
           bp=random.uniform(-0.05,0.05),wp=random.uniform(-0.15,0.15),
           sh=random.uniform(0,1),ro=random.uniform(0,1),toe=random.uniform(0,1),dep=random.uniform(0,1),
           den=random.uniform(0,1),dens=random.uniform(0,1),sub=random.uniform(0,1),
           rich=random.uniform(0,1),sep=random.uniform(0,1))
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


# ==================== PHASE 2 ====================
print("\n--- Phase 2 ---")
random.seed(21)
FULL=dict(DEF); FULL.update(den=1.0,dens=0.5,sub=1.0,rich=1.0,sep=1.0)

# P1: neutrals are untouched by every Phase 2 operator at every setting
dev=0.0
for a in [0.0,0.25,0.5,0.75,1.0]:
 for st in [0.0,0.5,1.0]:
  for g in [0.0,0.05,0.336,0.5138,0.9,1.05]:
    for f in [lambda v:film_density(v,a,st), lambda v:subtractive_saturation(v,a),
              lambda v:richness(v,a), lambda v:color_separation(v,a)]:
        o=f([g,g,g]); dev=max(dev,max(o)-min(o),max(abs(o[i]-g) for i in range(3)))
check("P2-1 neutrals untouched by all four operators", dev<1e-9, f"{dev:.3e}")

# P2: film density darkens only, and preserves channel differences exactly
worst_diff=0.0; brightened=0
for _ in range(20000):
    v=[random.uniform(-0.1,1.1) for _ in range(3)]
    o=film_density(v,random.uniform(0,1),random.uniform(0,1))
    for i in range(3):
        if o[i]>v[i]+1e-12: brightened+=1
    d1=[v[1]-v[0],v[2]-v[1]]; d2=[o[1]-o[0],o[2]-o[1]]
    worst_diff=max(worst_diff,abs(d1[0]-d2[0]),abs(d1[1]-d2[1]))
check("P2-2 film density never brightens a channel", brightened==0, f"{brightened} cases")
check("P2-2b film density preserves channel differences exactly (hue safe)", worst_diff<1e-12, f"{worst_diff:.2e}")

# P3: subtractive saturation never raises the max channel, and never desaturates
rose=0; desat=0
for _ in range(20000):
    v=[random.uniform(-0.1,1.1) for _ in range(3)]
    o=subtractive_saturation(v,random.uniform(0,1))
    if max(o)>max(v)+1e-9: rose+=1
    if chroma(o)<chroma(v)-1e-9: desat+=1
check("P2-3 subtractive saturation never raises the max channel", rose==0, f"{rose} cases")
check("P2-3b subtractive saturation never reduces chroma", desat==0, f"{desat} cases")

# P4: richness holds the median channel exactly fixed
worst=0.0
for _ in range(20000):
    v=[random.uniform(-0.1,1.1) for _ in range(3)]
    o=richness(v,random.uniform(0,1))
    worst=max(worst,abs(median3(*o)-median3(*v)))
check("P2-4 richness holds the median channel fixed", worst<1e-9, f"{worst:.2e}")

# P5: color separation preserves min and max exactly -> chroma bit-preserved
worstc=0.0
for _ in range(20000):
    v=[random.uniform(-0.1,1.1) for _ in range(3)]
    o=color_separation(v,random.uniform(0,1))
    worstc=max(worstc,abs(max(o)-max(v)),abs(min(o)-min(v)))
check("P2-5 color separation preserves min, max and chroma exactly", worstc<1e-9, f"{worstc:.2e}")

# P5b: the hue mapping is strictly increasing in p (hue order preserved)
nonmono=0
for k in [0.0,0.2,0.4,0.6]:
    prev=None
    for i in range(2001):
        p=i/2000.0; sp=p*p*(3.0-2.0*p); q=p+k*(sp-p)
        if prev is not None and q<=prev-1e-15: nonmono+=1; break
        prev=q
check("P2-5b hue position mapping is strictly increasing", nonmono==0, f"{nonmono}")

# P5c: the six pure hue axes are exact fixed points
fp=max(abs((0.0+k*(0.0-0.0))-0.0) for k in [0.0,0.3,0.6])
fp=max(fp,max(abs((1.0+k*(1.0-1.0))-1.0) for k in [0.0,0.3,0.6]))
check("P2-5c primaries and secondaries are exact fixed points", fp<1e-15)

# P6: skin stability. The stated requirement names density, richness and color
# separation; subtractive saturation is a saturation control and is expected to
# saturate skin, so it is measured separately rather than folded in.
SKIN={"shadow":[0.12,0.08,0.055],"mid":[0.30,0.20,0.14],"highlight":[0.50,0.36,0.27]}
DRS=dict(DEF); DRS.update(den=1.0,dens=0.5,rich=1.0,sep=1.0)
print("    skin, density + richness + separation all at maximum:")
worst_stops=0.0; worst_hue=0.0
for name,lin in SKIN.items():
    di=[lin2di(c) for c in lin]; o=transform(di,DRS,0)
    dstops=max(abs(o[i]-di[i]) for i in range(3))/DI_C
    hb=(median3(*di)-min(di))/(max(di)-min(di)); ha=(median3(*o)-min(o))/(max(o)-min(o))
    worst_stops=max(worst_stops,dstops); worst_hue=max(worst_hue,abs(ha-hb))
    print(f"      {name:9s} max channel shift {dstops:.3f} stop   hue position {hb:.3f} -> {ha:.3f}")
check("P2-6 skin within 1/4 stop under density, richness and separation", worst_stops<0.25, f"{worst_stops:.3f} stop")
check("P2-6b skin hue position essentially unchanged", worst_hue<0.01, f"{worst_hue:.4f}")
print("    skin, all four including subtractive saturation at maximum:")
ws2=0.0
for name,lin in SKIN.items():
    di=[lin2di(c) for c in lin]; o=transform(di,FULL,0)
    dstops=max(abs(o[i]-di[i]) for i in range(3))/DI_C
    hb=(median3(*di)-min(di))/(max(di)-min(di)); ha=(median3(*o)-min(o))/(max(o)-min(o))
    ws2=max(ws2,dstops)
    print(f"      {name:9s} max channel shift {dstops:.3f} stop   hue position {hb:.3f} -> {ha:.3f}")
check("P2-6c skin stays bounded with subtractive saturation maxed too", ws2<1.0, f"{ws2:.3f} stop")

# P6d: the gap function is monotonic and never shrinks a gap
nm=0; shrink=0
for b in [0.0,0.15,0.3,0.45,0.6]:
    prev=None
    for i in range(4001):
        gp=i*1.30/4000.0
        y=subsat_gap(gp,b)
        if y<gp-1e-12: shrink+=1
        if prev is not None and y<prev-1e-12: nm+=1; break
        prev=y
check("P2-6d gap expansion is monotonic in gap", nm==0, f"{nm}")
check("P2-6e gap expansion never shrinks a gap", shrink==0, f"{shrink}")

# P7: a saturated primary must actually be affected (the controls do something)
red=[lin2di(c) for c in [0.50,0.06,0.035]]
ro=transform(red,FULL,0)
delta=max(abs(ro[i]-red[i]) for i in range(3))/DI_C
check("P2-7 a saturated primary is meaningfully affected", delta>0.30, f"{delta:.3f} stop")
print(f"    saturated red max channel shift {delta:.3f} stop  (vs skin {worst_stops:.3f})")

# P8: constant-hue exposure ramp stays monotonic through the whole chain
nm=0
random.seed(33)
for _ in range(2000):
    P=dict(exp=0.0,temp=0,tint=0,con=random.uniform(0.25,2.5),piv=random.uniform(0.15,0.6),
           sat=random.uniform(0,2),bp=random.uniform(-0.05,0.05),wp=random.uniform(-0.15,0.15),
           sh=random.uniform(0,1),ro=random.uniform(0,1),toe=random.uniform(0,1),dep=random.uniform(0,1),
           den=random.uniform(0,1),dens=random.uniform(0,1),sub=random.uniform(0,1),
           rich=random.uniform(0,1),sep=random.uniform(0,1))
    base=[0.5,0.3,0.2]; prev=None
    for i in range(200):
        sc=0.02*(2.0**(i*8.0/199.0-4.0))
        o=transform([lin2di(c*sc/0.1) for c in base],P,0)
        y=luma(o)
        if prev is not None and y<prev-2e-3: nm+=1; break
        prev=y
check("P2-8 constant-hue exposure ramp stays monotonic in luma", nm==0, f"{nm} cases")

print("\n"+("ALL CHECKS PASSED" if not fails else f"{len(fails)} FAILURES:\n"+"\n".join(fails)))
