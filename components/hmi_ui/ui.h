// The LCD UI: draws a page from a Model onto a Canvas. Plain C++ (no ESPHome, no LVGL), so
// the device and the PC simulator run the same code.
//
// State of this file: the design phase. Each page is one source file (page_*.cpp); the home
// page exists in three candidate designs, chosen with `design`, until one is picked.
#pragma once

#include "ui_canvas.h"
#include "ui_draw.h"
#include "ui_model.h"
#include "ui_theme.h"

namespace hmiui {

enum class Page : uint8_t { HOME, CELLS, POWER, HEALTH, EVENTS, CONTROL, SETUP, COUNT };

class Ui {
 public:
  Theme theme;
  Page page = Page::HOME;
  char design = 'a';  // which candidate design to draw: 'a', 'b' or 'c'

  // Draws the whole frame for the current page
  void render(Canvas &c, const Model &m, uint32_t now_ms);
};

}  // namespace hmiui
