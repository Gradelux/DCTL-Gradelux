import AVFoundation
import SwiftUI
import UIKit

/// Shows the live camera feed using AVCaptureVideoPreviewLayer.
/// SwiftUI cannot host a CALayer directly, so it is wrapped in a UIView.
struct CameraPreview: UIViewRepresentable {
    let session: AVCaptureSession

    func makeUIView(context: Context) -> PreviewView {
        let view = PreviewView()
        view.backgroundColor = .black
        view.videoPreviewLayer.session = session
        view.videoPreviewLayer.videoGravity = .resizeAspectFill
        return view
    }

    func updateUIView(_ uiView: PreviewView, context: Context) {
        uiView.applyRotation()
    }
}

/// A UIView whose backing layer *is* the preview layer, so it always matches the view's size.
final class PreviewView: UIView {
    override class var layerClass: AnyClass {
        AVCaptureVideoPreviewLayer.self
    }

    var videoPreviewLayer: AVCaptureVideoPreviewLayer {
        // Safe: layerClass guarantees the layer type.
        layer as! AVCaptureVideoPreviewLayer
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        applyRotation()
    }

    /// Keeps the preview upright in portrait.
    /// When landscape support is added, drive this from `AVCaptureDevice.RotationCoordinator`.
    func applyRotation() {
        let angle = CameraManager.portraitRotationAngle
        guard let connection = videoPreviewLayer.connection,
              connection.isVideoRotationAngleSupported(angle) else { return }
        if connection.videoRotationAngle != angle {
            connection.videoRotationAngle = angle
        }
    }
}
