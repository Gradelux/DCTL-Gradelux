import SwiftUI
import UIKit

/// Which settings panel is open.
private enum SettingsPanel {
    case resolution
    case frameRate
}

/// The full-screen camera screen: live preview, settings bar, timer, record button, and status messages.
struct CameraView: View {
    @State private var camera = CameraManager()
    @State private var openPanel: SettingsPanel?

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
                    .onTapGesture { openPanel = nil }

                VStack(spacing: 10) {
                    SettingsBar(
                        resolutionText: camera.activeSettings?.resolutionLabel ?? "—",
                        frameRateText: camera.activeSettings?.frameRateLabel ?? "—",
                        openPanel: openPanel,
                        isEnabled: camera.canChangeFormat,
                        isBusy: camera.isApplyingFormat
                    ) { panel in
                        openPanel = (openPanel == panel) ? nil : panel
                    }
                    .padding(.top, 8)

                    if camera.isRecording {
                        RecordingTimerView(text: camera.formattedElapsedTime)
                    }

                    Spacer()

                    if let openPanel {
                        settingsPanel(openPanel)
                            .padding(.horizontal, 16)
                            .transition(.move(edge: .bottom).combined(with: .opacity))
                    }

                    if camera.showSavedConfirmation {
                        SavedConfirmationView(text: camera.savedConfirmationText)
                            .transition(.opacity)
                    }

                    RecordButton(
                        isRecording: camera.isRecording,
                        isEnabled: camera.isSessionRunning
                            && !camera.isApplyingFormat
                            && camera.recordingState != .starting
                            && camera.recordingState != .finishing
                    ) {
                        openPanel = nil
                        camera.toggleRecording()
                    }
                    .padding(.top, 6)
                    .padding(.bottom, 32)
                }
                .animation(.easeInOut(duration: 0.2), value: camera.showSavedConfirmation)
                .animation(.easeInOut(duration: 0.2), value: openPanel)
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
        .onChange(of: camera.recordingState) { _, newState in
            // Settings can't change while recording, so close any open panel.
            if newState != .idle { openPanel = nil }
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

    @ViewBuilder
    private func settingsPanel(_ panel: SettingsPanel) -> some View {
        switch panel {
        case .resolution:
            OptionPanel(
                title: "RESOLUTION",
                options: camera.capabilities.resolutions,
                selected: camera.activeSettings?.resolution,
                label: \.label,
                detail: \.detail,
                isEnabled: camera.canChangeFormat
            ) { resolution in
                camera.selectResolution(resolution)
            }
        case .frameRate:
            OptionPanel(
                title: "FRAME RATE · \(camera.selectedResolution.label)",
                options: camera.capabilities.frameRates(for: camera.selectedResolution),
                selected: camera.activeSettings?.lockedFrameRate,
                label: { "\($0.rawValue)" },
                detail: { _ in "FPS" },
                isEnabled: camera.canChangeFormat
            ) { frameRate in
                camera.selectFrameRate(frameRate)
            }
        }
    }
}

// MARK: - Settings bar

/// Top status bar, e.g. `4K | 24 FPS`. Each value is tappable.
private struct SettingsBar: View {
    let resolutionText: String
    let frameRateText: String
    let openPanel: SettingsPanel?
    let isEnabled: Bool
    let isBusy: Bool
    let onTap: (SettingsPanel) -> Void

    var body: some View {
        HStack(spacing: 0) {
            SettingChip(text: resolutionText, isOpen: openPanel == .resolution, isEnabled: isEnabled) {
                onTap(.resolution)
            }
            Divider()
                .frame(height: 14)
                .overlay(Color.white.opacity(0.35))
            SettingChip(text: frameRateText, isOpen: openPanel == .frameRate, isEnabled: isEnabled) {
                onTap(.frameRate)
            }
            if isBusy {
                ProgressView()
                    .controlSize(.small)
                    .tint(.white)
                    .padding(.trailing, 10)
            }
        }
        .background(Color.black.opacity(0.6), in: Capsule())
        .overlay(Capsule().strokeBorder(Color.white.opacity(0.12), lineWidth: 1))
    }
}

private struct SettingChip: View {
    let text: String
    let isOpen: Bool
    let isEnabled: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(text)
                .font(.system(size: 15, weight: .semibold, design: .monospaced))
                .foregroundStyle(isOpen ? Color.red : Color.white)
                .padding(.horizontal, 14)
                .padding(.vertical, 8)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!isEnabled)
        .opacity(isEnabled ? 1 : 0.45)
    }
}

// MARK: - Option panel

/// A row of choices (e.g. HD / 4K). Only supported options are passed in.
private struct OptionPanel<Option: Identifiable & Equatable>: View {
    let title: String
    let options: [Option]
    let selected: Option?
    let label: (Option) -> String
    let detail: (Option) -> String
    let isEnabled: Bool
    let onSelect: (Option) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(title)
                .font(.system(size: 11, weight: .semibold, design: .monospaced))
                .foregroundStyle(.white.opacity(0.6))

            if options.isEmpty {
                Text("No supported options on this camera.")
                    .font(.footnote)
                    .foregroundStyle(.white.opacity(0.8))
            } else {
                HStack(spacing: 8) {
                    ForEach(options) { option in
                        let isSelected = option == selected
                        Button {
                            onSelect(option)
                        } label: {
                            VStack(spacing: 2) {
                                Text(label(option))
                                    .font(.system(size: 17, weight: .semibold, design: .monospaced))
                                Text(detail(option))
                                    .font(.system(size: 10, weight: .medium, design: .monospaced))
                                    .opacity(0.6)
                            }
                            .foregroundStyle(.white)
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 10)
                            .background(Color.white.opacity(isSelected ? 0.12 : 0.04),
                                        in: RoundedRectangle(cornerRadius: 8))
                            .overlay(
                                RoundedRectangle(cornerRadius: 8)
                                    .strokeBorder(isSelected ? Color.red : Color.white.opacity(0.15),
                                                  lineWidth: isSelected ? 1.5 : 1)
                            )
                        }
                        .buttonStyle(.plain)
                        .disabled(!isEnabled)
                    }
                }
            }
        }
        .padding(14)
        .background(Color.black.opacity(0.75), in: RoundedRectangle(cornerRadius: 14))
        .overlay(RoundedRectangle(cornerRadius: 14).strokeBorder(Color.white.opacity(0.1), lineWidth: 1))
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
    let text: String

    var body: some View {
        Text(text)
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
