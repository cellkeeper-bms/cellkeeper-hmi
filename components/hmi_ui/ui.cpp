#include "ui.h"

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
    // Left alone: back to Home, with nothing half-entered left open
    state = UiState();
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
  uint32_t sum = c.end_dry();
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
    sum = c.end_dry();
  }
  was_down_ = input.down;
  if (drawn_ && sum == drawn_sum_) return false;
  settled(nullptr);
  drawn_ = true;
  drawn_sum_ = sum;
  return true;
}

}  // namespace hmiui
