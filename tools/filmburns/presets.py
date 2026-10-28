"""Film burn definitions.

Each burn is a few exposure layers plus timing. Times are normalised
(0 = first frame, 1 = last frame). 'cut' is the frame index where the
incoming shot should start: it sits on the brightest part of the burn.

Layer keys:
  source      'L', 'R', 'T', 'B' (light from a frame edge) or
              ('P', x_keys, y_keys, spread_x, spread_y) for a point source
  front       how far the light has travelled (frame heights), keyed over time
  gain        exposure strength inside the burn, keyed over time
  core        extra exposure in the hot centre, keyed over time
  warp/scale  size and scale of the irregular boundary
  swirl       how twisted the boundary shapes are
  soft        edge softness (varies across the frame automatically)
  tail        faint leading glow ahead of the front (renders red)
  spots       small secondary leaks near the front
  bend        curvature of edge light
"""

RESPONSE_DEFAULT = dict(
    kr=6.0, kg=1.1, pg=1.4, kb=0.22, pb=1.55,   # characteristic curves: red saturates first
    halation=0.55, halation_px=30,              # red spreads wider: thin red rim at burn edges
    density=0.05, bloom=0.20, magenta=0.0,
)

PRESETS = {
    # 1. Fast red-orange burn from the right edge, like the reference's first burn.
    'Gradelux_Burn_01_Classic35mm': dict(
        frames=14, cut=8, seed=11, noise_rate=0.20, flicker=0.10, grain=0.034, dust=0.8, weave=0.003,
        layers=[
            dict(source='R', seed=3.0, scale=1.0, warp=0.30, swirl=0.6, soft=0.06, bend=0.25, bend_at=0.4,
                 falloff=1.6, spots=0.0, core_off=0.3,
                 front=[(0, -0.15), (0.15, 0.05), (0.3, 0.35), (0.45, 1.0), (0.55, 1.9), (0.68, 1.6), (0.82, 0.7), (1, 0.05)],
                 gain=[(0, 0.0), (0.12, 0.35), (0.3, 0.8), (0.45, 1.5), (0.55, 2.2), (0.62, 1.9), (0.75, 0.9), (0.88, 0.3), (1, 0.0)],
                 core=[(0.35, 0.0), (0.5, 0.9), (0.6, 1.0), (0.75, 0.25), (0.9, 0.0)]),
            dict(source=('P', [(0, 1.70)], [(0, 0.88)], 0.8, 0.5), seed=21.0, scale=1.9, warp=0.22, soft=0.08,
                 tail=0.2, tail_gain=0.4,
                 front=[(0, 0.0), (0.4, 0.5), (0.6, 0.9), (1, 1.0)],
                 gain=[(0.2, 0.0), (0.45, 0.35), (0.6, 0.4), (0.8, 0.15), (1, 0.0)]),
        ],
        flash=[(0.45, 0.0), (0.55, 0.25), (0.62, 0.15), (0.75, 0.0)], flash_dir=0.8,
    ),
    # 2. Irregular light entering at the film edge, through the perforations.
    'Gradelux_Burn_04_FilmEdge': dict(
        frames=20, cut=11, seed=23, noise_rate=0.17, flicker=0.20, grain=0.036, dust=1.5, weave=0.004,
        scratch=(0.62, 0.30),
        layers=[
            dict(source='L', seed=7.0, scale=1.0, warp=0.34, swirl=0.65, soft=0.045, bend=-0.3, bend_at=0.55,
                 falloff=1.0, spots=0.35, core_off=0.15, streak=0.35, streak_freq=4.0,
                 front=[(0, -0.1), (0.15, 0.12), (0.3, 0.30), (0.42, 0.24), (0.52, 1.0), (0.58, 2.0), (0.64, 1.7), (0.74, 0.9), (0.86, 0.35), (1, 0.0)],
                 gain=[(0, 0.3), (0.15, 0.7), (0.3, 1.0), (0.42, 0.8), (0.55, 1.8), (0.62, 2.0), (0.75, 1.0), (0.9, 0.35), (1, 0.05)],
                 core=[(0.45, 0.0), (0.58, 1.3), (0.68, 0.6), (0.85, 0.0)]),
        ],
        perfs=dict(side='L', pitch=0.25, gain=[(0, 0.25), (0.5, 0.7), (0.62, 0.9), (0.9, 0.2), (1, 0.0)]),
        flash=[(0.5, 0.0), (0.57, 0.55), (0.62, 0.35), (0.7, 0.0)], flash_dir=-0.6,
    ),
    # 3. Dramatic burn to full-frame cream white, like the reference's second burn.
    'Gradelux_Burn_10_Burnout': dict(
        frames=16, cut=9, seed=37, noise_rate=0.24, flicker=0.08, grain=0.032, dust=1.0, weave=0.003,
        layers=[
            dict(source=('P', [(0, -0.35), (0.5, 0.45), (1, 0.8)], [(0, 0.55), (1, 0.45)], 0.85, 1.7),
                 seed=13.0, scale=1.0, warp=0.30, swirl=0.6, soft=0.06, falloff=1.4, spots=0.15, core_off=0.22,
                 front=[(0, 0.05), (0.3, 0.45), (0.5, 1.1), (0.6, 1.8), (0.68, 0.6), (0.76, 0.15), (1, 0.0)],
                 gain=[(0, 0.0), (0.1, 0.35), (0.3, 1.1), (0.45, 1.9), (0.55, 2.4), (0.62, 1.4), (0.7, 0.3), (0.78, 0.0)],
                 core=[(0.25, 0.2), (0.45, 1.6), (0.55, 2.2), (0.65, 0.3), (0.8, 0.0)]),
            dict(source=('P', [(0, 1.78)], [(0, 1.0)], 0.75, 0.6), seed=29.0, scale=1.0, warp=0.25, swirl=0.6, soft=0.07,
                 falloff=0.8,
                 front=[(0.45, 0.2), (0.65, 0.6), (0.85, 0.5), (1, 0.3)],
                 gain=[(0.45, 0.0), (0.6, 1.2), (0.75, 0.8), (0.9, 0.25), (1, 0.0)]),
        ],
        flash=[(0.4, 0.0), (0.5, 1.5), (0.56, 2.1), (0.6, 1.3), (0.68, 0.4), (0.76, 0.0)], flash_dir=-0.35,
        flash_shrink=[(0.58, -1.0), (0.66, 0.35), (0.76, 0.75), (0.9, 1.1)],
    ),
}
