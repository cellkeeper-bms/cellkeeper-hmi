// The colours of the LCD UI, in one place.
//
// Rule: green means healthy or charging, blue means discharging, and yellow, orange and red are
// only ever alarm levels, so a warm colour on the screen always means "look at this".
#pragma once

#include "ui_canvas.h"

namespace hmiui {

struct Theme {
  Rgb background = 0x08090A;
  Rgb card = 0x15181A;
  Rgb card_border = 0x252B2E;
  Rgb track = 0x2B3236;       // empty part of a gauge or bar
  Rgb text = 0xF4F6F5;
  Rgb text_dim = 0xB9C2BE;    // secondary values
  Rgb caption = 0x869590;     // card titles and labels
  Rgb hint = 0x66716D;
  Rgb good = 0x52DC96;        // healthy, charging
  Rgb out = 0x4DB8FF;         // discharging
  Rgb cell = 0x3C8FD8;        // a cell's bar
  Rgb cell_low = 0x35C9B5;    // the lowest cell
  Rgb cell_high = 0xF0609A;   // the highest cell
  Rgb info = 0x6E8496;
  Rgb warning = 0xFFC107;
  Rgb critical = 0xFF6D3A;
  Rgb fatal = 0xE5332F;
  Rgb nav = 0x0D0F10;
  Rgb nav_active = 0x1A2B23;
};

}  // namespace hmiui
