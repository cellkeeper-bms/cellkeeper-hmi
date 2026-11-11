// Home: the pack, the contactors and the inverter drawn as the circuit they are, with the power
// on the line between them, and under it what the BMS tells the inverter it may do (voltage
// targets and current limits). Below: the cells at a glance, or what is wrong when there are
// none, and the day's energy.
#include "ui_draw.h"

#include <algorithm>

namespace hmiui {

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

// What the BMS tells the inverter over CAN: a dashed line towards the inverter, and under it
// the state of charge, the two voltage targets and the two current limits with what sets them
static void sent_to_inverter(Canvas &c, const Theme &t, const Model &m, int y) {
  const int xa = X0 + 24, xb = X1 - 24;
  for (int x = xa; x < xb - 14; x += 12) c.fill(x, y, 6, 2, t.hint);
  c.triangle((float) xb, y + 1.0f, xb - 13.0f, y - 6.0f, xb - 13.0f, y + 8.0f, t.hint);
  c.text(FONT_R14, xa, y + 12, "SENT TO", t.caption);
  c.text(FONT_R14, xa, y + 30, "THE INVERTER", t.caption);
  c.text(FONT_R14, xa, y + 54, "over CAN", t.hint);

  const bool linked = m.link == Link::OK;
  stat(c, t, xa + 150, y + 12, "SOC", value(m.inverter_soc, "%.0f"), "%", t.text);
  stat(c, t, xa + 250, y + 12, "CHARGE TO", value(m.inverter_charge_v, "%.1f"), "V", t.text);
  stat(c, t, xa + 392, y + 12, "DISCHARGE TO", value(m.inverter_discharge_v, "%.1f"), "V", t.text);
  auto limit = [&](int x, const char *caption, float amps, const char *by_what, Rgb color) {
    const char *number = !linked || std::isnan(amps) ? "--"
                         : (std::isinf(amps) ? "none" : fmt("%.*f", amps < 10 ? 1 : 0, (double) amps));
    stat(c, t, x, y + 12, caption, number, std::isinf(amps) ? "" : "A", amps == 0.0f ? t.text_dim : color);
    if (linked && !std::isnan(amps) && !std::isinf(amps) && by_what[0]) {
      c.text(FONT_R14, x, y + 64, amps == 0.0f ? fmt("blocked by %s", by_what) : by_what, t.caption);
    }
  };
  limit(xa + 536, "CHARGE LIMIT", m.charge_limit, m.charge_limited_by, t.good);
  limit(xa + 712, "DISCHARGE LIMIT", m.discharge_limit, m.discharge_limited_by, t.out);
}

void draw_home(Canvas &c, const Theme &t, const Model &m) {
  const Flow flow = flow_of(m);
  const bool linked = m.link == Link::OK;
  const Event *list[MAX_EVENTS];

  top_line(c, t, m, m.pack_name[0] ? m.pack_name : "CellKeeper",
           m.cell_count > 0 && !std::isnan(m.capacity_ah) ? fmt("%ds  ·  %.0f Ah", m.cell_count, (double) m.capacity_ah) : "");

  // ---- the power path ----
  card(c, t, X0, Y0, X1 - X0, 328);
  const int by = 180;  // the line's height
  // Pack
  c.text(FONT_R14, X0 + 24, 80, "PACK", t.caption);
  c.text(FONT_M20, X0 + 24 + 252, 76, value(m.pack_voltage, "%.1f V", "-- V"), t.text, RIGHT);
  battery_flat(c, t, X0 + 24, 106, 252, 148, m.soc);
  if (!std::isnan(m.soc) && !std::isnan(m.capacity_ah) && !std::isnan(m.pack_voltage)) {
    c.text(FONT_R18, X0 + 24 + 126, 262, fmt("≈ %.1f kWh  ·  %.0f Ah", (double) kwh_left(m), (double) ah_left(m)), t.text_dim,
           CENTER);
  }
  // Inverter
  const int ix = X1 - 24 - 150;
  c.text(FONT_R14, ix, 80, "INVERTER", t.caption);
  c.text(FONT_M20, ix + 150, 76, value(m.output_voltage, "%.1f V", "-- V"), t.text, RIGHT);
  const Rgb box = linked ? t.text : t.hint;
  c.round_rect(ix, 106, 150, 148, 18, box);
  c.round_rect(ix + 5, 111, 140, 138, 13, t.card);
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
    c.line((float) sw_a, (float) by, sw_a + 68.0f, by - 18.0f, 5.0f, live);
  }
  c.disc((float) sw_a, by + 0.5f, 8.0f, live);
  c.disc((float) sw_b, by + 0.5f, 8.0f, far);
  const int mid = (xa + xb) / 2;
  c.text(FONT_R16, mid, by + 17, !linked ? "" : (m.contactors_closed ? "Contactors closed" : "Contactors open"),
         m.contactors_closed ? t.caption : t.text, CENTER);
  // The power, on the line
  if (linked && m.system_state == SystemState::FAULT) {
    c.text(FONT_M20, mid, 74, "FAULT", t.fatal, CENTER);
  } else {
    c.text(FONT_M20, mid, 74, linked ? flow_name(flow) : "", flow_color(t, flow), CENTER);
  }
  const NumberUnit p = power_text(m.power);
  num_unit(c, mid, 152, FONT_M72, p.number, FONT_R32, p.unit, linked ? t.text : t.hint, t.text_dim, CENTER, 8);
  {
    const char *eta = time_to_text(m);
    const char *line = std::isnan(m.current) ? "" : (eta[0] ? fmt("%+.1f A   ·   %s", (double) m.current, eta)
                                                             : fmt("%+.1f A", (double) m.current));
    c.text(FONT_R20, mid, 228, line, t.text_dim, CENTER);
  }
  sent_to_inverter(c, t, m, 298);

  // ---- cells, or what is wrong when there are none ----
  const int cy = Y0 + 328 + 12, ch = Y1 - cy, cw = 592;
  card(c, t, X0, cy, cw, ch);
  const CellStats cells = cell_stats(m);
  if (cells.count > 0) {
    int x = X0 + 16;
    x += c.text(FONT_R14, x, cy + 12, "CELLS", t.caption) + 10;
    c.text(FONT_R14, x, cy + 12, fmt("%d in series", cells.count), t.hint);
    c.text(FONT_R16, X0 + cw - 16, cy + 11, "All cells ›", t.out, RIGHT);
    const float spread = cells.high_mv - cells.low_mv;
    stat(c, t, X0 + 16, cy + 36, fmt("LOWEST #%d – HIGHEST #%d", cells.lowest + 1, cells.highest + 1),
         fmt("%.3f – %.3f", (double) cells.low_mv / 1000.0, (double) cells.high_mv / 1000.0), "V", t.text);
    stat(c, t, X0 + 278, cy + 36, "SPREAD", fmt("%.0f", (double) spread), "mV", spread > 50 ? t.warning : t.good);
    if (!std::isnan(m.temp_min)) {
      stat(c, t, X0 + 396, cy + 36, "TEMPERATURE", fmt("%.1f – %.1f", (double) m.temp_min, (double) m.temp_max), "°C", t.text);
    }
    cell_skyline(c, t, m, X0 + 16, cy + 96, cw - 32, ch - 96 - 14);
  } else if (linked) {
    const int all = top_events(m, list, MAX_EVENTS, Level::INFO);
    c.text(FONT_R14, X0 + 16, cy + 12, all > 0 ? "ACTIVE EVENTS" : "CELLS", t.caption);
    if (all == 0) cell_skyline(c, t, m, X0 + 16, cy + 30, cw - 32, ch - 50);
    const int rows = std::min(all, 3);
    for (int i = 0; i < rows; i++) {
      const Event &e = *list[i];
      const int y = cy + 36 + i * 46;
      c.round_rect(X0 + 16, y + 2, 6, 38, 3, level_color(t, e.level));
      c.text(FONT_M20, X0 + 34, y, event_title(e.type), t.text);
      c.text(FONT_R14, X0 + cw - 16, y + 4, e.count > 1 ? fmt("%s  ×%u", level_name(e.level), (unsigned) e.count)
                                                         : level_name(e.level), t.caption, RIGHT);
      c.text(FONT_R16, X0 + 34, y + 23, event_help(e.type), t.text_dim);
    }
    if (all > rows) c.text(FONT_R14, X0 + cw - 16, cy + 12, fmt("+%d more on Events ›", all - rows), t.out, RIGHT);
  }

  // ---- the day's energy ----
  const int rx0 = X0 + cw + 12, rw = X1 - rx0;
  card(c, t, rx0, cy, rw, ch);
  c.text(FONT_R14, rx0 + 16, cy + 12, "ENERGY TODAY", t.caption);
  if (!std::isnan(m.today_in_kwh) && !std::isnan(m.today_out_kwh)) {
    num_unit(c, rx0 + rw - 16, cy + 28, FONT_M20, fmt("%+.1f", (double) (m.today_in_kwh - m.today_out_kwh)), FONT_R14,
             "kWh net", t.text, t.caption, RIGHT);
    const float top = std::max(std::max(m.today_in_kwh, m.today_out_kwh), 0.1f);
    auto energy = [&](int y, const char *word, float kwh, Rgb color) {
      c.text(FONT_R18, rx0 + 16, y, word, t.text_dim);
      num_unit(c, rx0 + rw - 16, y + 20, FONT_M24, fmt("%.1f", (double) kwh), FONT_R16, "kWh", t.text, t.caption, RIGHT);
      c.round_rect(rx0 + 16, y + 30, rw - 32, 6, 3, t.track);
      const int bw = (int) ((rw - 32) * kwh / top);
      if (bw >= 6) c.round_rect(rx0 + 16, y + 30, bw, 6, 3, color);
    };
    energy(cy + 50, "Charged", m.today_in_kwh, t.good);
    energy(cy + 104, "Discharged", m.today_out_kwh, t.out);
  } else {
    c.text(FONT_R18, rx0 + rw / 2, cy + ch / 2 - 10, "No readings", t.hint, CENTER);
  }

  page_end(c, t, m, 0);
}

}  // namespace hmiui
