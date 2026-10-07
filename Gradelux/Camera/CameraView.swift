import SwiftUI
import UIKit

/// The full-screen camera screen: live preview, timer, record button, and status messages.
struct CameraView: View {
    @State private var camera = CameraManager()

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()

            if let error = camera.setupError {
                CameraErrorView(error: error) {
                    Task { await camera.start() }
                }
            } else {
                CameraPreview(session: camera.session)
                    .ignoresSafeArea()

                VStack {
                    if camera.isRecording {
                        RecordingTimerView(text: camera.formattedElapsedTime)
                            .padding(.top, 12)
                    }

                    Spacer()

                    if camera.showSavedConfirmation {
                        SavedConfirmationView()
                            .padding(.bottom, 16)
                            .transition(.opacity)
                    }

                    RecordButton(
                        isRecording: camera.isRecording,
                        isEnabled: camera.isSessionRunning
                            && camera.recordingState != .starting
                            && camera.recordingState != .finishing
                    ) {
                        camera.toggleRecording()
                    }
                    .padding(.bottom, 32)
                }
                .animation(.easeInOut(duration: 0.2), value: camera.showSavedConfirmation)
            }
        }
        .preferredColorScheme(.dark)
        .statusBarHidden()
        .task {
            await camera.start()
        }
        .onDisappear {
            camera.stop()
        }
        .alert(
            "Something went wrong",
            isPresented: Binding(
                get: { camera.alertMessage != nil },
                set: { isPresented in
                    if !isPresented { camera.alertMessage = nil }
                }
            )
        ) {
            Button("OK", role: .cancel) {}
        } message: {
            Text(camera.alertMessage ?? "")
        }
    }
}

// MARK: - Record button

/// White ring with a red circle inside. The circle morphs into a square while recording.
private struct RecordButton: View {
    let isRecording: Bool
    let isEnabled: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            ZStack {
                Circle()
                    .strokeBorder(Color.white, lineWidth: 6)
                    .frame(width: 84, height: 84)

                RoundedRectangle(cornerRadius: isRecording ? 8 : 32)
                    .fill(Color.red)
                    .frame(width: isRecording ? 34 : 64, height: isRecording ? 34 : 64)
            }
            .animation(.easeInOut(duration: 0.2), value: isRecording)
        }
        .buttonStyle(.plain)
        .disabled(!isEnabled)
        .opacity(isEnabled ? 1 : 0.5)
        .accessibilityLabel(isRecording ? "Stop recording" : "Start recording")
    }
}

// MARK: - Timer

private struct RecordingTimerView: View {
    let text: String

    var body: some View {
        HStack(spacing: 8) {
            Circle()
                .fill(Color.red)
                .frame(width: 8, height: 8)
            Text(text)
                .font(.system(size: 17, weight: .semibold, design: .monospaced))
                .foregroundStyle(.white)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .background(Color.black.opacity(0.5), in: Capsule())
    }
}

// MARK: - Saved confirmation

private struct SavedConfirmationView: View {
    var body: some View {
        Text("Saved")
            .font(.system(size: 15, weight: .semibold))
            .foregroundStyle(.white)
            .padding(.horizontal, 14)
            .padding(.vertical, 6)
            .background(Color.black.opacity(0.6), in: Capsule())
    }
}

// MARK: - Setup error screen

/// Shown instead of the preview when the camera cannot be used.
private struct CameraErrorView: View {
    let error: CameraError
    let retry: () -> Void

    @Environment(\.openURL) private var openURL

    var body: some View {
        VStack(spacing: 16) {
            Image(systemName: "video.slash")
                .font(.system(size: 44))
                .foregroundStyle(.white)

            Text("Camera Unavailable")
                .font(.title3.weight(.semibold))
                .foregroundStyle(.white)

            Text(error.localizedDescription)
                .font(.body)
                .foregroundStyle(.white.opacity(0.8))
                .multilineTextAlignment(.center)

            if error.isPermissionError {
                Button("Open Settings") {
                    if let url = URL(string: UIApplication.openSettingsURLString) {
                        openURL(url)
                    }
                }
                .buttonStyle(.borderedProminent)
            } else {
                Button("Try Again", action: retry)
                    .buttonStyle(.borderedProminent)
            }
        }
        .padding(32)
    }
}
