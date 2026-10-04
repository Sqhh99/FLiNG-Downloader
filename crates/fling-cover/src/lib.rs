//! Cropping the game cover out of a trainer screenshot. Port of `CoverExtractor`.
//!
//! The pipeline (download → decode → detect → crop → cache) lives here; the
//! detector itself is a [`CoverDetector`] so the model runtime stays
//! swappable and tests need no model.

mod cache;
mod extractor;
#[cfg(feature = "onnx")]
mod onnx;

pub use cache::{CoverCache, MODEL_NAME};
pub use extractor::{CoverDetector, CoverExtractor, CoverResult, Detection, DetectorLoader};
#[cfg(feature = "onnx")]
pub use onnx::OnnxCoverDetector;
