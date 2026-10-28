#!/usr/bin/env python3
"""Contact sheets for reviewing film burns.

For one burn: row 1 shows every frame of the overlay on black, row 2 shows the
same frames composited with Screen over a hard cut between two shots (the cut
falls on the burn's cut frame), with the cut frame marked.

Usage: python3 tools/filmburns/review.py FRAMES.npy CUT OUT.png [--a A.f32 --b B.f32] [--ref REF.png]
       FRAMES.npy: saved by burns.py (uint8, 720p). A/B: 1920x1080 float32 RGB shots.
"""
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont


def main():
    args = sys.argv[1:]
    frames = np.load(args[0])
    cut = int(args[1])
    out = args[2]
    get = lambda k: args[args.index(k) + 1] if k in args else None
    n = len(frames)
    tw, th = 256, 144
    a = b = None
    if get('--a'):
        load = lambda p: np.asarray(Image.fromarray((np.clip(np.fromfile(p, np.float32).reshape(1080, 1920, 3), 0, 1) * 255).astype(np.uint8)).resize((tw, th), Image.LANCZOS)).astype(np.float32) / 255
        a, b = load(get('--a')), load(get('--b'))
    rows = 2 if a is not None else 1
    sheet = Image.new('RGB', (tw * n, (th + 22) * rows + 6), (14, 14, 14))
    dr = ImageDraw.Draw(sheet)
    font = ImageFont.truetype('DejaVuSans.ttf', 14)
    for i, fr in enumerate(frames):
        small = np.asarray(Image.fromarray(fr).resize((tw, th), Image.LANCZOS)).astype(np.float32) / 255
        sheet.paste(Image.fromarray((small * 255).astype(np.uint8)), (i * tw, 22))
        lab = 'f%d%s' % (i + 1, '  CUT' if i == cut else '')
        dr.text((i * tw + 4, 3), lab, fill=(255, 210, 120) if i == cut else (150, 150, 150), font=font)
        if a is not None:
            base = a if i < cut else b
            comp = 1 - (1 - base) * (1 - small)
            sheet.paste(Image.fromarray((comp * 255 + 0.5).astype(np.uint8)), (i * tw, th + 22 * 2))
    if get('--ref'):
        ref = Image.open(get('--ref'))
        full = Image.new('RGB', (max(sheet.width, ref.width), sheet.height + ref.height + 10), (14, 14, 14))
        full.paste(sheet, (0, 0)); full.paste(ref, (0, sheet.height + 10))
        sheet = full
    sheet.save(out)


if __name__ == '__main__':
    main()
