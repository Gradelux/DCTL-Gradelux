#!/usr/bin/env python3
"""Demo video: each burn composited (Screen) over a hard cut between two shots.

Usage: python3 tools/filmburns/demo.py OUT.mp4 A.f32 B.f32 NAME:CUT [NAME:CUT ...] --frames DIR
       A/B: 1920x1080 float32 RGB stills. DIR holds NAME_frames.npy from burns.py.
"""
import os, subprocess, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont


def load(p, w, h):
    a = np.clip(np.fromfile(p, np.float32).reshape(1080, 1920, 3), 0, 1)
    return np.asarray(Image.fromarray((a * 255).astype(np.uint8)).resize((w, h), Image.LANCZOS)).astype(np.float32) / 255


def main():
    args = sys.argv[1:]
    out, pa, pb = args[0], args[1], args[2]
    d = args[args.index('--frames') + 1]
    items = [x for x in args[3:args.index('--frames')]]
    w, h = 1280, 720
    A, B = load(pa, w, h), load(pb, w, h)
    rng = np.random.default_rng(0)
    font = ImageFont.truetype('DejaVuSans.ttf', 22)
    ff = subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', '%dx%d' % (w, h), '-r', '24',
                           '-i', '-', '-c:v', 'libx264', '-crf', '15', '-pix_fmt', 'yuv420p', '-movflags', '+faststart', out],
                          stdin=subprocess.PIPE)

    def emit(img, label):
        im = Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8))
        ImageDraw.Draw(im).text((16, h - 34), label, fill=(235, 230, 220), font=font)
        ff.stdin.write(np.asarray(im).tobytes())

    for item in items:
        name, cut = item.rsplit(':', 1)
        cut = int(cut)
        fr = np.load(os.path.join(d, name + '_frames.npy')).astype(np.float32) / 255
        fr = np.stack([np.asarray(Image.fromarray((f * 255).astype(np.uint8)).resize((w, h), Image.LANCZOS)) for f in fr]).astype(np.float32) / 255
        label = name.replace('Gradelux_Burn_', '').replace('_', ' ')
        noisy = lambda s: s + rng.normal(0, 0.006, s.shape).astype(np.float32)   # stills that move like footage
        for _ in range(16):
            emit(noisy(A), label)
        for i, f in enumerate(fr):
            base = A if i < cut else B
            emit(1 - (1 - noisy(base)) * (1 - f), label)
        for _ in range(16):
            emit(noisy(B), label)
    ff.stdin.close(); ff.wait()


if __name__ == '__main__':
    main()
