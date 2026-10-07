# Gradelux V1.2 — Manual ISO & Shutter Angle (Milestone 2, Stage 2)

## Files
| File | Action |
|---|---|
| `Camera/ExposureControl.swift` | **New** — shutter angles, exposure math, hardware apply + read-back |
| `Camera/CameraManager.swift` | **Replace** — exposure state, Auto↔Manual transitions, sync with frame-rate changes |
| `Camera/CameraView.swift` | **Replace** — `4K | 24 FPS | 180° | ISO 400` bar, shutter and ISO panels |
| `Camera/CaptureFormat.swift`, `Camera/CameraPreview.swift`, `ContentView.swift`, `GradeluxApp.swift` | Unchanged |

No new permissions, Info.plist keys, or Xcode settings.

## Behaviour
- Shutter time = (angle / 360) × (1 / fps), clamped to the format's min/max exposure and never longer than one frame.
- Manual uses `setExposureModeCustom(duration:iso:)`; ISO clamped to `activeFormat.minISO…maxISO`.
- Auto → Manual derives missing values from the camera's current exposure and compensates ISO, so brightness doesn't jump.
- Frame-rate / resolution changes keep the shutter **angle**; duration is recalculated and ISO re-clamped for the new format.
- Bar values are read from the hardware ~3×/s. A red **A** marks values controlled by Auto Exposure.
- All changes are blocked during recording (UI + session-queue guard).

## Test plan
1. Launch: bar shows `HD | 30 FPS | A xx° | A ISO xx`; values move as you point at bright/dark scenes.
2. Shutter panel → tap `180°`: "A" disappears, image brightness stays about the same, bar shows `180°`.
3. Set 24 FPS: bar still `180°`; shutter panel shows `1/48`.
4. Switch to 60 FPS: still `180°`, now `1/120`. Back to 24: `1/48`.
5. ISO panel → drag slider: image brightens/darkens; value in bar matches. Min/max shown on the right.
6. Turn AUTO EXPOSURE on: "A" returns and exposure adapts smoothly.
7. Manual 360° at 24 FPS in low light; record 5 s; check video is smooth 24 FPS ("Saved · 4K · 24 FPS").
8. During recording the bar is dimmed and panels close.
9. Regression: 4K/HD switching, FPS list, audio, "Saved · …" message, video in Photos.
