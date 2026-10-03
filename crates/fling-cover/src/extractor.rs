//! Screenshot → cover pipeline.

use std::io::Cursor;
use std::path::PathBuf;
use std::sync::{Arc, OnceLock};

use fling_net::HttpClient;
use image::{ImageFormat, RgbImage};

use crate::CoverCache;

/// A detected cover box in original-image pixels.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Detection {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
    pub confidence: f32,
}

/// Finds the game cover in an RGB screenshot.
pub trait CoverDetector: Send + Sync {
    /// The most confident cover box, `Ok(None)` when there is none, `Err` when
    /// inference itself failed.
    fn detect(&self, image: &RgbImage) -> Result<Option<Detection>, String>;
}

/// Loads the detector on first use (model files may be missing).
pub type DetectorLoader = Arc<dyn Fn() -> Option<Arc<dyn CoverDetector>> + Send + Sync>;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CoverResult {
    Saved(PathBuf),
    /// The model found no cover in this screenshot; remembered in the cache.
    NoCover,
    /// Network, decode, model or disk failure; not remembered.
    Failed,
}

#[derive(Clone)]
pub struct CoverExtractor {
    http: Arc<dyn HttpClient>,
    cache: CoverCache,
    loader: DetectorLoader,
    detector: Arc<OnceLock<Option<Arc<dyn CoverDetector>>>>,
}

impl CoverExtractor {
    pub fn new(http: Arc<dyn HttpClient>, cache: CoverCache, loader: DetectorLoader) -> Self {
        Self {
            http,
            cache,
            loader,
            detector: Arc::new(OnceLock::new()),
        }
    }

    pub fn cache(&self) -> &CoverCache {
        &self.cache
    }

    fn detector(&self) -> Option<Arc<dyn CoverDetector>> {
        self.detector.get_or_init(|| (self.loader)()).clone()
    }

    /// Loads the model now (blocking), so the first cover does not pay for it.
    pub fn warm_up(&self) {
        let _ = self.detector();
    }

    /// Downloads `image_url`, crops the cover and caches it as `game_id`.
    /// Decoding and inference run on a blocking thread.
    pub async fn extract(&self, image_url: &str, game_id: &str) -> CoverResult {
        let Ok(bytes) = self.http.get(image_url).await else {
            return CoverResult::Failed;
        };
        let this = self.clone();
        let image_url = image_url.to_owned();
        let game_id = game_id.to_owned();
        tokio::task::spawn_blocking(move || this.extract_from_bytes(&bytes, &image_url, &game_id))
            .await
            .unwrap_or(CoverResult::Failed)
    }

    fn extract_from_bytes(&self, bytes: &[u8], image_url: &str, game_id: &str) -> CoverResult {
        let Ok(decoded) = image::load_from_memory(bytes) else {
            return CoverResult::Failed;
        };
        // A missing or broken model says nothing about this screenshot.
        let Some(detector) = self.detector() else {
            return CoverResult::Failed;
        };
        let rgb = decoded.to_rgb8();
        let detection = match detector.detect(&rgb) {
            Ok(detection) => detection,
            Err(err) => {
                tracing::warn!(%err, "cover inference failed");
                return CoverResult::Failed;
            }
        };
        let Some(cover) = detection.and_then(|d| crop(&rgb, d)) else {
            if let Err(err) = self.cache.record_no_cover(game_id, image_url) {
                tracing::warn!(%err, "failed to record missing cover");
            }
            return CoverResult::NoCover;
        };
        let mut png = Vec::new();
        if cover
            .write_to(&mut Cursor::new(&mut png), ImageFormat::Png)
            .is_err()
        {
            return CoverResult::Failed;
        }
        match self.cache.store(game_id, &png) {
            Ok(path) => CoverResult::Saved(path),
            Err(_) => CoverResult::Failed,
        }
    }
}

/// Crops `d` (clamped to the image) out of `image`; `None` for an empty box.
fn crop(image: &RgbImage, d: Detection) -> Option<RgbImage> {
    // The Qt build truncated the box to an integer cv::Rect.
    let x = (d.x as i64).max(0);
    let y = (d.y as i64).max(0);
    let width = (d.width as i64).min(image.width() as i64 - x);
    let height = (d.height as i64).min(image.height() as i64 - y);
    if width <= 0 || height <= 0 {
        return None;
    }
    Some(
        image::imageops::crop_imm(image, x as u32, y as u32, width as u32, height as u32)
            .to_image(),
    )
}

#[cfg(test)]
mod tests {
    use fling_net::fake::FakeHttpClient;

    use super::*;

    struct FixedDetector(Result<Option<Detection>, String>);

    impl CoverDetector for FixedDetector {
        fn detect(&self, _: &RgbImage) -> Result<Option<Detection>, String> {
            self.0.clone()
        }
    }

    fn png_bytes(width: u32, height: u32) -> Vec<u8> {
        let img = RgbImage::from_fn(width, height, |x, y| image::Rgb([x as u8, y as u8, 7]));
        let mut out = Vec::new();
        img.write_to(&mut Cursor::new(&mut out), ImageFormat::Png)
            .unwrap();
        out
    }

    fn extractor(
        fake: &FakeHttpClient,
        dir: &std::path::Path,
        result: Option<Result<Option<Detection>, String>>,
    ) -> CoverExtractor {
        let loader: DetectorLoader = Arc::new(move || {
            result
                .clone()
                .map(|r| Arc::new(FixedDetector(r)) as Arc<dyn CoverDetector>)
        });
        CoverExtractor::new(
            Arc::new(fake.clone()),
            CoverCache::new(dir.join("covers")),
            loader,
        )
    }

    const BOX: Detection = Detection {
        x: -5.0,
        y: 10.7,
        width: 30.0,
        height: 500.0,
        confidence: 0.9,
    };

    #[tokio::test]
    async fn saves_clamped_crop() {
        let dir = tempfile::tempdir().unwrap();
        let fake = FakeHttpClient::new();
        fake.page("https://x/shot.jpg", png_bytes(100, 60));
        let result = extractor(&fake, dir.path(), Some(Ok(Some(BOX))))
            .extract("https://x/shot.jpg", "G")
            .await;
        let CoverResult::Saved(path) = result else {
            panic!("{result:?}")
        };
        let cover = image::open(path).unwrap().to_rgb8();
        assert_eq!(cover.dimensions(), (30, 50));
        assert_eq!(cover.get_pixel(0, 0), &image::Rgb([0, 10, 7]));
    }

    #[tokio::test]
    async fn no_detection_is_remembered_but_failures_are_not() {
        let dir = tempfile::tempdir().unwrap();
        let fake = FakeHttpClient::new();
        fake.page("https://x/a.png", png_bytes(20, 20));
        fake.page("https://x/bad.png", b"not an image".to_vec());

        let none = extractor(&fake, dir.path(), Some(Ok(None)));
        assert_eq!(
            none.extract("https://x/a.png", "G").await,
            CoverResult::NoCover
        );
        assert!(none.cache().is_known_without_cover("G", "https://x/a.png"));

        assert_eq!(
            none.extract("https://x/bad.png", "H").await,
            CoverResult::Failed
        );
        assert_eq!(
            none.extract("https://x/missing.png", "H").await,
            CoverResult::Failed
        );

        let no_model = extractor(&fake, dir.path(), None);
        assert_eq!(
            no_model.extract("https://x/a.png", "M").await,
            CoverResult::Failed
        );
        let broken = extractor(&fake, dir.path(), Some(Err("ort".into())));
        assert_eq!(
            broken.extract("https://x/a.png", "B").await,
            CoverResult::Failed
        );
        assert!(
            !broken
                .cache()
                .is_known_without_cover("B", "https://x/a.png")
        );
    }
}
