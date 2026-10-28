# Gradelux Light Leak Overlays

The same eight light-leak transitions as the DCTL pack, rendered as video files: light on black,
1920x1080, 24 fps, 1.5 seconds (36 frames). They need no DCTL or plugin, so they work even when
Resolve's DCTL transition plugin does not.

| File | Look |
|---|---|
| `Gradelux_LightLeak_01_RedLeak_1080p.mp4` | Deep red glow from the right, blooms into an amber flood |
| `Gradelux_LightLeak_02_AmberFlash_1080p.mp4` | Quick golden flash from the centre |
| `Gradelux_LightLeak_03_BurnOut_1080p.mp4` | Red-orange flare from the left that burns out to cream white |
| `Gradelux_LightLeak_04_OrangeSweep_1080p.mp4` | Soft orange leak travelling left to right |
| `Gradelux_LightLeak_05_CornerFlare_1080p.mp4` | Hot red flare from the bottom right corner |
| `Gradelux_LightLeak_06_GoldDissolve_1080p.mp4` | Gentle golden glow |
| `Gradelux_LightLeak_07_TwinLeaks_1080p.mp4` | Warm leaks from both sides meeting in the middle |
| `Gradelux_LightLeak_08_FlashFrames_1080p.mp4` | Two quick flash frames, then a full burn |

## How to use them in Resolve

1. Import the videos into the Media Pool.
2. Cut your two clips together on track V1.
3. Put a light leak on track **V2**, above the cut, with its **middle** on the cut. The brightest moment
   is in the middle, so it hides the cut.
4. Select the light leak. In the Inspector, set **Composite Mode** to **Screen**. Use **Add** for a
   hotter, more blown-out look.
5. Adjust **Opacity** in the Inspector to taste.

Tips:

- **Longer or shorter:** right-click > Change Clip Speed, or use Retime Controls. The leaks are soft, so
  they still look good slowed down.
- **4K timelines:** scale the clip up in the Inspector. The light is soft, so upscaling does not show.
- **Flip or rotate** (Inspector > Transform) to make a leak come from the other side.
- For a cross dissolve under the leak, add a short Cross Dissolve on V1 at the same cut.

Rebuild the videos with `python3 tools/filmtransitions/render_overlays.py OUT_DIR [frames] [fps]`.
