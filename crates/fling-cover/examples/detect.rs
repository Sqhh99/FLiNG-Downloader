//! Runs the cover model on image files: `cargo run -p fling-cover --example detect -- a.png b.jpg`

use fling_cover::{CoverDetector, OnnxCoverDetector};

fn main() {
    let model = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../resources/models/game-cover-v2.onnx");
    let detector = OnnxCoverDetector::load(&model).expect("model");
    for path in std::env::args().skip(1) {
        let decoded = match image::open(&path) {
            Ok(image) => image,
            Err(err) => {
                println!("{path}: decode failed: {err}");
                continue;
            }
        };
        println!(
            "{path}: {:?} -> {:?}",
            decoded.color(),
            detector.detect(&decoded.to_rgb8())
        );
    }
}
