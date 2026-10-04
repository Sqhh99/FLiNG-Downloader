//! Small animation helpers shared by the views. Every helper renders the
//! final state immediately when the system asks for reduced motion (Windows
//! "Animation effects" off), which GPUI Kit reads into `cx.reduce_motion()`.

use std::time::Duration;

use gpui_kit::base::animation::ease_out_cubic;
use gpui_kit::base::motion::{Presence, Transition};
use gpui_kit::*;

/// Durations, in milliseconds.
pub const OVERLAY_MS: u64 = 180;
pub const DRAWER_MS: u64 = 220;
pub const POPUP_MS: u64 = 130;
pub const FADE_MS: u64 = 160;
const ROW_MS: u64 = 220;
const ROW_STAGGER_MS: u64 = 18;
/// Rows past this index appear without delay (they start off-screen anyway).
const ROW_STAGGER_LIMIT: usize = 16;

/// How visible an overlay is: `Some(0.0..=1.0)` while it is shown or
/// animating out, `None` once fully hidden. Keyed by `id`, so the same id
/// must be used every frame.
pub fn presence(
    id: &'static str,
    present: bool,
    ms: u64,
    window: &mut Window,
    cx: &mut App,
) -> Option<f32> {
    let sample = Presence::new(id, present)
        .transition(Transition::new(Duration::from_millis(ms)))
        .sample(window, cx);
    sample.should_render().then_some(sample.progress)
}

/// Fades `element` in when it first appears under `id`; a new `id` replays it.
pub fn fade_in<E>(element: E, id: impl Into<ElementId>, ms: u64, cx: &App) -> AnyElement
where
    E: IntoElement + Styled + 'static,
{
    if cx.reduce_motion() {
        return element.into_any_element();
    }
    element
        .with_animation(
            id,
            Animation::new(Duration::from_millis(ms)).with_easing(ease_out_cubic),
            |element, delta| element.opacity(delta),
        )
        .into_any_element()
}

/// A list row rising into place: fades in while sliding up a few pixels,
/// staggered by `index`. A new `id` (e.g. a new result set) replays it.
pub fn row_in<E>(element: E, id: impl Into<ElementId>, index: usize, cx: &App) -> AnyElement
where
    E: IntoElement + Styled + 'static,
{
    if cx.reduce_motion() {
        return element.into_any_element();
    }
    let delay = (index.min(ROW_STAGGER_LIMIT) as u64 * ROW_STAGGER_MS) as f32;
    let total = delay + ROW_MS as f32;
    let animation =
        Animation::new(Duration::from_millis(total as u64)).with_easing(move |t: f32| {
            // Hold at 0 during this row's delay, then ease out over ROW_MS.
            ease_out_cubic(((t * total - delay) / ROW_MS as f32).clamp(0.0, 1.0))
        });
    element
        .with_animation(id, animation, |element, delta| {
            element
                .relative()
                .top(px((1.0 - delta) * 8.0))
                .opacity(delta)
        })
        .into_any_element()
}
