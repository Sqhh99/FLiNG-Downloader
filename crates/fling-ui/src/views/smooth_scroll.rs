//! Smooth mouse-wheel scrolling for a scroll container.
//!
//! GPUI applies wheel notches as instant jumps of several lines. A
//! [`SmoothScroll`] takes those events over in the capture phase, before the
//! container's own handler, and eases the container's [`ScrollHandle`] toward
//! the accumulated target over a few frames. Precise (touchpad) scrolling is
//! already smooth and is left to GPUI, as is everything when the system asks
//! for reduced motion.
//!
//! Usage: give the container `.track_scroll(smooth.handle())`, wrap it in a
//! `relative()` parent, and add `smooth.driver()` as the container's next
//! sibling.

use std::cell::Cell;
use std::rc::Rc;

use gpui_kit::*;

/// Fraction of the remaining distance covered per frame (ease-out).
const STEP: f32 = 0.28;
/// Distance at which the animation snaps to its target.
const SNAP: f32 = 0.5;

#[derive(Clone, Default)]
pub struct SmoothScroll {
    handle: ScrollHandle,
    /// Where the animation is heading; `None` when idle.
    target: Rc<Cell<Option<Point<Pixels>>>>,
}

impl SmoothScroll {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn handle(&self) -> &ScrollHandle {
        &self.handle
    }

    /// Jumps to the top without animation (e.g. when the list is replaced).
    pub fn reset(&self) {
        self.target.set(None);
        self.handle.set_offset(Point::default());
    }

    /// An invisible overlay that drives the animation. Place it right after
    /// the scroll container, inside a `relative()` parent.
    pub fn driver(&self) -> impl IntoElement {
        let this = self.clone();
        canvas(
            move |bounds, window, _| window.insert_hitbox(bounds, HitboxBehavior::Normal),
            move |_, hitbox, window, cx| {
                this.step(window);
                if cx.reduce_motion() {
                    return;
                }
                let SmoothScroll { handle, target } = this;
                window.on_mouse_event(move |event: &ScrollWheelEvent, phase, window, cx| {
                    if phase != DispatchPhase::Capture
                        || event.delta.precise()
                        || !hitbox.should_handle_scroll(window)
                    {
                        return;
                    }
                    let delta = event.delta.pixel_delta(window.line_height());
                    let max = handle.max_offset();
                    let from = target.get().unwrap_or_else(|| handle.offset());
                    let to = point(from.x, (from.y + delta.y).clamp(-max.y, px(0.)));
                    target.set(Some(to));
                    cx.stop_propagation();
                    window.refresh();
                });
            },
        )
        .absolute()
        .inset_0()
    }

    /// Moves one frame toward the target and asks for another frame until
    /// it arrives.
    fn step(&self, window: &mut Window) {
        let Some(target) = self.target.get() else {
            return;
        };
        let current = self.handle.offset();
        let remaining = target.y - current.y;
        if remaining.abs() <= px(SNAP) {
            self.handle.set_offset(target);
            self.target.set(None);
        } else {
            self.handle
                .set_offset(point(target.x, current.y + remaining * STEP));
            window.request_animation_frame();
        }
    }
}
