import AVFoundation
import CoreMedia

// MARK: - Resolution

/// Recording resolutions Gradelux offers. Sizes are in sensor (landscape) orientation.
nonisolated enum VideoResolution: String, CaseIterable, Identifiable, Sendable {
    case hd
    case uhd4K

    var id: Self { self }

    var label: String {
        switch self {
        case .hd: "HD"
        case .uhd4K: "4K"
        }
    }

    var detail: String {
        switch self {
        case .hd: "1920 × 1080"
        case .uhd4K: "3840 × 2160"
        }
    }

    var width: Int {
        switch self {
        case .hd: 1920
        case .uhd4K: 3840
        }
    }

    var height: Int {
        switch self {
        case .hd: 1080
        case .uhd4K: 2160
        }
    }

    /// Matches a width/height pair in either orientation (a portrait file may report 2160 × 3840).
    init?(width: Int, height: Int) {
        let long = max(width, height)
        let short = min(width, height)
        guard let match = Self.allCases.first(where: { $0.width == long && $0.height == short }) else {
            return nil
        }
        self = match
    }
}

// MARK: - Frame rate

nonisolated enum FrameRate: Int, CaseIterable, Identifiable, Sendable {
    case fps24 = 24
    case fps25 = 25
    case fps30 = 30
    case fps60 = 60
    case fps120 = 120

    var id: Self { self }

    var label: String { "\(rawValue) FPS" }

    /// Exact frame duration, e.g. 1/24 s.
    var frameDuration: CMTime {
        CMTime(value: 1, timescale: CMTimeScale(rawValue))
    }

    /// True if `measured` frames per second is this frame rate (allows for rounding).
    func matches(_ measured: Double) -> Bool {
        abs(measured - Double(rawValue)) < 0.5
    }
}

// MARK: - Capabilities

/// Which resolution / frame-rate combinations the current camera can really record.
nonisolated struct CaptureCapabilities: Equatable, Sendable {
    var frameRatesByResolution: [VideoResolution: [FrameRate]] = [:]

    static let empty = CaptureCapabilities()

    /// Resolutions with at least one supported frame rate.
    var resolutions: [VideoResolution] {
        VideoResolution.allCases.filter { !frameRates(for: $0).isEmpty }
    }

    func frameRates(for resolution: VideoResolution) -> [FrameRate] {
        frameRatesByResolution[resolution] ?? []
    }

    func supports(_ resolution: VideoResolution, _ frameRate: FrameRate) -> Bool {
        frameRates(for: resolution).contains(frameRate)
    }

    /// Picks the frame rate to use after switching resolution:
    /// keep the current one if possible, otherwise the closest lower one, otherwise the lowest available.
    func bestFrameRate(for resolution: VideoResolution, preferring preferred: FrameRate) -> FrameRate? {
        let available = frameRates(for: resolution)
        if available.contains(preferred) { return preferred }
        if let lower = available.last(where: { $0.rawValue < preferred.rawValue }) { return lower }
        return available.first
    }

    /// Inspects every format the device offers. Call on the session queue.
    static func discover(for device: AVCaptureDevice) -> CaptureCapabilities {
        var result = CaptureCapabilities()
        for resolution in VideoResolution.allCases {
            let rates = FrameRate.allCases.filter {
                CaptureFormatSelector.bestFormat(for: device, resolution: resolution, frameRate: $0) != nil
            }
            if !rates.isEmpty {
                result.frameRatesByResolution[resolution] = rates
            }
        }
        return result
    }
}

// MARK: - Active (read-back) settings

/// What the camera hardware is *actually* set to, read back after a change.
nonisolated struct ActiveCaptureSettings: Equatable, Sendable {
    let width: Int
    let height: Int
    let minFrameRate: Double
    let maxFrameRate: Double

    var resolution: VideoResolution? { VideoResolution(width: width, height: height) }

    /// The frame rate, if the camera is locked to one exact rate.
    var lockedFrameRate: FrameRate? {
        guard abs(maxFrameRate - minFrameRate) < 0.01 else { return nil }
        return FrameRate.allCases.first { $0.matches(maxFrameRate) }
    }

    var resolutionLabel: String { resolution?.label ?? "\(width)×\(height)" }
    var frameRateLabel: String { "\(Int(maxFrameRate.rounded())) FPS" }

    func matches(_ resolution: VideoResolution, _ frameRate: FrameRate) -> Bool {
        self.resolution == resolution && lockedFrameRate == frameRate
    }

    static func read(from device: AVCaptureDevice) -> ActiveCaptureSettings {
        let dimensions = CMVideoFormatDescriptionGetDimensions(device.activeFormat.formatDescription)
        let shortest = device.activeVideoMinFrameDuration.seconds // shortest frame = highest fps
        let longest = device.activeVideoMaxFrameDuration.seconds
        return ActiveCaptureSettings(
            width: Int(dimensions.width),
            height: Int(dimensions.height),
            minFrameRate: longest > 0 ? 1 / longest : 0,
            maxFrameRate: shortest > 0 ? 1 / shortest : 0
        )
    }
}

// MARK: - Recorded file (verified after recording)

/// Resolution and frame rate read from the finished movie file itself.
nonisolated struct RecordedVideoInfo: Sendable {
    let width: Int
    let height: Int
    let frameRate: Double

    var resolution: VideoResolution? { VideoResolution(width: width, height: height) }

    var summary: String {
        let size = resolution?.label ?? "\(max(width, height))×\(min(width, height))"
        return "\(size) · \(Int(frameRate.rounded())) FPS"
    }

    func matches(_ settings: ActiveCaptureSettings) -> Bool {
        resolution == settings.resolution && abs(frameRate - settings.maxFrameRate) < 1
    }

    /// Reads the first video track of the file at `url`. Returns nil if it cannot be read.
    static func load(from url: URL) async -> RecordedVideoInfo? {
        let asset = AVURLAsset(url: url)
        guard let track = try? await asset.loadTracks(withMediaType: .video).first,
              let values = try? await track.load(.naturalSize, .nominalFrameRate) else {
            return nil
        }
        return RecordedVideoInfo(
            width: Int(values.0.width.rounded()),
            height: Int(values.0.height.rounded()),
            frameRate: Double(values.1)
        )
    }
}

// MARK: - Format selection

/// Finds the device format to use for a resolution + frame rate.
nonisolated enum CaptureFormatSelector {

    /// Standard 8-bit 4:2:0 formats (video range and full range).
    /// 10-bit / Log formats will be added in a later milestone.
    private static let supportedPixelFormats: Set<FourCharCode> = [
        kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
        kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
    ]

    /// Returns the best matching format, or nil if this device can't do that combination.
    static func bestFormat(for device: AVCaptureDevice,
                           resolution: VideoResolution,
                           frameRate: FrameRate) -> AVCaptureDevice.Format? {
        let fps = Double(frameRate.rawValue)

        let candidates = device.formats.filter { format in
            let description = format.formatDescription
            let dimensions = CMVideoFormatDescriptionGetDimensions(description)
            guard Int(dimensions.width) == resolution.width,
                  Int(dimensions.height) == resolution.height,
                  supportedPixelFormats.contains(CMFormatDescriptionGetMediaSubType(description)) else {
                return false
            }
            return format.videoSupportedFrameRateRanges.contains { range in
                range.minFrameRate <= fps + 0.01 && fps - 0.01 <= range.maxFrameRate
            }
        }

        // Preference order:
        // 1. Not binned (binned formats trade image quality for speed).
        // 2. Video-range 420v (the standard for video recording).
        // 3. Lowest maximum frame rate (the format designed for this rate, not a high-speed one).
        return candidates.min { a, b in
            if a.isVideoBinned != b.isVideoBinned { return !a.isVideoBinned }
            let aVideoRange = CMFormatDescriptionGetMediaSubType(a.formatDescription) == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
            let bVideoRange = CMFormatDescriptionGetMediaSubType(b.formatDescription) == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
            if aVideoRange != bVideoRange { return aVideoRange }
            return maxFrameRate(of: a) < maxFrameRate(of: b)
        }
    }

    private static func maxFrameRate(of format: AVCaptureDevice.Format) -> Double {
        format.videoSupportedFrameRateRanges.map(\.maxFrameRate).max() ?? 0
    }
}
