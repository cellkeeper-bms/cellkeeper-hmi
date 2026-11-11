#include "ui.h"

#include <algorithm>
#include <cstdlib>

namespace hmiui {

// A finger that moves further than this from where it came down is dragging, not tapping
static const int DRAG_START = 12;

void Ui::draw_(Frame &f) {
  f.c.reset_clip();
  f.c.clear(theme.background);
  // Under a pop-up the page is drawn but does not react
  f.active = state.popup.kind == Overlay::NONE;
  switch (state.page) {
    case Page::HOME: draw_home(f); break;
    case Page::CONTROL: draw_control(f); break;
    case Page::POWER: draw_power(f); break;
    case Page::CELLS: draw_cells(f); break;
    case Page::HEALTH: draw_health(f); break;
    case Page::EVENTS: draw_events(f); break;
    default: draw_setup(f); break;
  }
  if (state.popup.kind != Overlay::NONE) {
    f.active = true;
    draw_popup(f);
  }
}

bool Ui::frame(Canvas &c, const Model &m, const Input &input, Actions &actions, uint32_t now_ms, HitTrace *trace) {
  const bool pressed = input.down && !was_down_, released = !input.down && was_down_;
  int dx = 0, dy = 0;
  if (pressed) {
    press_x_ = input.x;
    press_y_ = input.y;
    dragging_ = false;
  } else if (input.down) {
    dx = input.x - last_x_;
    dy = input.y - last_y_;
    if (std::abs(input.x - press_x_) > DRAG_START || std::abs(input.y - press_y_) > DRAG_START) dragging_ = true;
  }
  if (input.down) {
    last_x_ = input.x;
    last_y_ = input.y;
  }
  if (input.down || released) {
    last_touch_ms_ = now_ms;
  } else if (this->idle(now_ms) && (state.page != Page::HOME || state.popup.kind != Overlay::NONE)) {
    // Left alone: back to Home, with nothing half-entered left open. Wrong PINs still count.
    const UiState before = state;
    state = UiState();
    state.pin_wrong = before.pin_wrong;
    state.pin_locked = before.pin_locked;
    state.pin_locked_at_ms = before.pin_locked_at_ms;
  }

  // First a dry run: the widgets see the touch and react, and what the frame would look like
  // is summed up without drawing it.
  Frame f{c, theme, m, state, actions, now_ms};
  f.down = input.down;
  f.released = released;
  f.dragging = dragging_;
  f.tx = last_x_;
  f.ty = last_y_;
  f.press_x = press_x_;
  f.press_y = press_y_;
  f.dx = dx;
  f.dy = dy;
  f.trace = trace;
  if (trace != nullptr) trace->count = 0;
  c.begin_dry();
  this->draw_(f);
  const uint32_t *sums = c.end_dry();
  // The frame as it is after the touch was dealt with: the touch itself is no longer in it
  auto settled = [&](HitTrace *record) {
    Frame after{c, theme, m, state, actions, now_ms};
    after.down = input.down;
    after.dragging = dragging_;
    after.tx = last_x_;
    after.ty = last_y_;
    after.press_x = press_x_;
    after.press_y = press_y_;
    after.trace = record;
    if (record != nullptr) record->count = 0;
    this->draw_(after);
  };
  if (f.changed) {
    // Something was tapped or moved: the frame to show is the one after it
    c.begin_dry();
    settled(trace);
    sums = c.end_dry();
  }
  was_down_ = input.down;

  // Which bands would look different: runs of them, a run taking in a gap of one band
  const int bands = c.bands();
  redrawn_count = 0;
  for (int band = 0; band < bands; band++) {
    if (drawn_ && sums[band] == drawn_sums_[band]) continue;
    const int y0 = band * Canvas::BAND_ROWS, y1 = std::min(c.height(), y0 + Canvas::BAND_ROWS);
    if (redrawn_count > 0 && (y0 - redrawn[redrawn_count - 1].y1 <= Canvas::BAND_ROWS || redrawn_count == MAX_RUNS)) {
      redrawn[redrawn_count - 1].y1 = y1;
    } else {
      redrawn[redrawn_count++] = {y0, y1};
    }
    drawn_sums_[band] = sums[band];
  }
  drawn_ = true;
  for (int i = 0; i < redrawn_count; i++) {
    c.set_window(redrawn[i].y0, redrawn[i].y1);
    settled(nullptr);
  }
  c.clear_window();
  return redrawn_count > 0;
}

}  // namespace hmiui
