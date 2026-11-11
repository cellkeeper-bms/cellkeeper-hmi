// The LCD UI: draws a page from a Model onto a Canvas and reacts to touch. Plain C++ (no
// ESPHome, no LVGL), so the device, the PC simulator and the host tests run the same code.
// Each page is one source file (page_*.cpp); README.md in this folder says how they are written.
#pragma once

#include "ui_canvas.h"
#include "ui_draw.h"
#include "ui_frame.h"
#include "ui_model.h"
#include "ui_theme.h"

namespace hmiui {

class Ui {
 public:
  Theme theme;
  UiState state;

  // One frame. Give it the touch panel as it is now; it lets the page and its widgets react,
  // sends what they ask for to `actions`, and draws the whole frame. Call it whenever the model
  // or the touch changed (a few times a second is enough when nothing happens).
  void frame(Canvas &c, const Model &m, const Input &input, Actions &actions, uint32_t now_ms, HitTrace *trace = nullptr);

  // True when nobody has touched the display for a while: the device may dim it. The UI has
  // then gone back to Home.
  bool idle(uint32_t now_ms) const { return now_ms - last_touch_ms_ >= IDLE_MS; }

  static const uint32_t IDLE_MS = 5 * 60 * 1000;

 protected:
  void draw_(Frame &f);
  bool was_down_ = false, dragging_ = false;
  int last_x_ = 0, last_y_ = 0, press_x_ = 0, press_y_ = 0;
  uint32_t last_touch_ms_ = 0;
};

}  // namespace hmiui
