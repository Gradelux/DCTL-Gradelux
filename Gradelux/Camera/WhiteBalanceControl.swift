import AVFoundation

// MARK: - Mode and requests

enum WhiteBalanceMode {
    case auto
    case manual
}

/// What the app asks the camera to do with white balance.
/// In `.manual`, a nil value means "keep what the camera is using right now",
/// which is how Auto → Manual switches happen without a color shift.
nonisolated enum WhiteBalanceRequest: Sendable {
    case auto
    case manual(temperature: Float?, tint: Float?)
}

/// What was actually applied to the hardware.
nonisolated enum WhiteBalanceOutcome: Sendable {
    case auto
    /// `temperature` / `tint`: the values to keep for the sliders.
    /// `applied`: what the locked hardware gains really correspond to (after clamping).
    case manual(temperature: Float, tint: Float, applied: WhiteBalanceReadout)
    case manualNotSupported
}

// MARK: - Device range

/// The Kelvin range this camera can reach without its RGB gains hitting hardware limits.
nonisolated struct WhiteBalanceRange: Equatable, Sendable {
    let minTemperature: Float
    let maxTemperature: Float
    /// True if a positive tint value makes the image greener on this device
    /// (measured from the device's own gain conversion, not assumed).
    let positiveTintIsGreen: Bool

    /// AVFoundation's documented tint range.
    static let tintRange: ClosedRange<Float> = -150...150

    static let fallback = WhiteBalanceRange(minTemperature: 2500, maxTemperature: 8000, positiveTintIsGreen: true)

    var temperatureRange: ClosedRange<Float> { minTemperature...maxTemperature }

    /// Call on the session queue.
    static func discover(for device: AVCaptureDevice) -> WhiteBalanceRange {
        guard device.isLockingWhiteBalanceWithCustomDeviceGainsSupported else { return .fallback }
        let maxGain = device.maxWhiteBalanceGain

        // A temperature is usable if its gains don't need clamping (clamping would change the color).
        func isReachable(_ temperature: Float) -> Bool {
            let values = AVCaptureDevice.WhiteBalanceTemperatureAndTintValues(temperature: temperature, tint: 0)
            let gains = device.deviceWhiteBalanceGains(for: values)
            return [gains.redGain, gains.greenGain, gains.blueGain].allSatisfy { gain in
                gain.isFinite && gain >= 0.999 && gain <= maxGain + 0.001
            }
        }

        let start: Float = 5000
        guard isReachable(start) else { return .fallback }

        var low = start
        while low - 100 >= 1500, isReachable(low - 100) { low -= 100 }
        var high = start
        while high + 100 <= 15000, isReachable(high + 100) { high += 100 }
        guard high - low >= 1000 else { return .fallback }

        // Which way does positive tint push the image? Compare green gain against red/blue.
        let neutral = device.deviceWhiteBalanceGains(
            for: AVCaptureDevice.WhiteBalanceTemperatureAndTintValues(temperature: start, tint: 0))
        let tinted = device.deviceWhiteBalanceGains(
            for: AVCaptureDevice.WhiteBalanceTemperatureAndTintValues(temperature: start, tint: 50))
        func greenRatio(_ g: AVCaptureDevice.WhiteBalanceGains) -> Float {
            g.greenGain / max((g.redGain + g.blueGain) / 2, 0.001)
        }

        return WhiteBalanceRange(minTemperature: low,
                                 maxTemperature: high,
                                 positiveTintIsGreen: greenRatio(tinted) >= greenRatio(neutral))
    }
}

// MARK: - Live read-back

/// White balance read from the camera hardware (live, also while in Auto).
nonisolated struct WhiteBalanceReadout: Equatable, Sendable {
    let isAuto: Bool
    let temperature: Float
    let tint: Float
    let supportsManual: Bool

    /// Rounded to 50 K, like a cinema camera display.
    var temperatureLabel: String { "\(Int((temperature / 50).rounded()) * 50)K" }
    var tintLabel: String {
        let value = Int(tint.rounded())
        return value > 0 ? "+\(value)" : "\(value)"
    }

    static func read(from device: AVCaptureDevice) -> WhiteBalanceReadout {
        let gains = WhiteBalanceControl.clamped(device.deviceWhiteBalanceGains, maxGain: device.maxWhiteBalanceGain)
        let values = device.temperatureAndTintValues(for: gains)
        return WhiteBalanceReadout(
            isAuto: device.whiteBalanceMode != .locked,
            temperature: values.temperature,
            tint: values.tint,
            supportsManual: device.isLockingWhiteBalanceWithCustomDeviceGainsSupported
        )
    }
}

// MARK: - Applying white balance to the hardware

nonisolated enum WhiteBalanceControl {

    /// Applies `request` to `device`.
    /// The caller MUST hold `device.lockForConfiguration()` and call this on the session queue.
    static func apply(_ request: WhiteBalanceRequest, to device: AVCaptureDevice) -> WhiteBalanceOutcome {
        switch request {
        case .auto:
            if device.isWhiteBalanceModeSupported(.continuousAutoWhiteBalance) {
                device.whiteBalanceMode = .continuousAutoWhiteBalance
            }
            return .auto

        case .manual(let requestedTemperature, let requestedTint):
            guard device.isLockingWhiteBalanceWithCustomDeviceGainsSupported else { return .manualNotSupported }

            let maxGain = device.maxWhiteBalanceGain
            let currentGains = clamped(device.deviceWhiteBalanceGains, maxGain: maxGain)

            let gains: AVCaptureDevice.WhiteBalanceGains
            if requestedTemperature == nil && requestedTint == nil {
                // Leaving Auto: lock exactly the gains Auto is using now — no color shift.
                gains = currentGains
            } else {
                let current = device.temperatureAndTintValues(for: currentGains)
                let target = AVCaptureDevice.WhiteBalanceTemperatureAndTintValues(
                    temperature: requestedTemperature ?? current.temperature,
                    tint: requestedTint ?? current.tint
                )
                gains = clamped(device.deviceWhiteBalanceGains(for: target), maxGain: maxGain)
            }

            device.setWhiteBalanceModeLocked(with: gains, completionHandler: nil)

            // Report what the gains we just set correspond to. (Reading the device right away could
            // still return the previous gains, because the hardware applies them a frame later.)
            let applied = device.temperatureAndTintValues(for: gains)
            let appliedReadout = WhiteBalanceReadout(isAuto: false,
                                                     temperature: applied.temperature,
                                                     tint: applied.tint,
                                                     supportsManual: true)
            return .manual(temperature: requestedTemperature ?? applied.temperature,
                           tint: requestedTint ?? applied.tint,
                           applied: appliedReadout)
        }
    }

    /// Every channel must be between 1.0 and `maxWhiteBalanceGain`, or AVFoundation throws an exception.
    static func clamped(_ gains: AVCaptureDevice.WhiteBalanceGains,
                        maxGain: Float) -> AVCaptureDevice.WhiteBalanceGains {
        func clamp(_ value: Float) -> Float {
            guard value.isFinite else { return 1 }
            return min(max(value, 1), maxGain)
        }
        var result = gains
        result.redGain = clamp(gains.redGain)
        result.greenGain = clamp(gains.greenGain)
        result.blueGain = clamp(gains.blueGain)
        return result
    }
}
