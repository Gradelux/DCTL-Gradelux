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

        if let configurationError = await performOnSessionQueue({ self.configureAndStartSession() }) {
            setupError = configurationError
            isSessionRunning = false
            return
        }

        isSessionRunning = true
    }

    /// Stops the session. If a recording is in progress it is finished and still saved.
    func stop() {
        isSessionRunning = false
        sessionQueue.async {
            self.stopSessionOnQueue()
        }
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
        guard isSessionRunning else { return }
        recordingState = .starting

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

        do {
            try await Self.saveMovieToPhotos(at: fileURL)
            presentSavedConfirmation()
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

    private func presentSavedConfirmation() {
        savedConfirmationTask?.cancel()
        showSavedConfirmation = true

        savedConfirmationTask = Task { [weak self] in
            try? await Task.sleep(for: .seconds(1.5))
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

    nonisolated private func configureAndStartSession() -> CameraError? {
        if !isConfigured {
            if let error = configureSession() {
                return error
            }
        }
        if !session.isRunning {
            session.startRunning() // Blocking — this is why we are on sessionQueue.
        }
        return nil
    }

    nonisolated private func configureSession() -> CameraError? {
        session.beginConfiguration()
        defer { session.commitConfiguration() }

        if session.canSetSessionPreset(.high) {
            session.sessionPreset = .high
        }

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
