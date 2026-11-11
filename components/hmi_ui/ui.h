// The LCD UI: draws a page from a Model onto a Canvas. Plain C++ (no ESPHome, no LVGL), so
// the device and the PC simulator run the same code.
//
// State of this file: the engine and a first page, drawn the way the LVGL "Main v2" page looks
// today, to prove the drawing core against a screenshot of the display. The design of the pages
// is the next step and will change what is in here.
#pragma once

#include "ui_canvas.h"
#include "ui_model.h"

namespace hmiui {

static const int SCREEN_W = 1024;
static const int SCREEN_H = 600;

enum class Page : uint8_t { MAIN, CONTROL, EVENTS, CELLS, POWER, HEALTH, CONFIG, LOG, SYSTEM, COUNT };

// The colours of the UI, in one place
struct Theme {
  Rgb background = 0x000000;
  Rgb card = 0x131715;
  Rgb card_border = 0x26302B;
  Rgb track = 0x26302B;       // empty part of a gauge or bar
  Rgb text = 0xFFFFFF;
  Rgb text_dim = 0xC8D2CD;    // secondary values
  Rgb caption = 0x8FA39A;     // card titles and labels
  Rgb hint = 0x6F7A75;
  Rgb accent = 0x5EE3A8;      // "all is well" green
  Rgb voltage = 0x00BFFF;
  Rgb current = 0xFFD54A;
  Rgb info = 0x2196F3;
  Rgb warning = 0xFFC107;
  Rgb critical = 0xFF5722;
  Rgb fatal = 0xD50000;
  Rgb tab_bar = 0x0B0D0C;
  Rgb tab_active = 0x1F3A2D;
};

class Ui {
 public:
  Theme theme;
  Page page = Page::MAIN;

  // Draws the whole frame for the current page
  void render(Canvas &c, const Model &m, uint32_t now_ms);

 protected:
  void draw_main_(Canvas &c, const Model &m);
  void draw_tab_bar_(Canvas &c, const Model &m);
  void draw_placeholder_(Canvas &c, const char *title);
  void card_(Canvas &c, int x, int y, int w, int h);
  Rgb level_color_(Level level) const;
};

// The firmware's name for an event type, as words ("Cell Voltages Stale"); "" if unknown
const char *event_title(uint8_t type);

}  // namespace hmiui
