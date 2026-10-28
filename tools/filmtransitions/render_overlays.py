#!/usr/bin/env python3
"""Renders every Film Transition as a light-leak overlay video on black.

Uses the transitions' own DCTL code (compiled as C), with black in and out, so
the video contains only the leaked light. Put it on a track above a cut and
set its Composite Mode to Screen (or Add). Line the middle of the overlay up
with the cut. No DCTL transition plugin needed.

Usage: python3 tools/filmtransitions/render_overlays.py OUT_DIR [frames] [fps]
       defaults: 36 frames at 24 fps (1.5 seconds), 1920x1080 H.264
"""
import glob, os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.argv, ARGS = sys.argv[:1], sys.argv[1:]
import test as tt

W, H = 1920, 1080


def main():
    out = ARGS[0]
    frames = int(ARGS[1]) if len(ARGS) > 1 else 36
    fps = int(ARGS[2]) if len(ARGS) > 2 else 24
    os.makedirs(out, exist_ok=True)
    black = np.zeros((H, W, 3), np.float32)
    rng = np.random.default_rng(0)
    for path in sorted(glob.glob(os.path.join(ROOT, 'FilmTransitions', 'Gradelux_Transition_*.dctl'))):
        name = os.path.basename(path)[:-5]
        exe, err = tt.build(path)
        tmp = os.path.join(tt.TMP, name + '_frames')
        os.makedirs(tmp, exist_ok=True)
        for i in range(frames):
            p = (i + 0.5) / frames
            img = tt.run(exe, black, black, p)
            # fine dither where there is light, so soft gradients do not band in 8-bit video
            light = np.clip(img.max(axis=2, keepdims=True) * 8.0, 0, 1)
            img = img + (rng.random(img.shape, dtype=np.float32) - rng.random(img.shape, dtype=np.float32)) / 255.0 * light
            q = (np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8)
            from PIL import Image
            Image.fromarray(q).save(os.path.join(tmp, 'f%03d.png' % i))
        mp4 = os.path.join(out, name.replace('Transition', 'LightLeak') + '_1080p.mp4')
        subprocess.check_call(['ffmpeg', '-v', 'error', '-y', '-framerate', str(fps), '-i', os.path.join(tmp, 'f%03d.png'),
                               '-c:v', 'libx264', '-preset', 'slow', '-crf', '10', '-pix_fmt', 'yuv420p',
                               '-color_primaries', 'bt709', '-color_trc', 'bt709', '-colorspace', 'bt709',
                               '-movflags', '+faststart', mp4])
        print('wrote', mp4)


if __name__ == '__main__':
    main()
