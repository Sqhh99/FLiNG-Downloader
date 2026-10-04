//! The game-cover YOLO model (`game-cover-v2.onnx`, Ultralytics end-to-end
//! export: input `images [1,3,640,640]`, output `output0 [1,300,6]`) run with
//! ONNX Runtime.
//!
//! Pre- and post-processing reproduce YOLOs-CPP (`preprocessing.hpp`
//! `letterBoxToBlob` / `getScalePad`, `detection.hpp` `postprocessV10`), which
//! the Qt build used, so boxes match it.

use std::path::Path;

use image::RgbImage;
use ort::session::Session;
use ort::session::builder::GraphOptimizationLevel;
use ort::value::Tensor;
use parking_lot::Mutex;

use crate::{CoverDetector, Detection};

const INPUT_SIZE: usize = 640;
const CONFIDENCE_THRESHOLD: f32 = 0.25;
const PAD_VALUE: f32 = 114.0 / 255.0;

pub struct OnnxCoverDetector {
    /// `Session::run` needs `&mut`; inference is serialized, as in the Qt build.
    session: Mutex<Session>,
}

impl OnnxCoverDetector {
    pub fn load(model: &Path) -> Result<Self, String> {
        let threads = std::thread::available_parallelism()
            .map_or(1, |n| n.get())
            .min(6);
        let session = Session::builder()
            .map_err(|e| e.to_string())?
            .with_optimization_level(GraphOptimizationLevel::Level3)
            .map_err(|e| e.to_string())?
            .with_intra_threads(threads)
            .map_err(|e| e.to_string())?
            .commit_from_file(model)
            .map_err(|e| e.to_string())?;
        Ok(Self {
            session: Mutex::new(session),
        })
    }
}

/// Letterbox geometry: the resized size and the integer top/left padding.
#[derive(Debug, Clone, Copy, PartialEq)]
struct Letterbox {
    scale: f32,
    new_w: usize,
    new_h: usize,
    pad_left: usize,
    pad_top: usize,
}

fn letterbox(src_w: usize, src_h: usize) -> Letterbox {
    let target = INPUT_SIZE as f32;
    let scale = (target / src_h as f32).min(target / src_w as f32);
    let new_w = (src_w as f32 * scale).round() as usize;
    let new_h = (src_h as f32 * scale).round() as usize;
    // Ultralytics' asymmetric padding: round(d - 0.1).
    let pad_left = ((INPUT_SIZE - new_w) as f32 / 2.0 - 0.1).round().max(0.0) as usize;
    let pad_top = ((INPUT_SIZE - new_h) as f32 / 2.0 - 0.1).round().max(0.0) as usize;
    Letterbox {
        scale,
        new_w,
        new_h,
        pad_left,
        pad_top,
    }
}

/// Bilinear resize with OpenCV `INTER_LINEAR` sampling (pixel centers, edge
/// clamping, no antialiasing).
fn resize_bilinear(src: &RgbImage, new_w: usize, new_h: usize) -> Vec<[f32; 3]> {
    let (src_w, src_h) = (src.width() as usize, src.height() as usize);
    let sx = src_w as f32 / new_w as f32;
    let sy = src_h as f32 / new_h as f32;
    let raw = src.as_raw();
    let px = |x: usize, y: usize, c: usize| raw[(y * src_w + x) * 3 + c] as f32;
    let mut out = Vec::with_capacity(new_w * new_h);
    for y in 0..new_h {
        let fy = ((y as f32 + 0.5) * sy - 0.5).max(0.0);
        let y0 = (fy.floor() as usize).min(src_h - 1);
        let y1 = (y0 + 1).min(src_h - 1);
        let wy = fy - y0 as f32;
        for x in 0..new_w {
            let fx = ((x as f32 + 0.5) * sx - 0.5).max(0.0);
            let x0 = (fx.floor() as usize).min(src_w - 1);
            let x1 = (x0 + 1).min(src_w - 1);
            let wx = fx - x0 as f32;
            let mut rgb = [0.0; 3];
            for (c, value) in rgb.iter_mut().enumerate() {
                let top = px(x0, y0, c) * (1.0 - wx) + px(x1, y0, c) * wx;
                let bottom = px(x0, y1, c) * (1.0 - wx) + px(x1, y1, c) * wx;
                *value = (top * (1.0 - wy) + bottom * wy).round();
            }
            out.push(rgb);
        }
    }
    out
}

/// The `[1, 3, 640, 640]` RGB input, normalized to `0..=1`.
fn input_blob(image: &RgbImage, lb: Letterbox) -> Vec<f32> {
    let plane = INPUT_SIZE * INPUT_SIZE;
    let mut blob = vec![PAD_VALUE; 3 * plane];
    let resized = resize_bilinear(image, lb.new_w, lb.new_h);
    for y in 0..lb.new_h {
        for x in 0..lb.new_w {
            let dst = (y + lb.pad_top) * INPUT_SIZE + x + lb.pad_left;
            let rgb = resized[y * lb.new_w + x];
            for (c, value) in rgb.iter().enumerate() {
                blob[c * plane + dst] = value / 255.0;
            }
        }
    }
    blob
}

/// The most confident `[x1, y1, x2, y2, conf, class]` row, mapped back to
/// image pixels and clamped the way YOLOs-CPP does.
fn best_detection(rows: &[f32], lb: Letterbox, src_w: usize, src_h: usize) -> Option<Detection> {
    // Descaling uses the unrounded padding.
    let pad_x = (INPUT_SIZE - lb.new_w) as f32 / 2.0;
    let pad_y = (INPUT_SIZE - lb.new_h) as f32 / 2.0;
    let inv = 1.0 / lb.scale;
    rows.as_chunks::<6>()
        .0
        .iter()
        .filter(|row| row[4] > CONFIDENCE_THRESHOLD)
        .map(|row| {
            let x1 = (row[0] - pad_x) * inv;
            let y1 = (row[1] - pad_y) * inv;
            let x2 = (row[2] - pad_x) * inv;
            let y2 = (row[3] - pad_y) * inv;
            let x = (x1 as i64).clamp(0, src_w as i64 - 1);
            let y = (y1 as i64).clamp(0, src_h as i64 - 1);
            let width = ((x2 - x1) as i64).clamp(1, src_w as i64 - x);
            let height = ((y2 - y1) as i64).clamp(1, src_h as i64 - y);
            Detection {
                x: x as f32,
                y: y as f32,
                width: width as f32,
                height: height as f32,
                confidence: row[4],
            }
        })
        .max_by(|a, b| a.confidence.total_cmp(&b.confidence))
}

impl CoverDetector for OnnxCoverDetector {
    fn detect(&self, image: &RgbImage) -> Result<Option<Detection>, String> {
        let (w, h) = (image.width() as usize, image.height() as usize);
        if w == 0 || h == 0 {
            return Ok(None);
        }
        let lb = letterbox(w, h);
        let input =
            Tensor::from_array(([1usize, 3, INPUT_SIZE, INPUT_SIZE], input_blob(image, lb)))
                .map_err(|e| e.to_string())?;
        let mut session = self.session.lock();
        let outputs = session
            .run(ort::inputs!["images" => input])
            .map_err(|e| e.to_string())?;
        let (shape, rows) = outputs["output0"]
            .try_extract_tensor::<f32>()
            .map_err(|e| e.to_string())?;
        if shape.len() != 3 || shape[2] != 6 {
            return Err(format!("unexpected output shape {shape:?}"));
        }
        Ok(best_detection(rows, lb, w, h))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn letterbox_matches_ultralytics_rounding() {
        let lb = letterbox(1920, 1080);
        assert_eq!((lb.new_w, lb.new_h), (640, 360));
        assert_eq!((lb.pad_left, lb.pad_top), (0, 140));
        let lb = letterbox(333, 500);
        assert_eq!((lb.new_w, lb.new_h), (426, 640));
        assert_eq!(lb.pad_left, 107); // round(107.0 - 0.1)
    }

    #[test]
    fn blob_is_padded_rgb_chw() {
        let image = RgbImage::from_pixel(64, 32, image::Rgb([255, 0, 51]));
        let lb = letterbox(64, 32);
        let blob = input_blob(&image, lb);
        let plane = INPUT_SIZE * INPUT_SIZE;
        assert_eq!(blob[0], PAD_VALUE, "top-left is padding");
        let center = 320 * INPUT_SIZE + 320;
        assert_eq!(blob[center], 1.0);
        assert_eq!(blob[plane + center], 0.0);
        assert!((blob[2 * plane + center] - 0.2).abs() < 1e-6);
    }

    #[test]
    fn picks_best_box_and_undoes_letterbox() {
        let lb = letterbox(1280, 720); // scale 0.5, pad_y 140
        let rows = [
            100.0, 200.0, 300.0, 400.0, 0.2, 0.0, // below threshold
            10.0, 150.0, 110.0, 290.0, 0.9, 0.0, // best
            0.0, 140.0, 640.0, 500.0, 0.5, 0.0,
        ];
        let d = best_detection(&rows, lb, 1280, 720).unwrap();
        assert_eq!((d.x, d.y, d.width, d.height), (20.0, 20.0, 200.0, 280.0));
        assert!(best_detection(&rows[..6], lb, 1280, 720).is_none());
    }
}
