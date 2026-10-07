# Gradelux V1.1 — Resolution & Frame Rate (Milestone 2, Stage 1)

## Files
| File | Action |
|---|---|
| `Camera/CaptureFormat.swift` | **New** — resolution/FPS types, format discovery, read-back + file verification |
| `Camera/CameraManager.swift` | **Replace** — applies the selected format on the session queue and verifies it |
| `Camera/CameraView.swift` | **Replace** — `4K | 24 FPS` settings bar and option panels |
| `Camera/CameraPreview.swift`, `ContentView.swift`, `GradeluxApp.swift` | Unchanged |

No new permissions or Info.plist keys. No Xcode setting changes.

## How it works
- Formats come from `AVCaptureDevice.formats`. A resolution/FPS option is shown only if an 8-bit 4:2:0 format with exactly 1920×1080 or 3840×2160 supports that frame rate.
- Changing a setting (only when not recording) runs on the session queue: `beginConfiguration` → `lockForConfiguration` → `activeFormat` → equal `activeVideoMinFrameDuration` / `activeVideoMaxFrameDuration` → `commitConfiguration`.
- The settings bar shows values **read back from the camera**, not the request.
- After each recording, the movie file itself is opened (`AVURLAsset`) and its real resolution + frame rate are shown in the "Saved · 4K · 24 FPS" message. If the file doesn't match the settings, an alert says so.

## Test plan
1. Launch: bar shows `HD | 30 FPS`.
2. Tap `HD` → panel lists only supported resolutions. Pick `4K`. Bar changes to `4K | 30 FPS`.
3. Tap `30 FPS` → only rates supported at 4K are listed. (120 usually appears only for HD, on newer iPhones.)
4. For each combination you care about: record ~5 s → "Saved · 4K · 24 FPS" must match the bar.
5. Verify in Photos: open the video → swipe up (or tap ⓘ). The info panel shows resolution (e.g. 4K) and frame rate (e.g. 24 FPS).
6. While recording, the bar is dimmed and can't be tapped.
7. Switch to 4K, then to a frame rate 4K doesn't support from HD (e.g. HD 120 → 4K): FPS drops to the nearest supported rate automatically.
