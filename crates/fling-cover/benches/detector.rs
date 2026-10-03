//! Cover detection over the sample screenshots (replaces the Qt build's
//! Google Benchmark `CoverExtractor` cases).
//!
//! `cargo bench -p fling-cover`

use std::path::PathBuf;

use criterion::{Criterion, criterion_group, criterion_main};
use fling_cover::{CoverDetector, OnnxCoverDetector};

fn repo() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../..")
}

fn detect(c: &mut Criterion) {
    let detector = OnnxCoverDetector::load(&repo().join("resources/models/game-cover-v2.onnx"))
        .expect("model");
    let samples = repo().join("tests/resources/fling_trainer_screenshot");
    let mut group = c.benchmark_group("CoverExtractor");
    for entry in std::fs::read_dir(samples).unwrap() {
        let path = entry.unwrap().path();
        let name = path.file_name().unwrap().to_string_lossy().into_owned();
        let image = image::open(&path).unwrap().to_rgb8();
        group.bench_function(name, |b| b.iter(|| detector.detect(&image).unwrap()));
    }
    group.finish();
}

criterion_group!(benches, detect);
criterion_main!(benches);
