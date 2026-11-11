// The LCD UI: draws a page from a Model onto a Canvas. Plain C++ (no ESPHome, no LVGL), so
// the device and the PC simulator run the same code. Each page is one source file (page_*.cpp).
#pragma once

#include "ui_canvas.h"
#include "ui_draw.h"
#include "ui_model.h"
#include "ui_theme.h"

namespace hmiui {

class Ui {
 public:
  Theme theme;
  Page page = Page::HOME;
  int setup_section = 0;  // which part of the Setup page is open

  // Draws the whole frame for the current page
  void render(Canvas &c, const Model &m, uint32_t now_ms);
};

}  // namespace hmiui
