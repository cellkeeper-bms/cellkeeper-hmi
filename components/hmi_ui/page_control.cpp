// Control: the commands. Run and Stop for the contactors, the calibrations, the balance charge.
//
// This is the reference page: the others follow its pattern.
//   1. top_line() first, page_end() last.
//   2. Read everything from f.m. Never keep a copy: the page is drawn again from the model.
//   3. A button both draws itself and returns true when tapped. A command is never sent from
//      the tap itself: ask_command() opens "are you sure", then the PIN, and sends it.
//   4. A button that would do nothing is drawn disabled, and the page says why.
//   5. Layout is plain numbers from X0, X1, Y0, Y1 (the area right of the rail, under the top
//      line). Every text has room for its longest value: check the renders of every scenario.
#include "ui_draw.h"

namespace hmiui {

// What the BMS does with a command depends on its state: it takes Run and the calibrations
// only while Inactive, and Stop only while Operating. Anything else it would leave waiting.
static const char *why_not(const Model &m) {
  switch (m.system_state) {
    case SystemState::FAULT: return "In FAULT the BMS takes no commands. Fix the cause, then restart the BMS.";
    case SystemState::UNINITIALIZED:
    case SystemState::INITIALIZING: return "The BMS is starting. Commands work once it is Inactive.";
    case SystemState::CALIBRATING: return "Calibrating. Commands work again when it is done.";
    case SystemState::OPERATING: return "Each command asks you to confirm, then for the PIN. Stop first to calibrate.";
    default: return "Each command asks you to confirm, then for the PIN.";
  }
}

void draw_control(Frame &f) {
  Canvas &c = f.c;
  const Theme &t = f.t;
  const Model &m = f.m;
  top_line(f, "Control");
  const int gap = 12, lw = 470, rx = X0 + lw + gap, rw = X1 - rx, top_h = 300;
  const bool linked = m.link == Link::OK;
  const bool inactive = linked && m.system_state == SystemState::INACTIVE;
  const bool operating = linked && m.system_state == SystemState::OPERATING;

  // ---- contactors ----
  card(c, t, X0, Y0, lw, top_h);
  c.text(FONT_R14, X0 + 16, Y0 + 12, "CONTACTORS", t.caption);
  {
    const char *state = !linked ? "--" : (m.contactors_closed ? "Closed" : (m.precharging ? "Precharging" : "Open"));
    int x = X0 + 16;
    x += c.text(FONT_M32, x, Y0 + 34, state, m.contactors_closed ? t.good : t.text) + 14;
    if (linked) {
      c.text(FONT_R18, x, Y0 + 48, fmt("BMS %s", system_state_name(m.system_state)),
             m.system_state == SystemState::FAULT ? t.fatal : t.caption);
    }
  }
  const int bw = (lw - 32 - gap) / 2;
  if (button(f, X0 + 16, Y0 + 92, bw, 150, "Run", "close the contactors", 0x1B5E3F, 0xFFFFFF, inactive)) {
    ask_command(f.s, Command::RUN, "Close the contactors?",
                "The BMS tests the contactors, precharges the output and connects the pack to the inverter.",
                "Close the contactors", "close the contactors", false);
  }
  if (button(f, X0 + 16 + bw + gap, Y0 + 92, bw, 150, "Stop", "open the contactors", 0x7A2222, 0xFFFFFF, operating)) {
    ask_command(f.s, Command::STOP, "Open the contactors?",
                "The BMS disconnects the pack from the inverter. The inverter loses its battery until you press Run.",
                "Open the contactors", "open the contactors", true);
  }
  text_wrap(c, FONT_R16, X0 + 16, Y0 + 254, lw - 32, linked ? why_not(m) : "", t.caption, 2);

  // ---- calibration ----
  card(c, t, rx, Y0, rw, top_h);
  c.text(FONT_R14, rx + 16, Y0 + 12, "CALIBRATION", t.caption);
  const int cw = (rw - 32 - gap) / 2, ch = 96;
  if (button(f, rx + 16, Y0 + 42, cw, ch, "Current sensor", "offline, long", 0, 0, inactive)) {
    ask_command(f.s, Command::CALIBRATE_OFFLINE_LONG, "Calibrate the current sensor?",
                "The long offline calibration of the current sensor. The contactors stay open while it runs.",
                "Calibrate", "calibrate the current sensor", false);
  }
  if (button(f, rx + 16 + cw + gap, Y0 + 42, cw, ch, "Current sensor", "offline, short", 0, 0, inactive)) {
    ask_command(f.s, Command::CALIBRATE_OFFLINE_SHORT, "Calibrate the current sensor?",
                "The short offline calibration of the current sensor. The contactors stay open while it runs.",
                "Calibrate", "calibrate the current sensor", false);
  }
  if (button(f, rx + 16, Y0 + 42 + ch + gap, cw, ch, "Current sensor", "online", 0, 0, inactive)) {
    ask_command(f.s, Command::CALIBRATE_ONLINE, "Calibrate the current sensor?", "The online calibration of the current sensor.",
                "Calibrate", "calibrate the current sensor", false);
  }
  if (button(f, rx + 16 + cw + gap, Y0 + 42 + ch + gap, cw, ch, "Isolation", "zero", 0, 0, inactive)) {
    ask_command(f.s, Command::ISOLATION_ZERO, "Zero the isolation measurement?",
                "Takes the present reading as zero. Do this only with nothing connected that could leak to the chassis.",
                "Zero it", "zero the isolation measurement", false);
  }
  c.text(FONT_R16, rx + 16, Y0 + 260, !linked || inactive ? "" : "Calibration works while the BMS is Inactive.", t.caption);

  // ---- balance charge ----
  const int by = Y0 + top_h + gap, bh = Y1 - by;
  card(c, t, X0, by, X1 - X0, bh);
  c.text(FONT_R14, X0 + 16, by + 12, "BALANCE CHARGE", t.caption);
  c.text(FONT_M24, X0 + 16, by + 42, m.balance_charge_status[0] ? m.balance_charge_status : "--", t.text);
  text_wrap(c, FONT_R16, X0 + 16, by + 84, 440,
            "Raises the charge voltage for a while so every cell reaches the top and the balancer can even them out. "
            "The schedule is under Setup.",
            t.caption, 4);
  if (button(f, X1 - 16 - 2 * 180 - gap, by + 60, 180, 96, "Start now", "", 0, 0, linked)) {
    ask_command(f.s, Command::BALANCE_CHARGE_START, "Start a balance charge now?",
                "The charge voltage sent to the inverter is raised until the balance charge ends or you stop it.",
                "Start the balance charge", "start a balance charge", false);
  }
  if (button(f, X1 - 16 - 180, by + 60, 180, 96, "Stop", "", 0, 0, linked)) {
    ask_command(f.s, Command::BALANCE_CHARGE_STOP, "Stop the balance charge?",
                "The charge voltage sent to the inverter goes back to normal.", "Stop the balance charge",
                "stop the balance charge", false);
  }

  page_end(f, Page::CONTROL);
}

}  // namespace hmiui
