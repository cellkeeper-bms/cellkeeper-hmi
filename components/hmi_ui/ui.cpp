#include "ui.h"

namespace hmiui {

void Ui::render(Canvas &c, const Model &m, uint32_t now_ms) {
  (void) now_ms;
  c.reset_clip();
  c.clear(theme.background);
  switch (page) {
    case Page::HOME:
      if (design == 'b') {
        draw_home_b(c, theme, m);
      } else if (design == 'c') {
        draw_home_c(c, theme, m);
      } else {
        draw_home_a(c, theme, m);
      }
      break;
    default:
      c.text(FONT_R24, SCREEN_W / 2, 260, "This page is not drawn yet", theme.caption, CENTER);
      nav_bar(c, theme, m, (int) page);
      break;
  }
}

}  // namespace hmiui
