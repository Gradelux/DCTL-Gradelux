import SwiftUI
import UIKit

/// Which settings panel is open.
private enum SettingsPanel {
    case resolution
    case frameRate
    case shutter
    case iso
    case whiteBalance
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
                        items: barItems,
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

    /// `4K | 24 FPS | 180° | ISO 400 | WB 5600K` — every value is read back from the camera hardware.
    private var barItems: [SettingsBarItem] {
        let isAuto = camera.exposureMode == .auto
        return [
            SettingsBarItem(panel: .resolution,
                            text: camera.activeSettings?.resolutionLabel ?? "—",
                            isAuto: false),
            SettingsBarItem(panel: .frameRate,
                            text: camera.activeSettings?.frameRateLabel ?? "—",
                            isAuto: false),
            SettingsBarItem(panel: .shutter,
                            text: camera.exposureReadout?.shutterAngleLabel ?? "—°",
                            isAuto: isAuto),
            SettingsBarItem(panel: .iso,
                            text: camera.exposureReadout?.isoLabel ?? "ISO —",
                            isAuto: isAuto),
            SettingsBarItem(panel: .whiteBalance,
                            text: "WB " + (camera.whiteBalanceReadout?.temperatureLabel ?? "—"),
                            isAuto: camera.whiteBalanceMode == .auto),
        ]
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
        case .shutter:
            ShutterPanel(
                frameRate: camera.activeSettings?.maxFrameRate ?? 0,
                isAuto: camera.exposureMode == .auto,
                selected: camera.exposureMode == .manual ? camera.manualShutterAngle : nil,
                readout: camera.exposureReadout,
                isEnabled: camera.canChangeExposure,
                onAutoChange: { camera.setAutoExposure($0) },
                onSelect: { camera.setShutterAngle($0) }
            )
        case .iso:
            ISOPanel(
                readout: camera.exposureReadout,
                isAuto: camera.exposureMode == .auto,
                manualISO: camera.manualISO,
                isEnabled: camera.canChangeExposure,
                onAutoChange: { camera.setAutoExposure($0) },
                onISOChange: { camera.setISO($0) }
            )
        case .whiteBalance:
            WhiteBalancePanel(
                readout: camera.whiteBalanceReadout,
                range: camera.whiteBalanceRange,
                isAuto: camera.whiteBalanceMode == .auto,
                manualTemperature: camera.manualTemperature,
                manualTint: camera.manualTint,
                isEnabled: camera.canChangeWhiteBalance,
                onAutoChange: { camera.setAutoWhiteBalance($0) },
                onTemperatureChange: { camera.setTemperature($0) },
                onTintChange: { camera.setTint($0) }
            )
        }
    }
}

// MARK: - Settings bar

private struct SettingsBarItem: Identifiable {
    let panel: SettingsPanel
    let text: String
    /// Shows a small red "A" when the value is controlled by Auto Exposure.
    let isAuto: Bool

    var id: SettingsPanel { panel }
}

/// Top status bar, e.g. `4K | 24 FPS | 180° | ISO 400 | WB 5600K`. Each value is tappable.
/// Centered when it fits; scrolls horizontally on narrow screens.
private struct SettingsBar: View {
    let items: [SettingsBarItem]
    let openPanel: SettingsPanel?
    let isEnabled: Bool
    let isBusy: Bool
    let onTap: (SettingsPanel) -> Void

    var body: some View {
        ViewThatFits(in: .horizontal) {
            bar
                .padding(.horizontal, 12)
            ScrollView(.horizontal, showsIndicators: false) {
                bar
                    .padding(.horizontal, 12)
            }
        }
    }

    private var bar: some View {
        HStack(spacing: 0) {
            ForEach(Array(items.enumerated()), id: \.element.id) { index, item in
                if index > 0 {
                    Divider()
                        .frame(height: 14)
                        .overlay(Color.white.opacity(0.35))
                }
                SettingChip(text: item.text,
                            isAuto: item.isAuto,
                            isOpen: openPanel == item.panel,
                            isEnabled: isEnabled) {
                    onTap(item.panel)
                }
            }
            if isBusy {
                ProgressView()
                    .controlSize(.small)
                    .tint(.white)
                    .padding(.trailing, 8)
            }
        }
        .background(Color.black.opacity(0.6), in: Capsule())
        .overlay(Capsule().strokeBorder(Color.white.opacity(0.12), lineWidth: 1))
    }
}

private struct SettingChip: View {
    let text: String
    let isAuto: Bool
    let isOpen: Bool
    let isEnabled: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: 3) {
                if isAuto {
                    Text("A")
                        .font(.system(size: 10, weight: .bold, design: .monospaced))
                        .foregroundStyle(Color.red)
                }
                Text(text)
                    .font(.system(size: 14, weight: .semibold, design: .monospaced))
                    .foregroundStyle(isOpen ? Color.red : Color.white)
                    .lineLimit(1)
                    .fixedSize()
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 8)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!isEnabled)
        .opacity(isEnabled ? 1 : 0.45)
    }
}

// MARK: - Panels

/// Shared dark card used by every settings panel.
private struct PanelContainer<Content: View>: View {
    let title: String
    @ViewBuilder let content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(title)
                .font(.system(size: 11, weight: .semibold, design: .monospaced))
                .foregroundStyle(.white.opacity(0.6))
            content
        }
        .padding(14)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Color.black.opacity(0.75), in: RoundedRectangle(cornerRadius: 14))
        .overlay(RoundedRectangle(cornerRadius: 14).strokeBorder(Color.white.opacity(0.1), lineWidth: 1))
    }
}

/// One selectable choice: big value on top, small detail underneath.
private struct OptionButton: View {
    let label: String
    let detail: String
    let isSelected: Bool
    let isEnabled: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            VStack(spacing: 2) {
                Text(label)
                    .font(.system(size: 17, weight: .semibold, design: .monospaced))
                Text(detail)
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
        PanelContainer(title: title) {
            if options.isEmpty {
                Text("No supported options on this camera.")
                    .font(.footnote)
                    .foregroundStyle(.white.opacity(0.8))
            } else {
                HStack(spacing: 8) {
                    ForEach(options) { option in
                        OptionButton(label: label(option),
                                     detail: detail(option),
                                     isSelected: option == selected,
                                     isEnabled: isEnabled) {
                            onSelect(option)
                        }
                    }
                }
            }
        }
    }
}

/// "AUTO … · ON / OFF" switch shown at the top of the exposure and white balance panels.
private struct AutoToggle: View {
    let title: String
    let isAuto: Bool
    let isEnabled: Bool
    let onChange: (Bool) -> Void

    var body: some View {
        Button {
            onChange(!isAuto)
        } label: {
            HStack(spacing: 6) {
                Circle()
                    .fill(isAuto ? Color.red : Color.white.opacity(0.25))
                    .frame(width: 8, height: 8)
                Text("\(title) · \(isAuto ? "ON" : "OFF")")
                    .font(.system(size: 12, weight: .semibold, design: .monospaced))
                    .foregroundStyle(.white)
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 6)
            .background(Color.white.opacity(0.06), in: Capsule())
            .overlay(Capsule().strokeBorder(isAuto ? Color.red : Color.white.opacity(0.15), lineWidth: 1))
        }
        .buttonStyle(.plain)
        .disabled(!isEnabled)
        .opacity(isEnabled ? 1 : 0.45)
    }
}

/// Shutter angle presets. Selecting one switches to manual exposure.
private struct ShutterPanel: View {
    let frameRate: Double
    let isAuto: Bool
    let selected: ShutterAngle?
    let readout: ExposureReadout?
    let isEnabled: Bool
    let onAutoChange: (Bool) -> Void
    let onSelect: (ShutterAngle) -> Void

    var body: some View {
        PanelContainer(title: "SHUTTER ANGLE · \(Int(frameRate.rounded())) FPS") {
            AutoToggle(title: "AUTO EXPOSURE", isAuto: isAuto, isEnabled: isEnabled, onChange: onAutoChange)

            if readout?.supportsManual == false {
                Text("This camera doesn't support manual shutter.")
                    .font(.footnote)
                    .foregroundStyle(.white.opacity(0.8))
            } else {
                HStack(spacing: 6) {
                    ForEach(ShutterAngle.allCases) { angle in
                        OptionButton(label: angle.label,
                                     detail: angle.shutterSpeedLabel(frameRate: frameRate),
                                     isSelected: angle == selected,
                                     isEnabled: isEnabled) {
                            onSelect(angle)
                        }
                    }
                }
            }

            if let note {
                Text(note)
                    .font(.system(size: 11, weight: .medium, design: .monospaced))
                    .foregroundStyle(isLimited ? Color.red : Color.white.opacity(0.6))
            }
        }
    }

    /// True when the hardware is using a different angle than the one selected.
    private var isLimited: Bool {
        guard !isAuto, let selected, let readout else { return false }
        return abs(readout.shutterAngle - Double(selected.rawValue)) > 1.5
    }

    private var note: String? {
        guard let readout else { return nil }
        if isAuto {
            return "Auto is using \(readout.shutterAngleLabel) · \(readout.shutterSpeedLabel)"
        }
        if isLimited {
            return "Camera limit: using \(readout.shutterAngleLabel) · \(readout.shutterSpeedLabel)"
        }
        return "Shutter \(readout.shutterSpeedLabel) s"
    }
}

/// ISO slider (logarithmic, so low ISOs get as much room as high ones).
private struct ISOPanel: View {
    let readout: ExposureReadout?
    let isAuto: Bool
    let manualISO: Float?
    let isEnabled: Bool
    let onAutoChange: (Bool) -> Void
    let onISOChange: (Float) -> Void

    var body: some View {
        PanelContainer(title: "ISO") {
            AutoToggle(title: "AUTO EXPOSURE", isAuto: isAuto, isEnabled: isEnabled, onChange: onAutoChange)

            if let readout {
                if readout.supportsManual {
                    let displayISO = isAuto ? readout.iso : (manualISO ?? readout.iso)

                    HStack {
                        Text("ISO \(Int(displayISO.rounded()))")
                            .font(.system(size: 17, weight: .semibold, design: .monospaced))
                            .foregroundStyle(.white)
                        Spacer()
                        Text("\(Int(readout.minISO.rounded()))–\(Int(readout.maxISO.rounded()))")
                            .font(.system(size: 11, weight: .medium, design: .monospaced))
                            .foregroundStyle(.white.opacity(0.6))
                    }

                    Slider(
                        value: Binding(
                            get: { position(of: displayISO, in: readout) },
                            set: { onISOChange(iso(at: $0, in: readout)) }
                        ),
                        in: 0...1
                    )
                    .tint(.red)
                    .disabled(!isEnabled)

                    if isAuto {
                        Text("Moving the slider switches to manual exposure.")
                            .font(.system(size: 11, weight: .medium, design: .monospaced))
                            .foregroundStyle(.white.opacity(0.6))
                    }
                } else {
                    Text("This camera doesn't support manual ISO.")
                        .font(.footnote)
                        .foregroundStyle(.white.opacity(0.8))
                }
            } else {
                Text("Reading camera…")
                    .font(.footnote)
                    .foregroundStyle(.white.opacity(0.8))
            }
        }
    }

    private func position(of iso: Float, in readout: ExposureReadout) -> Double {
        let minISO = Double(readout.minISO)
        let maxISO = Double(readout.maxISO)
        guard maxISO > minISO, minISO > 0 else { return 0 }
        let clamped = min(max(Double(iso), minISO), maxISO)
        return log(clamped / minISO) / log(maxISO / minISO)
    }

    private func iso(at position: Double, in readout: ExposureReadout) -> Float {
        let minISO = Double(readout.minISO)
        let maxISO = Double(readout.maxISO)
        guard maxISO > minISO, minISO > 0 else { return readout.minISO }
        return Float(minISO * pow(maxISO / minISO, position))
    }
}

/// Kelvin and tint sliders. These change the camera's real white balance gains (not a color overlay).
private struct WhiteBalancePanel: View {
    let readout: WhiteBalanceReadout?
    let range: WhiteBalanceRange
    let isAuto: Bool
    let manualTemperature: Float?
    let manualTint: Float?
    let isEnabled: Bool
    let onAutoChange: (Bool) -> Void
    let onTemperatureChange: (Float) -> Void
    let onTintChange: (Float) -> Void

    var body: some View {
        PanelContainer(title: "WHITE BALANCE") {
            AutoToggle(title: "AUTO WB", isAuto: isAuto, isEnabled: isEnabled, onChange: onAutoChange)

            if let readout {
                if readout.supportsManual {
                    let temperature = isAuto ? readout.temperature : (manualTemperature ?? readout.temperature)
                    let tint = isAuto ? readout.tint : (manualTint ?? readout.tint)

                    // Kelvin
                    HStack {
                        Text("\(Int(temperature.rounded()))K")
                            .font(.system(size: 17, weight: .semibold, design: .monospaced))
                            .foregroundStyle(.white)
                        Spacer()
                        Text("\(Int(range.minTemperature))–\(Int(range.maxTemperature))K")
                            .font(.system(size: 11, weight: .medium, design: .monospaced))
                            .foregroundStyle(.white.opacity(0.6))
                    }
                    Slider(
                        value: Binding(
                            get: { min(max(temperature, range.minTemperature), range.maxTemperature) },
                            set: { onTemperatureChange($0) }
                        ),
                        in: range.temperatureRange,
                        step: 50
                    )
                    .tint(.red)
                    .disabled(!isEnabled)

                    // Tint
                    HStack {
                        Text("TINT \(Int(tint.rounded()) > 0 ? "+" : "")\(Int(tint.rounded()))")
                            .font(.system(size: 15, weight: .semibold, design: .monospaced))
                            .foregroundStyle(.white)
                        Spacer()
                        Text(range.positiveTintIsGreen ? "M ← → G" : "G ← → M")
                            .font(.system(size: 11, weight: .medium, design: .monospaced))
                            .foregroundStyle(.white.opacity(0.6))
                    }
                    Slider(
                        value: Binding(
                            get: { min(max(tint, WhiteBalanceRange.tintRange.lowerBound),
                                       WhiteBalanceRange.tintRange.upperBound) },
                            set: { onTintChange($0) }
                        ),
                        in: WhiteBalanceRange.tintRange,
                        step: 1
                    )
                    .tint(.red)
                    .disabled(!isEnabled)

                    if let status = statusNote(readout: readout, temperature: temperature, tint: tint) {
                        Text(status.text)
                            .font(.system(size: 11, weight: .medium, design: .monospaced))
                            .foregroundStyle(status.isWarning ? Color.red : Color.white.opacity(0.6))
                    }
                } else {
                    Text("This camera doesn't support manual white balance.")
                        .font(.footnote)
                        .foregroundStyle(.white.opacity(0.8))
                }
            } else {
                Text("Reading camera…")
                    .font(.footnote)
                    .foregroundStyle(.white.opacity(0.8))
            }
        }
    }

    private func statusNote(readout: WhiteBalanceReadout,
                            temperature: Float,
                            tint: Float) -> (text: String, isWarning: Bool)? {
        if isAuto {
            return ("Moving a slider locks white balance (manual).", false)
        }
        // The hardware gains hit their limit, so the camera can't reach the selected values.
        if abs(readout.temperature - temperature) > 150 || abs(readout.tint - tint) > 5 {
            return ("Camera limit: using \(readout.temperatureLabel) · tint \(readout.tintLabel)", true)
        }
        return nil
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
