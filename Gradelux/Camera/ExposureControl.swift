import AVFoundation
import CoreMedia

// MARK: - Shutter angle

/// Cinema shutter angles. Exposure time = (angle / 360) × (1 / fps).
nonisolated enum ShutterAngle: Int, CaseIterable, Identifiable, Sendable {
    case deg45 = 45
    case deg90 = 90
    case deg180 = 180
    case deg270 = 270
    case deg360 = 360

    var id: Self { self }

    var label: String { "\(rawValue)°" }

    /// Exposure time in seconds for a given frame duration (1 / fps).
    func exposureSeconds(frameDuration: Double) -> Double {
        Double(rawValue) / 360 * frameDuration
    }

    /// Shutter speed text, e.g. "1/48" for 180° at 24 fps.
    func shutterSpeedLabel(frameRate: Double) -> String {
        guard frameRate > 0 else { return "—" }
        let denominator = frameRate * 360 / Double(rawValue)
        return "1/\(Int(denominator.rounded()))"
    }

    /// The preset closest to `angle` (compared on a logarithmic scale, like exposure stops).
    static func nearest(to angle: Double) -> ShutterAngle {
        guard angle.isFinite, angle > 0 else { return .deg180 }
        return allCases.min { a, b in
            abs(log2(Double(a.rawValue) / angle)) < abs(log2(Double(b.rawValue) / angle))
        } ?? .deg180
    }
}

// MARK: - Exposure mode and requests

enum ExposureMode {
    case auto
    case manual
}

/// What the app asks the camera to do with exposure.
/// In `.manual`, a nil value means "derive it from what the camera is doing right now",
/// which is how Auto → Manual switches happen without a visible jump in brightness.
nonisolated enum ExposureRequest: Sendable {
    case auto
    case manual(iso: Float?, angle: ShutterAngle?)
}

/// What was actually applied to the hardware.
nonisolated enum ExposureOutcome: Sendable {
    case auto
    case manual(iso: Float, angle: ShutterAngle)
    case manualNotSupported
}

// MARK: - Live read-back

/// Exposure values read from the camera hardware (live, also while in Auto).
nonisolated struct ExposureReadout: Equatable, Sendable {
    let isAuto: Bool
    let iso: Float
    let exposureSeconds: Double
    let frameDuration: Double
    let minISO: Float
    let maxISO: Float
    let supportsManual: Bool

    /// The shutter angle the camera is really using right now.
    var shutterAngle: Double {
        frameDuration > 0 ? exposureSeconds / frameDuration * 360 : 0
    }

    var isoLabel: String { "ISO \(Int(iso.rounded()))" }
    var shutterAngleLabel: String { "\(Int(shutterAngle.rounded()))°" }
    var shutterSpeedLabel: String {
        exposureSeconds > 0 ? "1/\(Int((1 / exposureSeconds).rounded()))" : "—"
    }

    static func read(from device: AVCaptureDevice) -> ExposureReadout {
        ExposureReadout(
            isAuto: device.exposureMode != .custom,
            iso: device.iso,
            exposureSeconds: device.exposureDuration.seconds,
            frameDuration: device.activeVideoMaxFrameDuration.seconds,
            minISO: device.activeFormat.minISO,
            maxISO: device.activeFormat.maxISO,
            supportsManual: device.isExposureModeSupported(.custom)
        )
    }
}

// MARK: - Applying exposure to the hardware

nonisolated enum ExposureControl {

    /// Applies `request` to `device`.
    /// The caller MUST hold `device.lockForConfiguration()` and call this on the session queue.
    /// The device's frame rate must already be set, because the shutter angle depends on it.
    static func apply(_ request: ExposureRequest, to device: AVCaptureDevice) -> ExposureOutcome {
        switch request {
        case .auto:
            if device.isExposureModeSupported(.continuousAutoExposure) {
                device.exposureMode = .continuousAutoExposure
            }
            return .auto

        case .manual(let requestedISO, let requestedAngle):
            guard device.isExposureModeSupported(.custom) else { return .manualNotSupported }

            let format = device.activeFormat
            let frameDuration = device.activeVideoMaxFrameDuration
            let wasManual = device.exposureMode == .custom

            // What the camera is doing right now (used to avoid jumps when leaving Auto).
            let currentSeconds = device.exposureDuration.seconds
            let currentISO = device.iso
            let currentIsValid = currentSeconds.isFinite && currentSeconds > 0 && currentISO > 0

            let angle = requestedAngle
                ?? (currentIsValid
                    ? ShutterAngle.nearest(to: currentSeconds / frameDuration.seconds * 360)
                    : .deg180)

            let duration = clampedDuration(for: angle, frameDuration: frameDuration, format: format)

            var iso: Float
            if let requestedISO {
                iso = requestedISO
            } else if currentIsValid && !wasManual {
                // Leaving Auto: keep the same brightness by trading shutter time for ISO.
                iso = currentISO * Float(currentSeconds / duration.seconds)
            } else if currentIsValid {
                iso = currentISO
            } else {
                iso = format.minISO
            }
            iso = min(max(iso, format.minISO), format.maxISO)

            device.setExposureModeCustom(duration: duration, iso: iso, completionHandler: nil)
            return .manual(iso: iso, angle: angle)
        }
    }

    /// Exposure time for `angle`, limited to what the format allows and never longer than one frame.
    static func clampedDuration(for angle: ShutterAngle,
                                frameDuration: CMTime,
                                format: AVCaptureDevice.Format) -> CMTime {
        let seconds = angle.exposureSeconds(frameDuration: frameDuration.seconds)
        let wanted = CMTime(seconds: seconds, preferredTimescale: 1_000_000_000)
        let longest = CMTimeMinimum(format.maxExposureDuration, frameDuration)
        return CMTimeMaximum(format.minExposureDuration, CMTimeMinimum(wanted, longest))
    }
}
