import AVFoundation
import Observation
import Photos

// MARK: - Errors

/// Every way the camera pipeline can fail. Each case has a user-facing message,
/// so nothing fails silently.
nonisolated enum CameraError: LocalizedError, Equatable, Sendable {
    case cameraPermissionDenied
    case microphonePermissionDenied
    case photoLibraryPermissionDenied
    case cameraUnavailable
    case microphoneUnavailable
    case cannotAddCameraInput
    case cannotAddMicrophoneInput
    case cannotAddMovieOutput
    case recordingFailed(String)
    case savingFailed(String)
    case formatNotSupported(String)
    case formatChangeFailed(String)
    case settingsLockedWhileRecording
    case manualExposureNotSupported
    case exposureChangeFailed(String)

    var errorDescription: String? {
        switch self {
        case .cameraPermissionDenied:
            "Gradelux needs access to the camera to record video. Turn on Camera access in Settings."
        case .microphonePermissionDenied:
            "Gradelux needs access to the microphone to record audio. Turn on Microphone access in Settings."
        case .photoLibraryPermissionDenied:
            "Gradelux needs permission to add videos to your Photos library. Turn on Photos access in Settings."
        case .cameraUnavailable:
            "The rear wide-angle camera is not available on this device."
        case .microphoneUnavailable:
            "No microphone is available on this device."
        case .cannotAddCameraInput:
            "The camera could not be connected to the capture session."
        case .cannotAddMicrophoneInput:
            "The microphone could not be connected to the capture session."
        case .cannotAddMovieOutput:
            "The video recorder could not be connected to the capture session."
        case .recordingFailed(let reason):
            "Recording failed: \(reason)"
        case .savingFailed(let reason):
            "The video could not be saved to Photos: \(reason)"
        case .formatNotSupported(let setting):
            "This camera can't record \(setting)."
        case .formatChangeFailed(let reason):
            "The camera format could not be changed: \(reason)"
        case .settingsLockedWhileRecording:
            "Camera settings can't be changed while recording."
        case .manualExposureNotSupported:
            "This camera doesn't support manual ISO and shutter. Auto Exposure stays on."
        case .exposureChangeFailed(let reason):
            "Exposure could not be changed: \(reason)"
        }
    }

    /// Permission errors can be fixed by the user in the Settings app.
    var isPermissionError: Bool {
        switch self {
        case .cameraPermissionDenied, .microphonePermissionDenied, .photoLibraryPermissionDenied:
            true
        default:
            false
        }
    }
}

// MARK: - Recording state

enum RecordingState {
    case idle
    case starting
    case recording
    case finishing
}

// MARK: - CameraManager

/// Owns the AVFoundation capture pipeline.
///
/// Threading model:
/// - Everything the UI reads (`recordingState`, `elapsedSeconds`, errors...) lives on the main actor.
/// - Everything that touches `AVCaptureSession` runs on `sessionQueue`, a private serial queue,
///   because session configuration and `startRunning()` are slow and blocking.
///   Those methods are marked `nonisolated` and are only ever called on `sessionQueue`.
@MainActor
@Observable
final class CameraManager {

    // MARK: UI state (main actor)

    /// Set when the camera cannot be used at all (permissions, missing hardware, setup failure).
    private(set) var setupError: CameraError?

    /// Set for one-off problems (a recording or save failed). Shown as an alert.
    var alertMessage: String?

    private(set) var isSessionRunning = false
    private(set) var recordingState: RecordingState = .idle
    private(set) var elapsedSeconds = 0
    private(set) var isSaving = false
    private(set) var showSavedConfirmation = false
    private(set) var savedConfirmationText = "Saved"

    /// Resolution / frame-rate combinations the current camera can really record.
    private(set) var capabilities = CaptureCapabilities.empty

    /// What the user asked for.
    private(set) var selectedResolution: VideoResolution = .hd
    private(set) var selectedFrameRate: FrameRate = .fps30

    /// What the camera hardware is actually set to (read back after every change).
    /// The settings bar displays this, not the request, so it never shows a value that isn't real.
    private(set) var activeSettings: ActiveCaptureSettings?

    private(set) var isApplyingFormat = false

    /// Format changes are only allowed while the camera is running and not recording.
    var canChangeFormat: Bool {
        isSessionRunning && recordingState == .idle && !isApplyingFormat
    }

    // MARK: Exposure state

    /// Auto Exposure, or manual ISO + shutter angle.
    private(set) var exposureMode: ExposureMode = .auto
    /// Manual values. nil while switching from Auto (the camera derives them so brightness doesn't jump).
    private(set) var manualISO: Float?
    private(set) var manualShutterAngle: ShutterAngle?
    /// Live ISO / shutter values read back from the camera a few times per second.
    private(set) var exposureReadout: ExposureReadout?

    /// Exposure changes are only allowed while the camera is running and not recording.
    var canChangeExposure: Bool {
        isSessionRunning && recordingState == .idle && !isApplyingFormat
    }

    /// The request matching the current exposure state.
    private var currentExposureRequest: ExposureRequest {
        switch exposureMode {
        case .auto: .auto
        case .manual: .manual(iso: manualISO, angle: manualShutterAngle)
        }
    }

    @ObservationIgnored private var isSendingExposure = false
    @ObservationIgnored private var exposureNeedsResend = false
    @ObservationIgnored private var exposureMonitorTask: Task<Void, Never>?

    var isRecording: Bool { recordingState == .recording }

    /// Elapsed time formatted as 00:00:00.
    var formattedElapsedTime: String {
        let hours = elapsedSeconds / 3600
        let minutes = (elapsedSeconds % 3600) / 60
        let seconds = elapsedSeconds % 60
        return String(format: "%02d:%02d:%02d", hours, minutes, seconds)
    }

    @ObservationIgnored private var timerTask: Task<Void, Never>?
    @ObservationIgnored private var savedConfirmationTask: Task<Void, Never>?
    /// Hardware settings at the moment recording started, used to verify the finished file.
    @ObservationIgnored private var settingsAtRecordingStart: ActiveCaptureSettings?

    // MARK: Capture objects (only touched on sessionQueue)

    /// The capture session. The preview layer reads it; all configuration happens on `sessionQueue`.
    nonisolated(unsafe) let session = AVCaptureSession()

    nonisolated private let sessionQueue = DispatchQueue(label: "com.gradelux.camera.session")
    nonisolated(unsafe) private let movieOutput = AVCaptureMovieFileOutput()
    @ObservationIgnored nonisolated(unsafe) private var videoDeviceInput: AVCaptureDeviceInput?
    @ObservationIgnored nonisolated(unsafe) private var audioDeviceInput: AVCaptureDeviceInput?
    @ObservationIgnored nonisolated(unsafe) private var isConfigured = false
    @ObservationIgnored nonisolated(unsafe) private var recordingDelegate: MovieRecordingDelegate?

    /// Rotation applied to recorded video. 90° = portrait for the rear camera.
    /// When landscape support is added, replace this with a value from
    /// `AVCaptureDevice.RotationCoordinator`.
    nonisolated static let portraitRotationAngle: CGFloat = 90

    // MARK: - Session lifecycle

    /// Requests permissions, configures the session (once) and starts it.
    func start() async {
        setupError = nil

        if let permissionError = await requestPermissions() {
            setupError = permissionError
            return
        }

        let resolution = selectedResolution
        let frameRate = selectedFrameRate
        let exposure = currentExposureRequest
        let result = await performOnSessionQueue {
            self.configureAndStartSession(resolution: resolution, frameRate: frameRate, exposure: exposure)
        }

        switch result {
        case .failure(let error):
            setupError = error
            isSessionRunning = false
        case .success(let setup):
            capabilities = setup.capabilities
            if let resolution = setup.resolution, let frameRate = setup.frameRate {
                selectedResolution = resolution
                selectedFrameRate = frameRate
            }
            activeSettings = setup.active
            handleExposureOutcome(setup.exposure, overwriteManualValues: true)
            isSessionRunning = true
            startExposureMonitoring()
            if let formatError = setup.formatError {
                alertMessage = formatError.localizedDescription
            }
        }
    }

    /// Stops the session. If a recording is in progress it is finished and still saved.
    func stop() {
        isSessionRunning = false
        stopExposureMonitoring()
        sessionQueue.async {
            self.stopSessionOnQueue()
        }
    }

    // MARK: - Resolution & frame rate

    func selectResolution(_ resolution: VideoResolution) {
        guard let frameRate = capabilities.bestFrameRate(for: resolution, preferring: selectedFrameRate) else {
            alertMessage = CameraError.formatNotSupported(resolution.label).localizedDescription
            return
        }
        applyFormat(resolution: resolution, frameRate: frameRate)
    }

    func selectFrameRate(_ frameRate: FrameRate) {
        applyFormat(resolution: selectedResolution, frameRate: frameRate)
    }

    private func applyFormat(resolution: VideoResolution, frameRate: FrameRate) {
        guard recordingState == .idle else {
            alertMessage = CameraError.settingsLockedWhileRecording.localizedDescription
            return
        }
        guard canChangeFormat else { return }
        guard capabilities.supports(resolution, frameRate) else {
            alertMessage = CameraError.formatNotSupported("\(resolution.label) at \(frameRate.label)").localizedDescription
            return
        }

        isApplyingFormat = true
        // Exposure is re-applied with the new frame rate, so the shutter angle stays the same.
        let exposure = currentExposureRequest
        Task {
            let result = await performOnSessionQueue {
                self.applyFormatOnQueue(resolution: resolution, frameRate: frameRate, exposure: exposure)
            }
            isApplyingFormat = false
            handleExposureOutcome(result.exposure, overwriteManualValues: true)
            if let readout = result.exposureReadout {
                exposureReadout = readout
            }
            if let active = result.active {
                activeSettings = active
                // Keep the selection in sync with what the hardware really reports.
                if let activeResolution = active.resolution, let activeFrameRate = active.lockedFrameRate {
                    selectedResolution = activeResolution
                    selectedFrameRate = activeFrameRate
                }
            }
            if let error = result.error {
                alertMessage = error.localizedDescription
            }
        }
    }

    // MARK: - Exposure (ISO & shutter angle)

    /// Turns Auto Exposure on or off. Turning it off locks the current brightness.
    func setAutoExposure(_ isAuto: Bool) {
        guard checkCanChangeExposure() else { return }
        if isAuto {
            exposureMode = .auto
        } else if exposureMode == .auto {
            exposureMode = .manual
            manualISO = nil          // both derived from the camera's current values
            manualShutterAngle = nil
        } else {
            return
        }
        sendExposure()
    }

    func setShutterAngle(_ angle: ShutterAngle) {
        guard checkCanChangeExposure() else { return }
        if exposureMode == .auto {
            exposureMode = .manual
            manualISO = nil          // ISO compensates so the image doesn't jump
        }
        manualShutterAngle = angle
        sendExposure()
    }

    func setISO(_ iso: Float) {
        guard checkCanChangeExposure() else { return }
        if exposureMode == .auto {
            exposureMode = .manual
            manualShutterAngle = nil // nearest preset to what Auto was using
        }
        manualISO = iso
        sendExposure()
    }

    private func checkCanChangeExposure() -> Bool {
        if recordingState != .idle {
            alertMessage = CameraError.settingsLockedWhileRecording.localizedDescription
            return false
        }
        return canChangeExposure
    }

    /// Sends the current exposure state to the camera.
    /// Slider drags produce many changes; only one is in flight at a time and the latest one wins.
    private func sendExposure() {
        guard !isSendingExposure else {
            exposureNeedsResend = true
            return
        }
        isSendingExposure = true
        let request = currentExposureRequest

        Task {
            let result = await performOnSessionQueue { self.applyExposureOnQueue(request) }
            isSendingExposure = false
            handleExposureOutcome(result.outcome, overwriteManualValues: false)
            if let readout = result.readout {
                exposureReadout = readout
            }
            if let error = result.error {
                alertMessage = error.localizedDescription
            }
            if exposureNeedsResend {
                exposureNeedsResend = false
                sendExposure()
            }
        }
    }

    /// Stores what the camera actually applied.
    /// `overwriteManualValues` is true after format changes (values may have been clamped to the new
    /// format's limits) and false after slider updates (so a newer slider value isn't replaced by an older one).
    private func handleExposureOutcome(_ outcome: ExposureOutcome?, overwriteManualValues: Bool) {
        switch outcome {
        case .manual(let iso, let angle)?:
            guard exposureMode == .manual else { return }
            if overwriteManualValues || manualISO == nil { manualISO = iso }
            if overwriteManualValues || manualShutterAngle == nil { manualShutterAngle = angle }
        case .manualNotSupported?:
            exposureMode = .auto
            manualISO = nil
            manualShutterAngle = nil
            alertMessage = CameraError.manualExposureNotSupported.localizedDescription
        case .auto?, nil:
            break
        }
    }

    /// Reads live ISO / shutter from the camera a few times per second (Auto changes them constantly).
    private func startExposureMonitoring() {
        exposureMonitorTask?.cancel()
        exposureMonitorTask = Task { [weak self] in
            while !Task.isCancelled {
                guard let self else { return }
                let readout = await self.performOnSessionQueue { self.readExposureOnQueue() }
                if let readout, readout != self.exposureReadout {
                    self.exposureReadout = readout
                }
                try? await Task.sleep(for: .milliseconds(300))
            }
        }
    }

    private func stopExposureMonitoring() {
        exposureMonitorTask?.cancel()
        exposureMonitorTask = nil
    }

    // MARK: - Recording

    /// Called by the record button.
    func toggleRecording() {
        switch recordingState {
        case .idle:
            startRecording()
        case .recording:
            stopRecording()
        case .starting, .finishing:
            break // Ignore taps while a transition is in progress.
        }
    }

    private func startRecording() {
        guard isSessionRunning, !isApplyingFormat else { return }
        recordingState = .starting
        settingsAtRecordingStart = activeSettings

        Task {
            if let error = await performOnSessionQueue({ self.startRecordingOnQueue() }) {
                recordingState = .idle
                alertMessage = error.localizedDescription
            }
            // On success, `recordingDidStart()` is called by the recording delegate.
        }
    }

    private func stopRecording() {
        recordingState = .finishing
        stopTimer()
        sessionQueue.async {
            self.stopRecordingOnQueue()
        }
    }

    /// Called (on the main actor) when the movie file output has actually begun writing.
    private func recordingDidStart() {
        recordingState = .recording
        startTimer()
    }

    /// Called (on the main actor) when the movie file output has finished writing.
    private func recordingDidFinish(fileURL: URL, error: (any Error)?) {
        stopTimer()
        recordingState = .idle

        if let error {
            // A recording can end with an "error" yet still be a complete, playable file
            // (for example when the app goes to the background). AVFoundation tells us via this key.
            let finishedSuccessfully = (error as NSError)
                .userInfo[AVErrorRecordingSuccessfullyFinishedKey] as? Bool ?? false

            guard finishedSuccessfully else {
                try? FileManager.default.removeItem(at: fileURL)
                alertMessage = CameraError.recordingFailed(error.localizedDescription).localizedDescription
                return
            }
        }

        Task {
            await saveRecording(at: fileURL)
        }
    }

    // MARK: - Timer

    private func startTimer() {
        timerTask?.cancel()
        elapsedSeconds = 0
        let startDate = Date()

        timerTask = Task { [weak self] in
            while !Task.isCancelled {
                try? await Task.sleep(for: .milliseconds(250))
                guard let self else { return }
                self.elapsedSeconds = Int(Date().timeIntervalSince(startDate))
            }
        }
    }

    private func stopTimer() {
        timerTask?.cancel()
        timerTask = nil
    }

    // MARK: - Saving to Photos

    private func saveRecording(at fileURL: URL) async {
        isSaving = true
        defer { isSaving = false }

        // Read the real resolution and frame rate from the file before it is moved into Photos.
        let info = await RecordedVideoInfo.load(from: fileURL)
        let expected = settingsAtRecordingStart

        do {
            try await Self.saveMovieToPhotos(at: fileURL)

            if let info {
                presentSavedConfirmation(text: "Saved · \(info.summary)")
                if let expected, !info.matches(expected) {
                    alertMessage = "The video was saved, but it was recorded as \(info.summary) instead of \(expected.resolutionLabel) · \(expected.frameRateLabel)."
                }
            } else {
                presentSavedConfirmation(text: "Saved")
                alertMessage = "The video was saved, but its resolution and frame rate could not be verified."
            }
        } catch {
            try? FileManager.default.removeItem(at: fileURL)
            alertMessage = CameraError.savingFailed(error.localizedDescription).localizedDescription
        }
    }

    /// Adds the movie file to the Photos library. The file is moved (not copied) into Photos.
    nonisolated private static func saveMovieToPhotos(at fileURL: URL) async throws {
        try await PHPhotoLibrary.shared().performChanges { @Sendable in
            let options = PHAssetResourceCreationOptions()
            options.shouldMoveFile = true
            let request = PHAssetCreationRequest.forAsset()
            request.addResource(with: .video, fileURL: fileURL, options: options)
        }
    }

    private func presentSavedConfirmation(text: String) {
        savedConfirmationTask?.cancel()
        savedConfirmationText = text
        showSavedConfirmation = true

        savedConfirmationTask = Task { [weak self] in
            try? await Task.sleep(for: .seconds(2.5))
            guard !Task.isCancelled, let self else { return }
            self.showSavedConfirmation = false
        }
    }

    // MARK: - Permissions

    private func requestPermissions() async -> CameraError? {
        guard await requestCaptureAccess(for: .video) else { return .cameraPermissionDenied }
        guard await requestCaptureAccess(for: .audio) else { return .microphonePermissionDenied }

        let photosStatus = await PHPhotoLibrary.requestAuthorization(for: .addOnly)
        guard photosStatus == .authorized || photosStatus == .limited else {
            return .photoLibraryPermissionDenied
        }
        return nil
    }

    /// Returns true if access is granted. Shows the system prompt only the first time.
    private func requestCaptureAccess(for mediaType: AVMediaType) async -> Bool {
        switch AVCaptureDevice.authorizationStatus(for: mediaType) {
        case .authorized:
            return true
        case .notDetermined:
            return await AVCaptureDevice.requestAccess(for: mediaType)
        default:
            return false // .denied or .restricted
        }
    }

    // MARK: - Session queue helpers

    /// Runs `work` on the serial session queue and returns its result to the caller.
    private func performOnSessionQueue<T: Sendable>(_ work: @escaping @Sendable () -> T) async -> T {
        await withCheckedContinuation { continuation in
            sessionQueue.async {
                continuation.resume(returning: work())
            }
        }
    }

    // MARK: - Session queue work (never call these on the main thread)

    nonisolated private func configureAndStartSession(resolution: VideoResolution,
                                                      frameRate: FrameRate,
                                                      exposure: ExposureRequest) -> Result<SessionSetup, CameraError> {
        if !isConfigured {
            if let error = configureSession() {
                return .failure(error)
            }
        }
        guard let device = videoDeviceInput?.device else {
            return .failure(.cameraUnavailable)
        }

        let capabilities = CaptureCapabilities.discover(for: device)

        // Use the requested format if this camera supports it, otherwise the closest supported one.
        var chosenResolution: VideoResolution?
        var chosenFrameRate: FrameRate?
        if capabilities.supports(resolution, frameRate) {
            chosenResolution = resolution
            chosenFrameRate = frameRate
        } else if let fallbackResolution = capabilities.resolutions.contains(resolution)
                    ? resolution : capabilities.resolutions.first {
            chosenResolution = fallbackResolution
            chosenFrameRate = capabilities.bestFrameRate(for: fallbackResolution, preferring: frameRate)
        }

        var formatError: CameraError?
        var exposureOutcome: ExposureOutcome?
        if let chosenResolution, let chosenFrameRate {
            let formatResult = applyFormatOnQueue(resolution: chosenResolution,
                                                  frameRate: chosenFrameRate,
                                                  exposure: exposure)
            formatError = formatResult.error
            exposureOutcome = formatResult.exposure
        } else {
            formatError = .formatNotSupported("HD or 4K at 24–120 FPS")
        }

        if !session.isRunning {
            session.startRunning() // Blocking — this is why we are on sessionQueue.
        }

        // Read back after the session is running, so we report what is really active.
        let active = ActiveCaptureSettings.read(from: device)
        if formatError == nil, let chosenResolution, let chosenFrameRate,
           !active.matches(chosenResolution, chosenFrameRate) {
            formatError = .formatChangeFailed(
                "the camera is running at \(active.resolutionLabel) · \(active.frameRateLabel).")
        }

        return .success(SessionSetup(
            capabilities: capabilities,
            resolution: chosenResolution,
            frameRate: chosenFrameRate,
            active: active,
            exposure: exposureOutcome,
            formatError: formatError
        ))
    }

    /// Switches the camera to the best format for `resolution` + `frameRate`, locks the frame
    /// duration, then reads the hardware back to confirm the change really happened.
    nonisolated private func applyFormatOnQueue(resolution: VideoResolution,
                                                frameRate: FrameRate,
                                                exposure: ExposureRequest) -> FormatChangeResult {
        guard let device = videoDeviceInput?.device else {
            return FormatChangeResult(active: nil, error: .cameraUnavailable)
        }
        // Never reconfigure during recording.
        guard !movieOutput.isRecording else {
            return FormatChangeResult(active: ActiveCaptureSettings.read(from: device),
                                      error: .settingsLockedWhileRecording)
        }
        guard let format = CaptureFormatSelector.bestFormat(for: device, resolution: resolution, frameRate: frameRate) else {
            return FormatChangeResult(active: ActiveCaptureSettings.read(from: device),
                                      error: .formatNotSupported("\(resolution.label) at \(frameRate.label)"))
        }
        var exposureOutcome: ExposureOutcome?

        session.beginConfiguration()
        do {
            try device.lockForConfiguration()
            // Setting activeFormat switches the session preset to .inputPriority automatically,
            // so the session keeps this format instead of overriding it with a preset.
            device.activeFormat = format
            // Equal min and max durations lock the camera to a constant frame rate.
            device.activeVideoMinFrameDuration = frameRate.frameDuration
            device.activeVideoMaxFrameDuration = frameRate.frameDuration
            // Re-apply exposure for the new frame rate and the new format's ISO / shutter limits.
            exposureOutcome = ExposureControl.apply(exposure, to: device)
            device.unlockForConfiguration()
        } catch {
            session.commitConfiguration()
            return FormatChangeResult(active: ActiveCaptureSettings.read(from: device),
                                      error: .formatChangeFailed(error.localizedDescription))
        }
        session.commitConfiguration()

        // Verify the hardware and the recording output.
        let active = ActiveCaptureSettings.read(from: device)
        let readout = ExposureReadout.read(from: device)
        guard active.matches(resolution, frameRate) else {
            return FormatChangeResult(active: active, exposure: exposureOutcome, exposureReadout: readout,
                                      error: .formatChangeFailed(
                "the camera is running at \(active.resolutionLabel) · \(active.frameRateLabel)."))
        }
        guard let connection = movieOutput.connection(with: .video), connection.isEnabled else {
            return FormatChangeResult(active: active, exposure: exposureOutcome, exposureReadout: readout,
                                      error: .formatChangeFailed(
                "the video recorder is not connected for this format."))
        }
        return FormatChangeResult(active: active, exposure: exposureOutcome, exposureReadout: readout, error: nil)
    }

    /// Applies an exposure change (ISO / shutter angle / Auto) to the hardware.
    nonisolated private func applyExposureOnQueue(_ request: ExposureRequest) -> ExposureChangeResult {
        guard let device = videoDeviceInput?.device else {
            return ExposureChangeResult(outcome: nil, readout: nil, error: .cameraUnavailable)
        }
        // Never change settings during recording.
        guard !movieOutput.isRecording else {
            return ExposureChangeResult(outcome: nil, readout: ExposureReadout.read(from: device),
                                        error: .settingsLockedWhileRecording)
        }
        do {
            try device.lockForConfiguration()
        } catch {
            return ExposureChangeResult(outcome: nil, readout: ExposureReadout.read(from: device),
                                        error: .exposureChangeFailed(error.localizedDescription))
        }
        let outcome = ExposureControl.apply(request, to: device)
        device.unlockForConfiguration()
        return ExposureChangeResult(outcome: outcome, readout: ExposureReadout.read(from: device), error: nil)
    }

    nonisolated private func readExposureOnQueue() -> ExposureReadout? {
        guard let device = videoDeviceInput?.device else { return nil }
        return ExposureReadout.read(from: device)
    }

    nonisolated private func configureSession() -> CameraError? {
        session.beginConfiguration()
        defer { session.commitConfiguration() }

        // No session preset: the resolution and frame rate are set on the camera itself
        // (see applyFormatOnQueue), which puts the session in .inputPriority mode.

        // Rear wide-angle camera.
        guard let camera = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: .back) else {
            return .cameraUnavailable
        }
        guard let cameraInput = try? AVCaptureDeviceInput(device: camera),
              session.canAddInput(cameraInput) else {
            removeAllInputsAndOutputs()
            return .cannotAddCameraInput
        }
        session.addInput(cameraInput)
        videoDeviceInput = cameraInput

        // Microphone.
        guard let microphone = AVCaptureDevice.default(for: .audio) else {
            removeAllInputsAndOutputs()
            return .microphoneUnavailable
        }
        guard let microphoneInput = try? AVCaptureDeviceInput(device: microphone),
              session.canAddInput(microphoneInput) else {
            removeAllInputsAndOutputs()
            return .cannotAddMicrophoneInput
        }
        session.addInput(microphoneInput)
        audioDeviceInput = microphoneInput

        // Movie file output.
        guard session.canAddOutput(movieOutput) else {
            removeAllInputsAndOutputs()
            return .cannotAddMovieOutput
        }
        session.addOutput(movieOutput)

        isConfigured = true
        return nil
    }

    /// Leaves the session empty after a failed configuration so a retry starts clean.
    nonisolated private func removeAllInputsAndOutputs() {
        for input in session.inputs { session.removeInput(input) }
        for output in session.outputs { session.removeOutput(output) }
        videoDeviceInput = nil
        audioDeviceInput = nil
    }

    nonisolated private func stopSessionOnQueue() {
        if movieOutput.isRecording {
            movieOutput.stopRecording()
        }
        if session.isRunning {
            session.stopRunning()
        }
    }

    nonisolated private func startRecordingOnQueue() -> CameraError? {
        guard session.isRunning else {
            return .recordingFailed("The camera is not running.")
        }
        guard !movieOutput.isRecording else { return nil }
        guard let connection = movieOutput.connection(with: .video) else {
            return .recordingFailed("No video connection is available.")
        }

        if connection.isVideoRotationAngleSupported(Self.portraitRotationAngle) {
            connection.videoRotationAngle = Self.portraitRotationAngle
        }

        let fileURL = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString)
            .appendingPathExtension("mov")

        let delegate = MovieRecordingDelegate(
            onStart: { [weak self] in
                guard let self else { return }
                Task { @MainActor in self.recordingDidStart() }
            },
            onFinish: { [weak self] url, error in
                guard let self else { return }
                Task { @MainActor in self.recordingDidFinish(fileURL: url, error: error) }
            }
        )
        recordingDelegate = delegate
        movieOutput.startRecording(to: fileURL, recordingDelegate: delegate)
        return nil
    }

    nonisolated private func stopRecordingOnQueue() {
        if movieOutput.isRecording {
            movieOutput.stopRecording()
        }
    }
}

// MARK: - Session queue results

/// Result of starting the session (sent from the session queue to the main actor).
nonisolated private struct SessionSetup: Sendable {
    let capabilities: CaptureCapabilities
    let resolution: VideoResolution?
    let frameRate: FrameRate?
    let active: ActiveCaptureSettings
    let exposure: ExposureOutcome?
    let formatError: CameraError?
}

/// Result of a format change: what the hardware is now set to, and any error.
nonisolated private struct FormatChangeResult: Sendable {
    let active: ActiveCaptureSettings?
    var exposure: ExposureOutcome? = nil
    var exposureReadout: ExposureReadout? = nil
    let error: CameraError?
}

/// Result of an exposure change.
nonisolated private struct ExposureChangeResult: Sendable {
    let outcome: ExposureOutcome?
    let readout: ExposureReadout?
    let error: CameraError?
}

// MARK: - Recording delegate

/// Receives callbacks from AVCaptureMovieFileOutput (on a background queue)
/// and forwards them to CameraManager.
nonisolated private final class MovieRecordingDelegate: NSObject, AVCaptureFileOutputRecordingDelegate {
    private let onStart: @Sendable () -> Void
    private let onFinish: @Sendable (URL, (any Error)?) -> Void

    init(onStart: @escaping @Sendable () -> Void,
         onFinish: @escaping @Sendable (URL, (any Error)?) -> Void) {
        self.onStart = onStart
        self.onFinish = onFinish
        super.init()
    }

    func fileOutput(_ output: AVCaptureFileOutput,
                    didStartRecordingTo fileURL: URL,
                    from connections: [AVCaptureConnection]) {
        onStart()
    }

    func fileOutput(_ output: AVCaptureFileOutput,
                    didFinishRecordingTo outputFileURL: URL,
                    from connections: [AVCaptureConnection],
                    error: (any Error)?) {
        onFinish(outputFileURL, error)
    }
}
