#include "ui.h"

namespace hmiui {

void Ui::render(Canvas &c, const Model &m, uint32_t now_ms) {
  (void) now_ms;
  c.reset_clip();
  c.clear(theme.background);
  switch (page) {
    case Page::HOME: draw_home(c, theme, m); break;
    case Page::CELLS: draw_cells(c, theme, m); break;
    case Page::POWER: draw_power(c, theme, m); break;
    case Page::HEALTH: draw_health(c, theme, m); break;
    case Page::EVENTS: draw_events(c, theme, m, events_view); break;
    case Page::CONTROL: draw_control(c, theme, m); break;
    default: draw_setup(c, theme, m, setup_section, raw_registers); break;
  }
  if (popup.kind != Overlay::NONE) draw_popup(c, theme, m, popup);
}

}  // namespace hmiui
