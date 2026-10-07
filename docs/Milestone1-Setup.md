# Gradelux — Milestone 1 Setup (Camera Foundation)

Requirements: a Mac with **Xcode 26 or newer**, an iPhone on **iOS 17 or newer**, a USB cable, and a free Apple ID.

## 1. Project / file structure

```
Gradelux/                      ← Xcode project folder
├── Gradelux.xcodeproj
└── Gradelux/                  ← app source folder (blue/yellow folder in Xcode)
    ├── GradeluxApp.swift      ← replace contents
    ├── ContentView.swift      ← replace contents
    ├── Assets.xcassets        ← leave alone
    └── Camera/                ← new folder
        ├── CameraManager.swift
        ├── CameraPreview.swift
        └── CameraView.swift
```

The Swift files in this repo's `Gradelux/` folder are the exact contents to paste.

## 2. Create the Xcode project

1. Xcode → **File ▸ New ▸ Project…**
2. **iOS** tab → **App** → Next.
3. Product Name: `Gradelux`. Team: your Apple ID (or "None" for now). Organization Identifier: something unique like `com.yourname`. Interface: **SwiftUI**. Language: **Swift**. Testing System: **None**. Storage: **None**.
4. Next → choose a folder → Create.

## 3. Add / replace files

1. In the left Project navigator, open `GradeluxApp.swift`, select all (⌘A), delete, paste the repo version.
2. Do the same for `ContentView.swift` (this removes the default "Hello, world!" and `#Preview` code).
3. Right-click the **Gradelux** source folder (the one containing ContentView.swift) → **New Folder** → name it `Camera`.
4. Right-click `Camera` → **New File from Template…** → **Swift File** → Next → name `CameraManager` → make sure target **Gradelux** is checked → Create. Replace its contents.
5. Repeat for `CameraPreview` and `CameraView`.

## 4. Xcode settings

Click the blue **Gradelux** project icon at the top of the navigator → select the **Gradelux** target.

**General tab**
- Minimum Deployments → iOS: **17.0**.
- Deployment Info → iPhone Orientation: check **Portrait** only; uncheck Landscape Left / Right and Upside Down.

**Signing & Capabilities tab**
- Check **Automatically manage signing**.
- Team: choose your Apple ID (Add an Account… if it is missing).
- If the Bundle Identifier shows an error, change it to something unique, e.g. `com.yourname.gradelux`.

**Info tab** (Custom iOS Target Properties) — hover any row, click **+**, add:

| Key (shown in Xcode) | Raw key | Value |
|---|---|---|
| Privacy - Camera Usage Description | `NSCameraUsageDescription` | Gradelux uses the camera to record video. |
| Privacy - Microphone Usage Description | `NSMicrophoneUsageDescription` | Gradelux uses the microphone to record audio with your video. |
| Privacy - Photo Library Additions Usage Description | `NSPhotoLibraryAddUsageDescription` | Gradelux saves your recorded videos to your Photos library. |

## 5. Prepare the iPhone and select it

1. Plug the iPhone into the Mac. Unlock it and tap **Trust** → enter passcode.
2. Enable Developer Mode: iPhone **Settings ▸ Privacy & Security ▸ Developer Mode** → On → restart → confirm. (The option appears after the iPhone has been connected to Xcode once.)
3. In Xcode's toolbar, click the run destination menu (next to the scheme name "Gradelux") and choose your iPhone under **iOS Device**. Wait for any "Preparing iPhone…" progress to finish.

## 6. Build and run

Press **▶ Run** (or ⌘R). First install with a free Apple ID: if the iPhone says "Untrusted Developer", go to **Settings ▸ General ▸ VPN & Device Management**, tap your Apple ID under Developer App, tap **Trust**, then run again.

## 7. Expected permission prompts (first launch, in order)

1. "Gradelux" Would Like to Access the Camera → **Allow**
2. "Gradelux" Would Like to Access the Microphone → **Allow**
3. "Gradelux" Would Like to Add to your Photos → **Allow**

If you tap Don't Allow, the app shows an error screen with **Open Settings**.

## 8. Test recording

1. Live rear-camera preview fills the screen.
2. Tap the record button → red circle becomes a red square; timer `00:00:00` appears at top and counts up.
3. Record ~10 seconds while speaking.
4. Tap again → square returns to circle, timer disappears, **Saved** appears briefly.

## 9. Verify the video

Open **Photos** → Library / Recents: the newest item is a video. Play it — it should be portrait (upright), show the time you recorded, and have sound.

## 10. Common errors

| Problem | Fix |
|---|---|
| "Signing for Gradelux requires a development team" | Signing & Capabilities → choose your Team. |
| "Failed to register bundle identifier" | Change Bundle Identifier to something unique. |
| "Developer Mode disabled" | Step 5.2. |
| "Untrusted Developer" on iPhone | Settings ▸ General ▸ VPN & Device Management → Trust. |
| App crashes at launch with "…privacy-sensitive data without a usage description" | An Info key from step 4 is missing or misspelled. |
| "Camera Unavailable" on the Simulator | The Simulator has no camera. Run on a real iPhone. |
| "Cannot find 'CameraView' in scope" | File isn't in the target: select it → File inspector (right panel) → Target Membership → check Gradelux. |
| "'main' attribute can only apply to one type" | Two files have `@main`. Keep it only in GradeluxApp.swift. |
| iPhone not in destination list | Unlock phone, re-plug cable, tap Trust, wait for Xcode to finish "Preparing". Window ▸ Devices and Simulators shows status. |
| Errors mentioning `videoRotationAngle` or `@Observable` | Minimum Deployment must be iOS 17.0 or later. |
| Errors mentioning `nonisolated` on a type | Update to Xcode 26 or newer. |
| Video not in Photos | Settings ▸ Privacy & Security ▸ Photos ▸ Gradelux → Add Photos Only. |
| No audio | Settings ▸ Privacy & Security ▸ Microphone ▸ Gradelux → On; check the phone isn't connected to a Bluetooth headset. |
| Preview looks more zoomed than the saved video | Expected: the preview fills the taller screen (aspect fill); the file is 16:9. |
