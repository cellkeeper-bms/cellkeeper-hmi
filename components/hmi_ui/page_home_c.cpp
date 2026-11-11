// Home page, design C, "glance": two numbers that read from across the room (state of charge
// and power), a coloured band that says how the pack is, and one line of detail underneath.
// In FAULT the power half becomes the reason there is no power.
#include "ui_draw.h"

#include <algorithm>

namespace hmiui {

static const int BAND_H = 52, FOOT_Y = 448, CONTENT_H = SCREEN_H - NAV_BAR_H;

static void stat(Canvas &c, const Theme &t, int x, const char *caption, const char *number, const char *unit,
                 const char *note, Rgb note_color) {
  c.text(FONT_R14, x, FOOT_Y + 14, caption, t.caption);
  const int w = num_unit(c, x, FOOT_Y + 64, FONT_M24, number, FONT_R18, unit, t.text, t.caption);
  if (note[0]) c.text(FONT_R16, x + w + 12, FOOT_Y + 45, note, note_color);
}

void draw_home_c(Canvas &c, const Theme &t, const Model &m) {
  const Flow flow = flow_of(m);
  const bool linked = m.link == Link::OK;
  const bool fault = linked && m.system_state == SystemState::FAULT;
  const Event *list[MAX_EVENTS];
  const int alarms = top_events(m, list, MAX_EVENTS, Level::WARNING);

  // ---- the band ----
  const Level worst = alarms > 0 ? list[0]->level : Level::INACTIVE;
  const bool ok = linked && alarms == 0 && m.system_state == SystemState::OPERATING;
  const Rgb band = alarms > 0 ? level_color(t, worst) : (ok ? mix(t.background, t.good, 0.22f) : t.track);
  const Rgb on = alarms > 0 ? level_text_color(worst) : (ok ? t.good : t.text_dim);
  c.fill(0, 0, SCREEN_W, BAND_H, band);
  int x = 24;
  x += c.text(FONT_M24, x, 12, m.pack_name[0] ? m.pack_name : "CellKeeper", alarms > 0 ? on : t.text) + 20;
  if (!linked) {
    c.text(FONT_R20, x, 15, "No BMS link", on);
  } else if (alarms > 0) {
    x += c.text(FONT_B20, x, 15, level_name(worst), on) + 12;
    x += c.text(FONT_R20, x, 15, event_title(list[0]->type), on) + 16;
    if (alarms > 1) c.text(FONT_R20, x, 15, fmt("+%d more", alarms - 1), on);
  } else {
    c.text(FONT_R20, x, 15, fmt("%s  ·  Contactors %s", system_state_name(m.system_state),
                                m.contactors_closed ? "closed" : "open"), on);
  }
  if (alarms > 0) {
    c.text(FONT_R20, SCREEN_W - 24, 15, "Events  ›", on, RIGHT);
  } else if (m.clock[0]) {
    c.text(FONT_R20, SCREEN_W - 24, 15, m.clock, t.text_dim, RIGHT);
  }

  // ---- left: state of charge ----
  const int lx = 40, half = 432;
  c.text(FONT_R18, lx, 76, "STATE OF CHARGE", t.caption);
  {
    const Rgb color = std::isnan(m.soc) ? t.hint : t.text;
    num_unit(c, lx - 4, 246, FONT_M128, value(m.soc, "%.0f"), FONT_M48, "%", color, t.text_dim, LEFT, 10);
  }
  c.round_rect(lx, 280, half, 34, 10, t.track);
  if (!std::isnan(m.soc)) {
    const int fw = std::max(20, (int) (half * clampf(m.soc, 0, 100) / 100.0f));
    c.round_rect(lx, 280, fw, 34, 10, soc_color(t, m.soc));
  }
  for (int i = 1; i < 4; i++) c.fill(lx + i * half / 4, 280, 2, 34, t.background);
  if (!std::isnan(m.soc) && !std::isnan(m.capacity_ah) && !std::isnan(m.pack_voltage)) {
    c.text(FONT_R24, lx, 334, fmt("≈ %.1f kWh  ·  %.0f Ah", (double) kwh_left(m), (double) ah_left(m)), t.text_dim);
    const char *eta = time_to_text(m);
    if (eta[0]) {
      c.text(FONT_R24, lx, 372, eta, t.text);
    } else if (!std::isnan(m.soc_change_1h)) {
      c.text(FONT_R24, lx, 372, fmt("%+.1f %% in the last hour", (double) m.soc_change_1h), t.caption);
    }
  }
  c.fill(SCREEN_W / 2 - 1, BAND_H + 24, 1, FOOT_Y - BAND_H - 48, t.card_border);

  // ---- right: power, or in FAULT the reason there is none ----
  const int rx = SCREEN_W / 2 + 40;
  if (fault) {
    warning_sign(c, rx + 30.0f, 100.0f, 54.0f, t.fatal, 0xFFFFFF);
    c.text(FONT_M32, rx + 76, 70, "Fault", t.fatal);
    c.text(FONT_R20, rx + 76, 108, m.contactors_closed ? "Contactors closed" : "Contactors open", t.text_dim);
    int y = 156;
    if (alarms > 0) {
      c.text(FONT_M32, rx, y, event_title(list[0]->type), t.text);
      y = text_wrap(c, FONT_R20, rx, y + 44, half, event_help(list[0]->type), t.text_dim, 3) + 18;
      const int rest = std::min(alarms - 1, 3);
      for (int i = 0; i < rest; i++) {
        const Event &e = *list[1 + i];
        c.round_rect(rx, y + 3, 6, 20, 3, level_color(t, e.level));
        c.text(FONT_R20, rx + 18, y, event_title(e.type), t.text);
        y += 30;
      }
      if (alarms - 1 > rest) c.text(FONT_R18, rx + 18, y + 2, fmt("and %d more on Events  ›", alarms - 1 - rest), t.out);
    }
  } else {
    c.text(FONT_M24, rx, 72, linked ? flow_name(flow) : "POWER", flow_color(t, flow));
    const NumberUnit p = power_text(m.power);
    num_unit(c, rx - 4, 246, FONT_M128, p.number, FONT_M48, p.unit, linked ? t.text : t.hint, t.text_dim, LEFT, 12);
    // The current between its limits, zero in the middle of what is allowed
    const float chg = m.charge_limit, dis = m.discharge_limit;
    const bool known = !std::isnan(chg) && !std::isnan(dis) && !std::isinf(chg) && !std::isinf(dis) && chg + dis > 0;
    c.round_rect(rx, 280, half, 34, 10, t.track);
    const int zero = known ? rx + (int) (half * dis / (chg + dis)) : rx + half / 2;
    if (known && !std::isnan(m.current)) {
      const int at = zero + (int) (clampf(m.current, -dis, chg) / (chg + dis) * half);
      if (std::abs(at - zero) >= 4) c.round_rect(std::min(zero, at), 280, std::abs(at - zero), 34, 10, flow_color(t, flow));
    }
    c.fill(zero - 1, 274, 3, 46, t.text);
    c.text(FONT_R16, rx, 320, value(dis, "%.0f A out", ""), t.caption);
    c.text(FONT_R16, rx + half, 320, value(chg, "%.0f A in", ""), t.caption, RIGHT);
    if (!std::isnan(m.pack_voltage) && !std::isnan(m.current)) {
      c.text(FONT_R24, rx, 348, fmt("%.1f V   ·   %+.1f A", (double) m.pack_voltage, (double) m.current), t.text_dim);
    }
    if (chg == 0.0f && m.charge_limited_by[0]) {
      c.text(FONT_R20, rx, 386, fmt("Charging is blocked by %s", m.charge_limited_by), t.caption);
    } else if (dis == 0.0f && m.discharge_limited_by[0]) {
      c.text(FONT_R20, rx, 386, fmt("Discharging is blocked by %s", m.discharge_limited_by), t.caption);
    } else if (flow == Flow::CHARGING && m.charge_limited_by[0]) {
      c.text(FONT_R20, rx, 386, fmt("Charge limit set by %s", m.charge_limited_by), t.caption);
    } else if (flow == Flow::DISCHARGING && m.discharge_limited_by[0]) {
      c.text(FONT_R20, rx, 386, fmt("Discharge limit set by %s", m.discharge_limited_by), t.caption);
    }
  }

  // ---- one line of detail ----
  c.fill(0, FOOT_Y, SCREEN_W, CONTENT_H - FOOT_Y, t.card);
  c.fill(0, FOOT_Y, SCREEN_W, 1, t.card_border);
  const CellStats cells = cell_stats(m);
  const float spread = cells.high_mv - cells.low_mv;
  stat(c, t, 40, "CELLS",
       cells.count > 0 ? fmt("%.3f – %.3f", (double) cells.low_mv / 1000.0, (double) cells.high_mv / 1000.0) : "--",
       cells.count > 0 ? "V" : "", cells.count > 0 ? fmt("%.0f mV", (double) spread) : "", spread > 50 ? t.warning : t.good);
  stat(c, t, 440, "TEMPERATURE",
       std::isnan(m.temp_min) ? "--" : fmt("%.1f – %.1f", (double) m.temp_min, (double) m.temp_max),
       std::isnan(m.temp_min) ? "" : "°C", "", t.caption);
  stat(c, t, 730, "TODAY",
       std::isnan(m.today_in_kwh) ? "--" : fmt("+%.1f  −%.1f", (double) m.today_in_kwh, (double) m.today_out_kwh),
       std::isnan(m.today_in_kwh) ? "" : "kWh", "", t.caption);
  for (const int dx : {414, 704}) c.fill(dx, FOOT_Y + 16, 1, CONTENT_H - FOOT_Y - 32, t.card_border);

  if (!linked) no_link_overlay(c, t, m, 0, BAND_H, SCREEN_W, CONTENT_H - BAND_H);
  nav_bar(c, t, m, 0);
}

}  // namespace hmiui
