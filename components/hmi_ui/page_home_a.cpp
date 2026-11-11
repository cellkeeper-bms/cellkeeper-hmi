// Home page, design A, "dashboard": the Home Assistant pack card's layout on the display.
// A battery with the state of charge, the power with the limits it runs between, the last
// 24 hours, and a row of cards for cells, temperature and the day's energy.
#include "ui_draw.h"

#include <algorithm>

namespace hmiui {

// An upright battery filled to the state of charge, with an arrow for the direction of flow
// (down into the battery = charging, up and out = discharging)
static void battery_upright(Canvas &c, const Theme &t, int x, int y, int w, int h, float soc, Flow flow) {
  const Rgb shell = std::isnan(soc) ? t.hint : t.text;
  c.round_rect(x + w / 2 - 15, y - 10, 30, 16, 5, shell);
  c.round_rect(x, y, w, h, 14, shell);
  c.round_rect(x + 5, y + 5, w - 10, h - 10, 9, t.card);
  if (std::isnan(soc)) return;
  const int ix = x + 10, iy = y + 10, iw = w - 20, ih = h - 20;
  const int fh = std::max(6, (int) (ih * clampf(soc, 0, 100) / 100.0f));
  const Rgb fill = soc_color(t, soc);
  c.round_rect(ix, iy + ih - fh, iw, fh, 5, fill);
  if (flow != Flow::CHARGING && flow != Flow::DISCHARGING) return;
  const float cx = x + w / 2.0f, cy = y + h / 2.0f, dir = flow == Flow::CHARGING ? 1.0f : -1.0f;
  auto arrow = [&](Rgb color) {
    c.line(cx, cy - 24 * dir, cx, cy + 6 * dir, 11.0f, color);
    c.triangle(cx, cy + 28 * dir, cx - 19, cy + 4 * dir, cx + 19, cy + 4 * dir, color);
  };
  // In the fill's colour over the empty part, dark over the fill
  c.set_clip(ix, iy, iw, ih - fh);
  arrow(fill);
  c.set_clip(ix, iy + ih - fh, iw, fh);
  arrow(t.card);
  c.reset_clip();
}

// State of charge as a line and power as bars over the last 24 hours
static void history_chart(Canvas &c, const Theme &t, const Model &m, int x, int y, int w, int h) {
  c.text(FONT_R14, x, y, "LAST 24 H", t.caption);
  const int gy = y + 26, gh = h - 26 - 20;
  c.fill(x, gy, w, 1, t.card_border);
  c.fill(x, gy + gh / 2, w, 1, t.card_border);
  c.fill(x, gy + gh, w, 1, t.card_border);
  c.text(FONT_R14, x, gy + gh + 4, "24 h ago", t.hint);
  c.text(FONT_R14, x + w, gy + gh + 4, "now", t.hint, RIGHT);
  if (m.history_count < 2) {
    c.text(FONT_R16, x + w / 2, gy + gh / 2 - 22, "No history yet", t.hint, CENTER);
    return;
  }
  float peak = 1.0f;
  for (int i = 0; i < m.history_count; i++) {
    if (!std::isnan(m.history_kw[i])) peak = std::max(peak, std::fabs(m.history_kw[i]));
  }
  c.text(FONT_R14, x + w, y, fmt("SoC  ·  ±%.0f kW", (double) std::ceil(peak)), t.hint, RIGHT);
  const int first = HISTORY_POINTS - m.history_count;  // the newest point sits at the right edge
  const float mid = gy + gh / 2.0f;
  for (int i = 0; i < m.history_count; i++) {
    const float kw = m.history_kw[i];
    if (std::isnan(kw)) continue;
    const int x0 = x + (first + i) * w / HISTORY_POINTS, x1 = x + (first + i + 1) * w / HISTORY_POINTS;
    const int bh = (int) (std::fabs(kw) / std::ceil(peak) * (gh / 2 - 2));
    if (bh < 1) continue;
    c.fill(x0, kw > 0 ? (int) mid - bh : (int) mid + 1, std::max(1, x1 - x0), bh, mix(t.card, kw > 0 ? t.good : t.out, 0.6f));
  }
  float px = 0, py = 0;
  bool have = false;
  for (int i = 0; i < m.history_count; i++) {
    const float soc = m.history_soc[i];
    if (std::isnan(soc)) {
      have = false;
      continue;
    }
    const float cx = x + (first + i + 0.5f) * w / HISTORY_POINTS, cy = gy + gh - clampf(soc, 0, 100) / 100.0f * gh;
    if (have) c.line(px, py, cx, cy, 2.2f, t.text);
    px = cx;
    py = cy;
    have = true;
  }
}

// The current between the limits it may run in: discharge limit at the left end, charge limit
// at the right, a mark at zero and a dot where the current is now
static void limits_bar(Canvas &c, const Theme &t, const Model &m, int x, int y, int w) {
  const float chg = m.charge_limit, dis = m.discharge_limit;
  const bool known = !std::isnan(chg) && !std::isnan(dis) && !std::isinf(chg) && !std::isinf(dis) && chg + dis > 0;
  c.round_rect(x, y, w, 8, 4, t.track);
  const int zero = known ? x + (int) (w * dis / (chg + dis)) : x + w / 2;
  if (known && !std::isnan(m.current)) {
    const float a = clampf(m.current, -dis, chg);
    const int at = zero + (int) (a / (chg + dis) * w);
    const Rgb color = flow_color(t, flow_of(m));
    if (at != zero) c.round_rect(std::min(zero, at), y, std::abs(at - zero), 8, 4, color);
    c.disc((float) at, y + 4.0f, 8.0f, t.text);
    c.disc((float) at, y + 4.0f, 4.5f, color);
  }
  c.fill(zero - 1, y - 5, 2, 18, t.text_dim);

  auto label = [&](int lx, Align align, float amps, const char *word, const char *by) {
    const char *head = std::isnan(amps) ? fmt("-- %s", word)
                       : (std::isinf(amps) ? fmt("No %s limit", word) : fmt("%.*f A %s", amps < 10 ? 1 : 0, (double) amps, word));
    const char *tail = (!std::isnan(amps) && !std::isinf(amps) && by[0]) ? fmt("  ·  %s", by) : "";
    const int wh = Canvas::text_width(FONT_M16, head), wt = Canvas::text_width(FONT_R16, tail);
    const int x0 = align == RIGHT ? lx - wh - wt : lx;
    c.text(FONT_M16, x0, y + 20, head, t.text);
    c.text(FONT_R16, x0 + wh, y + 20, tail, t.caption);
  };
  label(x, LEFT, dis, "discharge", m.discharge_limited_by);
  label(x + w, RIGHT, chg, "charge", m.charge_limited_by);
}

static void stat(Canvas &c, const Theme &t, int x, int y, const char *caption, const char *number, const char *unit,
                 Rgb color) {
  c.text(FONT_R14, x, y, caption, t.caption);
  num_unit(c, x, y + 46, FONT_M24, number, FONT_R18, unit, color, t.caption);
}

void draw_home_a(Canvas &c, const Theme &t, const Model &m) {
  const Flow flow = flow_of(m);
  const bool linked = m.link == Link::OK;

  // ---- header: the pack, what is wrong, the state ----
  int x = 24;
  x += c.text(FONT_M24, x, 13, m.pack_name[0] ? m.pack_name : "CellKeeper", t.text) + 12;
  if (m.cell_count > 0 && !std::isnan(m.capacity_ah)) {
    x += c.text(FONT_R18, x, 19, fmt("%ds  ·  %.0f Ah", m.cell_count, (double) m.capacity_ah), t.caption);
  }
  x += 24;
  int rx = SCREEN_W - 16;
  if (m.clock[0]) rx -= c.text(FONT_R20, rx, 17, m.clock, t.text_dim, RIGHT) + 16;
  if (linked) {
    rx -= pill(c, rx, 10, m.contactors_closed ? "Contactors closed" : "Contactors open",
               m.contactors_closed ? t.good : t.text_dim, m.contactors_closed ? mix(t.background, t.good, 0.16f) : t.track,
               RIGHT) + 8;
    const bool fault = m.system_state == SystemState::FAULT, run = m.system_state == SystemState::OPERATING;
    rx -= pill(c, rx, 10, system_state_name(m.system_state), fault ? 0xFFFFFF : (run ? t.good : t.text_dim),
               fault ? t.fatal : (run ? mix(t.background, t.good, 0.16f) : t.track), RIGHT, true);
  } else {
    rx -= pill(c, rx, 10, m.link == Link::CONNECTING ? "Connecting" : "No BMS link", 0x000000, t.warning, RIGHT, true);
  }
  // Active warnings and worse, as many as fit between the name and the state
  const Event *list[MAX_EVENTS];
  const int n = top_events(m, list, MAX_EVENTS, Level::WARNING);
  for (int i = 0; i < n; i++) {
    const Event &e = *list[i];
    const char *label = e.count > 1 ? fmt("%s ×%u", event_title(e.type), (unsigned) e.count) : event_title(e.type);
    const char *rest = fmt("+%d more", n - i);
    const int need = pill_width(label, true) + (i < n - 1 ? 8 + pill_width(fmt("+%d more", n - i - 1)) : 0);
    if (x + need > rx - 16) {
      pill(c, x, 10, rest, t.text, t.track);
      break;
    }
    const bool solid = e.level == Level::FATAL;
    x += pill(c, x, 10, label, solid ? 0xFFFFFF : level_color(t, e.level),
              solid ? t.fatal : mix(t.background, level_color(t, e.level), 0.2f), LEFT, true) + 8;
  }

  // ---- the pack ----
  card(c, t, 16, 56, 992, 262);
  battery_upright(c, t, 44, 98, 80, 196, m.soc, flow);
  c.text(FONT_R14, 152, 96, "STATE OF CHARGE", t.caption);
  {
    const Rgb color = std::isnan(m.soc) ? t.hint : soc_color(t, m.soc);
    num_unit(c, 150, 186, FONT_M72, value(m.soc, m.soc >= 99.95f ? "%.0f" : "%.1f"), FONT_R32, "%", color, color);
  }
  if (!std::isnan(m.soc) && !std::isnan(m.capacity_ah) && !std::isnan(m.pack_voltage)) {
    c.text(FONT_R20, 152, 204, fmt("≈ %.1f kWh  ·  %.0f Ah", (double) kwh_left(m), (double) ah_left(m)), t.text_dim);
  }
  if (linked && !std::isnan(m.soc_change_1h) && !std::isnan(m.soc)) {
    c.text(FONT_R16, 152, 238, fmt("%+.1f %% in the last hour", (double) m.soc_change_1h), t.caption);
  }

  c.text(FONT_M16, 430, 74, linked ? flow_name(flow) : "", flow_color(t, flow));
  const NumberUnit p = power_text(m.power);
  num_unit(c, 428, 152, FONT_M72, p.number, FONT_R32, p.unit, linked ? t.text : t.hint, t.text_dim, LEFT, 8);
  c.text(FONT_R20, 430, 164, time_to_text(m), t.text_dim);
  stat(c, t, 430, 202, "CURRENT", value(m.current, "%+.1f"), "A", t.text);
  stat(c, t, 548, 202, "PACK", value(m.pack_voltage, "%.1f"), "V", t.text);
  stat(c, t, 666, 202, "OUTPUT", value(m.output_voltage, "%.1f"), "V", t.text);
  history_chart(c, t, m, 790, 74, 198, 176);
  limits_bar(c, t, m, 430, 268, 558);

  // ---- cells ----
  card(c, t, 16, 330, 500, 198);
  const CellStats cells = cell_stats(m);
  x = 32 + c.text(FONT_R14, 32, 344, "CELLS", t.caption) + 10;
  if (cells.count > 0) c.text(FONT_R14, x, 344, fmt("%d in series", cells.count), t.hint);
  c.text(FONT_R16, 500, 343, "All cells ›", t.out, RIGHT);
  if (cells.count > 0) {
    c.disc(37.0f, 381.0f, 4.5f, t.cell_low);
    stat(c, t, 48, 372, fmt("LOWEST  ·  #%d", cells.lowest + 1), fmt("%.3f", (double) cells.low_mv / 1000.0), "V", t.text);
    c.disc(205.0f, 381.0f, 4.5f, t.cell_high);
    stat(c, t, 216, 372, fmt("HIGHEST  ·  #%d", cells.highest + 1), fmt("%.3f", (double) cells.high_mv / 1000.0), "V",
         t.text);
    const float spread = cells.high_mv - cells.low_mv;
    stat(c, t, 384, 372, "SPREAD", fmt("%.0f", (double) spread), "mV", spread > 50 ? t.warning : t.good);
    cell_skyline(c, t, m, 32, 438, 468, 76);
  } else {
    cell_skyline(c, t, m, 32, 372, 468, 140);
  }

  // ---- temperature ----
  card(c, t, 528, 330, 236, 198);
  c.text(FONT_R14, 544, 344, "TEMPERATURE", t.caption);
  if (!std::isnan(m.temp_min)) {
    num_unit(c, 544, 402, FONT_M32, fmt("%.1f – %.1f", (double) m.temp_min, (double) m.temp_max), FONT_R20, "°C", t.good,
             t.caption);
    c.text(FONT_R14, 544, 414, fmt("%d modules  ·  spread %.1f °C", m.module_count, (double) (m.temp_max - m.temp_min)),
           t.caption);
    const int n_mod = std::min(m.module_count, MAX_MODULES), pitch = 204 / std::max(n_mod, 4);
    for (int i = 0; i < n_mod; i++) {
      const float temp = m.module_temp[i];
      if (std::isnan(temp)) continue;
      const int bx = 544 + i * pitch + (pitch - 18) / 2, by = 446, bh = 48;
      const int fh = std::max(3, (int) (clampf(temp, 0, 50) / 50.0f * bh));
      c.round_rect(bx, by, 18, bh, 4, t.track);
      c.round_rect(bx, by + bh - fh, 18, fh, 4, t.good);
      c.text(FONT_R14, bx + 9, by + bh + 3, fmt("%.0f°", (double) temp), t.caption, CENTER);
    }
  } else {
    c.text(FONT_R18, 646, 418, "No readings", t.hint, CENTER);
  }

  // ---- the day's energy ----
  card(c, t, 776, 330, 232, 198);
  c.text(FONT_R14, 792, 344, "ENERGY TODAY", t.caption);
  if (!std::isnan(m.today_in_kwh) && !std::isnan(m.today_out_kwh)) {
    num_unit(c, 792, 402, FONT_M32, fmt("%+.1f", (double) (m.today_in_kwh - m.today_out_kwh)), FONT_R20, "kWh net", t.text,
             t.caption, LEFT, 7);
    const float top = std::max(std::max(m.today_in_kwh, m.today_out_kwh), 0.1f);
    auto row = [&](int y, const char *word, float kwh, Rgb color) {
      c.text(FONT_R16, 792, y, word, t.text_dim);
      c.text(FONT_M16, 992, y, fmt("%.1f kWh", (double) kwh), t.text, RIGHT);
      c.round_rect(792, y + 24, 200, 6, 3, t.track);
      const int bw = (int) (200 * kwh / top);
      if (bw >= 6) c.round_rect(792, y + 24, bw, 6, 3, color);
    };
    row(420, "Charged", m.today_in_kwh, t.good);
    row(468, "Discharged", m.today_out_kwh, t.out);
  } else {
    c.text(FONT_R18, 892, 418, "No readings", t.hint, CENTER);
  }

  if (!linked) no_link_overlay(c, t, m, 0, 52, SCREEN_W, SCREEN_H - NAV_BAR_H - 52);
  nav_bar(c, t, m, 0);
}

}  // namespace hmiui
