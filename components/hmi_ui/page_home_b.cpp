// Home page, design B, "power path": the pack, the contactors and the inverter drawn as the
// circuit they are, with the power on the line between them. Navigation is a rail on the left.
#include "ui_draw.h"

#include <algorithm>

namespace hmiui {

static const int X0 = NAV_RAIL_W + 16, X1 = SCREEN_W - 16;

// A battery lying down, filled to the state of charge, with the number inside
static void battery_flat(Canvas &c, const Theme &t, int x, int y, int w, int h, float soc) {
  const Rgb shell = std::isnan(soc) ? t.hint : t.text;
  c.round_rect(x + w - 2, y + h / 2 - 24, 14, 48, 5, shell);
  c.round_rect(x, y, w, h, 18, shell);
  c.round_rect(x + 5, y + 5, w - 10, h - 10, 13, t.card);
  const int ix = x + 11, iy = y + 11, iw = w - 22, ih = h - 22;
  const int cx = x + w / 2, baseline = y + h / 2 + 26;
  if (std::isnan(soc)) {
    num_unit(c, cx, baseline, FONT_M72, "--", FONT_M32, "%", t.hint, t.hint, CENTER);
    return;
  }
  const int fw = std::max(8, (int) (iw * clampf(soc, 0, 100) / 100.0f));
  const Rgb fill = soc_color(t, soc);
  c.round_rect(ix, iy, fw, ih, 7, fill);
  const char *number = fmt("%.0f", (double) soc);
  // Dark on the filled part, light on the empty part
  c.set_clip(ix, iy, fw, ih);
  num_unit(c, cx, baseline, FONT_M72, number, FONT_M32, "%", t.background, t.background, CENTER);
  c.set_clip(ix + fw, iy, iw - fw, ih);
  num_unit(c, cx, baseline, FONT_M72, number, FONT_M32, "%", t.text, t.text, CENTER);
  c.reset_clip();
}

// A stretch of the line between pack and inverter, with arrow heads when power flows
static void bus(Canvas &c, int xa, int xb, int y, Rgb color, int dir) {
  c.fill(xa, y - 2, xb - xa, 5, color);
  if (dir == 0) return;
  const int count = (xb - xa) / 64;
  for (int i = 0; i < count; i++) {
    const float x = xa + (i + 0.5f) * (xb - xa) / count;
    c.line(x - 7.0f * dir, y - 11.0f, x + 5.0f * dir, (float) y, 4.0f, color);
    c.line(x - 7.0f * dir, y + 11.0f, x + 5.0f * dir, (float) y, 4.0f, color);
  }
}

void draw_home_b(Canvas &c, const Theme &t, const Model &m) {
  const Flow flow = flow_of(m);
  const bool linked = m.link == Link::OK;
  const Event *list[MAX_EVENTS];
  const int alarms = top_events(m, list, MAX_EVENTS, Level::WARNING);

  // ---- top line: the pack and its state, or what is wrong ----
  if (alarms > 0) {
    const Level worst = list[0]->level;
    const Rgb on = level_text_color(worst);
    c.round_rect(X0, 12, X1 - X0, 44, 10, level_color(t, worst));
    warning_sign(c, X0 + 30.0f, 34.0f, 26.0f, on, level_color(t, worst));
    int x = X0 + 56;
    x += c.text(FONT_B20, x, 22, level_name(worst), on) + 14;
    c.text(FONT_R20, x, 22, event_title(list[0]->type), on);
    c.text(FONT_R20, X1 - 18, 22, alarms > 1 ? fmt("+%d more  ›", alarms - 1) : "Events  ›", on, RIGHT);
  } else {
    int x = X0 + 4;
    x += c.text(FONT_M24, x, 18, m.pack_name[0] ? m.pack_name : "CellKeeper", t.text) + 12;
    if (m.cell_count > 0 && !std::isnan(m.capacity_ah)) {
      c.text(FONT_R18, x, 24, fmt("%ds  ·  %.0f Ah", m.cell_count, (double) m.capacity_ah), t.caption);
    }
    int rx = X1;
    if (m.clock[0]) rx -= c.text(FONT_R20, rx, 22, m.clock, t.text_dim, RIGHT) + 16;
    if (linked) {
      const bool run = m.system_state == SystemState::OPERATING;
      pill(c, rx, 15, system_state_name(m.system_state), run ? t.good : t.text_dim,
           run ? mix(t.background, t.good, 0.16f) : t.track, RIGHT, true);
    } else {
      pill(c, rx, 15, "No BMS link", 0x000000, t.warning, RIGHT, true);
    }
  }

  // ---- the power path ----
  card(c, t, X0, 66, X1 - X0, 272);
  const int by = 196;  // the line's height
  // Pack
  c.text(FONT_R14, X0 + 24, 82, "PACK", t.caption);
  c.text(FONT_M20, X0 + 24 + 252, 78, value(m.pack_voltage, "%.1f V", "-- V"), t.text, RIGHT);
  battery_flat(c, t, X0 + 24, 116, 252, 160, m.soc);
  if (!std::isnan(m.soc) && !std::isnan(m.capacity_ah) && !std::isnan(m.pack_voltage)) {
    c.text(FONT_R18, X0 + 24 + 126, 290, fmt("≈ %.1f kWh  ·  %.0f Ah", (double) kwh_left(m), (double) ah_left(m)), t.text_dim,
           CENTER);
  }
  // Inverter
  const int ix = X1 - 24 - 150;
  c.text(FONT_R14, ix, 82, "INVERTER", t.caption);
  c.text(FONT_M20, ix + 150, 78, value(m.output_voltage, "%.1f V", "-- V"), t.text, RIGHT);
  const Rgb box = linked ? t.text : t.hint;
  c.round_rect(ix, 116, 150, 160, 18, box);
  c.round_rect(ix + 5, 121, 140, 150, 13, t.card);
  {
    // A sine wave: alternating current
    float px = 0, py = 0;
    for (int i = 0; i <= 32; i++) {
      const float x = ix + 35 + i * 2.5f, y = by - 22.0f * std::sin(i / 32.0f * 6.2832f);
      if (i > 0) c.line(px, py, x, y, 4.0f, box);
      px = x;
      py = y;
    }
  }
  if (!std::isnan(m.inverter_soc)) {
    c.text(FONT_R18, ix + 75, 290, fmt("is told %.0f %%", (double) m.inverter_soc), t.text_dim, CENTER);
  }
  // The line: live on the pack side, switched by the contactors
  const int xa = X0 + 24 + 252 + 14, xb = ix, sw_a = (xa + xb) / 2 - 36, sw_b = (xa + xb) / 2 + 36;
  const Rgb live = flow == Flow::CHARGING || flow == Flow::DISCHARGING ? flow_color(t, flow) : (linked ? t.text_dim : t.track);
  const Rgb far = m.contactors_closed ? live : t.track;
  const int dir = !m.contactors_closed ? 0 : (flow == Flow::CHARGING ? -1 : (flow == Flow::DISCHARGING ? 1 : 0));
  bus(c, xa, sw_a, by, live, dir);
  bus(c, sw_b, xb, by, far, dir);
  if (m.contactors_closed) {
    c.fill(sw_a, by - 2, sw_b - sw_a, 5, live);
  } else {
    c.line((float) sw_a, (float) by, sw_a + 66.0f, by - 24.0f, 5.0f, live);
  }
  c.disc((float) sw_a, by + 0.5f, 8.0f, live);
  c.disc((float) sw_b, by + 0.5f, 8.0f, far);
  const int mid = (xa + xb) / 2;
  c.text(FONT_R16, mid, by + 18, !linked ? "" : (m.contactors_closed ? "Contactors closed" : "Contactors open"),
         m.contactors_closed ? t.caption : t.text, CENTER);
  // The power, on the line
  if (linked && m.system_state == SystemState::FAULT) {
    c.text(FONT_M20, mid, 76, "FAULT", t.fatal, CENTER);
  } else {
    c.text(FONT_M20, mid, 76, linked ? flow_name(flow) : "", flow_color(t, flow), CENTER);
  }
  const NumberUnit p = power_text(m.power);
  num_unit(c, mid, 160, FONT_M72, p.number, FONT_R32, p.unit, linked ? t.text : t.hint, t.text_dim, CENTER, 8);
  {
    const char *eta = time_to_text(m);
    const char *line = std::isnan(m.current) ? "" : (eta[0] ? fmt("%+.1f A   ·   %s", (double) m.current, eta)
                                                             : fmt("%+.1f A", (double) m.current));
    c.text(FONT_R20, mid, 252, line, t.text_dim, CENTER);
  }

  // ---- cells, or what is wrong when there are none ----
  const int cw = 548;
  card(c, t, X0, 350, cw, 234);
  const CellStats cells = cell_stats(m);
  if (cells.count > 0) {
    int x = X0 + 16;
    x += c.text(FONT_R14, x, 364, "CELLS", t.caption) + 10;
    c.text(FONT_R14, x, 364, fmt("%d in series", cells.count), t.hint);
    c.text(FONT_R16, X0 + cw - 16, 363, "All cells ›", t.out, RIGHT);
    const float spread = cells.high_mv - cells.low_mv;
    auto stat = [&](int sx, const char *caption, const char *number, const char *unit, Rgb color) {
      c.text(FONT_R14, sx, 390, caption, t.caption);
      num_unit(c, sx, 436, FONT_M24, number, FONT_R18, unit, color, t.caption);
    };
    stat(X0 + 16, "LOWEST – HIGHEST", fmt("%.3f – %.3f", (double) cells.low_mv / 1000.0, (double) cells.high_mv / 1000.0),
         "V", t.text);
    stat(X0 + 250, "SPREAD", fmt("%.0f", (double) spread), "mV", spread > 50 ? t.warning : t.good);
    if (!std::isnan(m.temp_min)) {
      stat(X0 + 370, "TEMPERATURE", fmt("%.1f – %.1f", (double) m.temp_min, (double) m.temp_max), "°C", t.text);
    }
    cell_skyline(c, t, m, X0 + 16, 456, cw - 32, 88);
    c.text(FONT_R14, X0 + 16, 552, "1", t.hint);
    c.text(FONT_R14, X0 + cw - 16, 552, fmt("%d", cells.count), t.hint, RIGHT);
    c.disc(X0 + cw / 2 - 118.0f, 561.0f, 4.5f, t.cell_low);
    c.text(FONT_R14, X0 + cw / 2 - 108, 552, fmt("lowest #%d", cells.lowest + 1), t.caption);
    c.disc(X0 + cw / 2 + 22.0f, 561.0f, 4.5f, t.cell_high);
    c.text(FONT_R14, X0 + cw / 2 + 32, 552, fmt("highest #%d", cells.highest + 1), t.caption);
  } else if (linked) {
    const int all = top_events(m, list, MAX_EVENTS, Level::INFO);
    c.text(FONT_R14, X0 + 16, 364, all > 0 ? "ACTIVE EVENTS" : "CELLS", t.caption);
    if (all == 0) cell_skyline(c, t, m, X0 + 16, 390, cw - 32, 170);
    const int rows = std::min(all, 4);
    for (int i = 0; i < rows; i++) {
      const Event &e = *list[i];
      const int y = 390 + i * 47;
      c.round_rect(X0 + 16, y + 2, 6, 38, 3, level_color(t, e.level));
      c.text(FONT_M20, X0 + 34, y, event_title(e.type), t.text);
      c.text(FONT_R14, X0 + cw - 16, y + 4, e.count > 1 ? fmt("%s  ×%u", level_name(e.level), (unsigned) e.count)
                                                         : level_name(e.level), t.caption, RIGHT);
      c.text(FONT_R16, X0 + 34, y + 23, event_help(e.type), t.text_dim);
    }
    if (all > rows) c.text(FONT_R14, X0 + cw - 16, 364, fmt("+%d more on Events ›", all - rows), t.out, RIGHT);
  }

  // ---- limits and the day ----
  const int rx0 = X0 + cw + 12, rw = X1 - rx0;
  card(c, t, rx0, 350, rw, 234);
  c.text(FONT_R14, rx0 + 16, 364, "LIMITS", t.caption);
  auto limit = [&](int y, const char *word, float amps, const char *by_what, Rgb color) {
    c.text(FONT_R18, rx0 + 16, y + 4, word, t.text_dim);
    const char *number = std::isnan(amps) ? "--" : (std::isinf(amps) ? "none" : fmt("%.*f", amps < 10 ? 1 : 0, (double) amps));
    num_unit(c, rx0 + rw - 16, y + 26, FONT_M24, number, FONT_R18, std::isinf(amps) ? "" : "A",
             amps == 0.0f ? t.caption : color, t.caption, RIGHT);
    if (!std::isnan(amps) && !std::isinf(amps) && by_what[0]) {
      c.text(FONT_R14, rx0 + 16, y + 30, amps == 0.0f ? fmt("blocked by %s", by_what) : fmt("set by %s", by_what), t.caption);
    }
  };
  limit(386, "Charge", m.charge_limit, m.charge_limited_by, t.good);
  limit(440, "Discharge", m.discharge_limit, m.discharge_limited_by, t.out);
  c.fill(rx0 + 16, 498, rw - 32, 1, t.card_border);
  c.text(FONT_R14, rx0 + 16, 510, "TODAY", t.caption);
  if (!std::isnan(m.today_in_kwh) && !std::isnan(m.today_out_kwh)) {
    int x = rx0 + 16;
    x += num_unit(c, x, 560, FONT_M24, fmt("%.1f", (double) m.today_in_kwh), FONT_R16, "in", t.good, t.caption) + 16;
    x += num_unit(c, x, 560, FONT_M24, fmt("%.1f", (double) m.today_out_kwh), FONT_R16, "out", t.out, t.caption);
    c.text(FONT_R16, rx0 + rw - 16, 541, "kWh", t.caption, RIGHT);
  } else {
    c.text(FONT_R18, rx0 + 16, 536, "--", t.hint);
  }

  if (!linked) no_link_overlay(c, t, m, NAV_RAIL_W, 62, SCREEN_W - NAV_RAIL_W, SCREEN_H - 62);
  nav_rail(c, t, m, 0);
}

}  // namespace hmiui
