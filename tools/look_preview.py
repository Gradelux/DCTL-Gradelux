# Gradelux FilmLab — contact sheet from tools/look_preview.c output.
# Converts DaVinci Intermediate -> linear DWG -> linear Rec.709, then applies
# a simple filmic display curve for viewing only (not a calibrated DRT).
#   python3 tools/look_preview.py looks.raw preview.png
import numpy as np, sys
from PIL import Image, ImageDraw
W,H=320,200
names=["Off","Pure","Ember","Slate","Solace","Chrome","Dune","Noir","Bloom","Haze","Vivid","Muse","Tundra","Sienna","Velvet","Cinder","Lumen","Fable","Moss","Prism","Eclipse","Mono","Obsidian","Silver"]
raw=np.fromfile(sys.argv[1],dtype=np.float32).reshape(len(names),H,W,3)
A,B,C,M,LC,LO=0.0075,7.0,0.07329248,10.44426855,0.00262409,0.02740668
lin=np.where(raw<=LO, raw/M, 2**(raw/C-B)-A)
dwg2709=np.array([[1.8986148993,-0.7921761834,-0.1064387159],[-0.1689487865,1.4889757541,-0.3200269676],[-0.1215391606,-0.3156758531,1.4372150137]])
x=np.einsum('ij,nhwj->nhwi',dwg2709,lin)
x=np.clip(x*0.75,0,None)   # VIEWING ONLY: simple filmic display rendering, not part of any look
y=(x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14)
y=np.clip(y,0,1)**(1/2.4)
img=(y*255+0.5).astype(np.uint8)
cols=6; rows=4; pad=22
sheet=Image.new('RGB',(cols*W,rows*(H+pad)),(30,30,30))
d=ImageDraw.Draw(sheet)
for i,n in enumerate(names):
    cx,cy=(i%cols)*W,(i//cols)*(H+pad)
    sheet.paste(Image.fromarray(img[i]),(cx,cy+pad))
    d.text((cx+6,cy+5),f"{i:02d} {n}",fill=(235,235,235))
sheet.save(sys.argv[2])
