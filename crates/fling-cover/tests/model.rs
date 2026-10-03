//! The real cover model over the sample trainer screenshots in
//! `tests/resources/fling_trainer_screenshot/` (the set the Qt build's
//! benchmark used). Every sample is a FLiNG screenshot with a cover in it.

#![cfg(feature = "onnx")]

use std::path::PathBuf;

use fling_cover::{CoverDetector, OnnxCoverDetector};

fn repo() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../..")
}

#[test]
fn finds_a_cover_in_every_sample() {
    let detector = OnnxCoverDetector::load(&repo().join("resources/models/game-cover-v2.onnx"))
        .expect("model loads");
    let dump = std::env::var_os("FLING_COVER_DUMP").map(PathBuf::from);
    let mut samples: Vec<_> =
        std::fs::read_dir(repo().join("tests/resources/fling_trainer_screenshot"))
            .unwrap()
            .map(|e| e.unwrap().path())
            .filter(|p| {
                p.extension()
                    .is_some_and(|e| e == "png" || e == "jpg" || e == "jpeg")
            })
            .collect();
    samples.sort();
    assert!(!samples.is_empty());

    for sample in samples {
        let image = image::open(&sample).unwrap().to_rgb8();
        let d = detector
            .detect(&image)
            .unwrap()
            .unwrap_or_else(|| panic!("no cover in {sample:?}"));
        println!("{:?}: {d:?}", sample.file_name().unwrap());
        assert!(d.confidence > 0.25);
        assert!(d.x + d.width <= image.width() as f32 && d.y + d.height <= image.height() as f32);
        // Box art is portrait-ish; reject degenerate boxes.
        assert!(
            d.width >= 40.0 && d.height >= 40.0,
            "tiny box in {sample:?}"
        );
        if let Some(dir) = &dump {
            let crop = image::imageops::crop_imm(
                &image,
                d.x as u32,
                d.y as u32,
                d.width as u32,
                d.height as u32,
            );
            crop.to_image()
                .save(dir.join(sample.file_name().unwrap()))
                .unwrap();
        }
    }
}
