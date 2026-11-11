// Control: the commands. Run and Stop for the contactors, the calibrations, the balance charge.
// Every command asks for confirmation and then the PIN, so the buttons can be big.
#include "ui_draw.h"

namespace hmiui {

void draw_control(Canvas &c, const Theme &t, const Model &m) {
  top_line(c, t, m, "Control");
  const int gap = 12, lw = 470, rx = X0 + lw + gap, rw = X1 - rx, top_h = 300;
  const bool linked = m.link == Link::OK;

  // ---- contactors ----
  card(c, t, X0, Y0, lw, top_h);
  c.text(FONT_R14, X0 + 16, Y0 + 12, "CONTACTORS", t.caption);
  {
    const char *state = !linked ? "--" : (m.contactors_closed ? "Closed" : "Open");
    int x = X0 + 16;
    x += c.text(FONT_M32, x, Y0 + 34, state, m.contactors_closed ? t.good : t.text) + 14;
    if (linked) c.text(FONT_R18, x, Y0 + 48, fmt("BMS %s", system_state_name(m.system_state)),
                       m.system_state == SystemState::FAULT ? t.fatal : t.caption);
  }
  const int bw = (lw - 32 - gap) / 2;
  button(c, t, X0 + 16, Y0 + 92, bw, 150, "Run", "close the contactors", 0x1B5E3F, 0xFFFFFF);
  button(c, t, X0 + 16 + bw + gap, Y0 + 92, bw, 150, "Stop", "open the contactors", 0x7A2222, 0xFFFFFF);
  c.text(FONT_R16, X0 + 16, Y0 + 260, "Each command asks you to confirm, then for the PIN.", t.caption);

  // ---- calibration ----
  card(c, t, rx, Y0, rw, top_h);
  c.text(FONT_R14, rx + 16, Y0 + 12, "CALIBRATION", t.caption);
  const int cw = (rw - 32 - gap) / 2, ch = 96;
  button(c, t, rx + 16, Y0 + 42, cw, ch, "Current sensor", "offline, long");
  button(c, t, rx + 16 + cw + gap, Y0 + 42, cw, ch, "Current sensor", "offline, short");
  button(c, t, rx + 16, Y0 + 42 + ch + gap, cw, ch, "Current sensor", "online");
  button(c, t, rx + 16 + cw + gap, Y0 + 42 + ch + gap, cw, ch, "Isolation", "zero");

  // ---- balance charge ----
  const int by = Y0 + top_h + gap, bh = Y1 - by;
  card(c, t, X0, by, X1 - X0, bh);
  c.text(FONT_R14, X0 + 16, by + 12, "BALANCE CHARGE", t.caption);
  c.text(FONT_M24, X0 + 16, by + 42, m.balance_charge_status[0] ? m.balance_charge_status : "--", t.text);
  text_wrap(c, FONT_R16, X0 + 16, by + 84, 440,
            "Raises the charge voltage for a while so every cell reaches the top and the balancer can even them out. "
            "The schedule is under Setup.",
            t.caption, 4);
  button(c, t, X1 - 16 - 2 * 180 - gap, by + 60, 180, 96, "Start now");
  button(c, t, X1 - 16 - 180, by + 60, 180, 96, "Stop");

  page_end(c, t, m, 5);
}

}  // namespace hmiui
