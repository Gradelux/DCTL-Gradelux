# Gradelux FilmLab — contact sheet from tools/look_preview.c output.
# Converts DaVinci Intermediate -> linear DWG -> linear Rec.709, then applies
# a simple filmic display curve for viewing only (not a calibrated DRT, and
# not part of any look).
#   python3 tools/look_preview.py tiles.raw sheet.png ["label|label|..."] [columns]
# Without labels, the Film Looks of the current version (default look_preview output) are assumed.
import numpy as np, sys
from PIL import Image, ImageDraw
W, H = 320, 200
LOOKS = ["Off", "Verona 250D", "Verdant 250D", "Midnight 800T", "Aurelia 400", "Solara 200", "Moss 250D",
         "Chroma 100", "Silvergrain 100", "Tungsten 500T", "Halcyon 400", "Argent 200", "Nocturne 800",
         "Pastelle 400"]   # Version 7
labels = sys.argv[3].split('|') if len(sys.argv) > 3 else [f"{i:02d} {n}" for i, n in enumerate(LOOKS)]
cols = int(sys.argv[4]) if len(sys.argv) > 4 else 6
raw = np.fromfile(sys.argv[1], dtype=np.float32).reshape(len(labels), H, W, 3)
A, B, C, M, LO = 0.0075, 7.0, 0.07329248, 10.44426855, 0.02740668
lin = np.where(raw <= LO, raw / M, 2 ** (raw / C - B) - A)
dwg2709 = np.array([[1.8986148993, -0.7921761834, -0.1064387159],
                    [-0.1689487865, 1.4889757541, -0.3200269676],
                    [-0.1215391606, -0.3156758531, 1.4372150137]])
x = np.clip(np.einsum('ij,nhwj->nhwi', dwg2709, lin) * 0.75, 0, None)   # VIEWING ONLY
y = np.clip((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0, 1) ** (1 / 2.4)
img = (y * 255 + 0.5).astype(np.uint8)
rows = (len(labels) + cols - 1) // cols; pad = 22
sheet = Image.new('RGB', (cols * W, rows * (H + pad)), (30, 30, 30))
d = ImageDraw.Draw(sheet)
for i, n in enumerate(labels):
    cx, cy = (i % cols) * W, (i // cols) * (H + pad)
    sheet.paste(Image.fromarray(img[i]), (cx, cy + pad))
    d.text((cx + 6, cy + 5), n, fill=(235, 235, 235))
sheet.save(sys.argv[2])
