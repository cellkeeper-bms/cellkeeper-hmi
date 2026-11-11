// Power: why the current limits are what they are (every part of each limit, the lowest one
// in force), the last 24 hours, the voltages along the power path, and capacity and energy.
#include "ui_draw.h"

#include <algorithm>

namespace hmiui {

// One limit with its parts: the lowest part is the limit in force and is drawn bright
static int limit_block(Canvas &c, const Theme &t, int x, int y, int w, const char *title, float limit,
                       const LimitPart *parts, int count, Rgb color) {
  c.text(FONT_M20, x, y, title, t.text);
  const char *number = std::isnan(limit) ? "--" : (std::isinf(limit) ? "none" : fmt("%.*f", limit < 10 ? 1 : 0, (double) limit));
  num_unit(c, x + w, y + 24, FONT_M32, number, FONT_R20, std::isinf(limit) ? "" : "A", limit == 0.0f ? t.text_dim : color,
           t.caption, RIGHT);
  float top = 1.0f;
  for (int i = 0; i < count; i++) {
    if (!std::isinf(parts[i].amps) && !std::isnan(parts[i].amps)) top = std::max(top, parts[i].amps);
  }
  y += 42;
  const int bar_x = x + 210, bar_w = w - 210 - 78;
  for (int i = 0; i < count; i++) {
    const float a = parts[i].amps;
    const bool in_force = !std::isnan(limit) && a == limit;
    c.text(in_force ? FONT_M16 : FONT_R16, x, y, parts[i].name, in_force ? t.text : t.caption);
    c.round_rect(bar_x, y + 6, bar_w, 8, 4, t.track);
    if (!std::isnan(a)) {
      const int bw = std::isinf(a) ? bar_w : (int) (bar_w * a / top);
      if (bw >= 8) c.round_rect(bar_x, y + 6, bw, 8, 4, in_force ? color : (std::isinf(a) ? t.track : 0x56615C));
    }
    c.text(in_force ? FONT_M16 : FONT_R16, x + w, y, std::isnan(a) ? "--" : (std::isinf(a) ? "no limit" : fmt("%.1f A", (double) a)),
           in_force ? t.text : t.caption, RIGHT);
    y += 30;
  }
  return y;
}

// State of charge as a line and power as bars over the last 24 hours
static void history_chart(Canvas &c, const Theme &t, const Model &m, int x, int y, int w, int h) {
  const int gy = y + 4, gh = h - 26;
  c.fill(x, gy, w, 1, t.card_border);
  c.fill(x, gy + gh / 2, w, 1, t.card_border);
  c.fill(x, gy + gh, w, 1, t.card_border);
  c.text(FONT_R14, x, gy + gh + 5, "24 h ago", t.hint);
  c.text(FONT_R14, x + w / 2, gy + gh + 5, "12 h", t.hint, CENTER);
  c.text(FONT_R14, x + w, gy + gh + 5, "now", t.hint, RIGHT);
  if (m.history_count < 2) {
    c.text(FONT_R18, x + w / 2, gy + gh / 2 - 34, "No history yet", t.hint, CENTER);
    c.text(FONT_R14, x + w / 2, gy + gh / 2 + 12, "The display keeps the last 24 hours since it started.", t.hint, CENTER);
    return;
  }
  float peak = 1.0f;
  for (int i = 0; i < m.history_count; i++) {
    if (!std::isnan(m.history_kw[i])) peak = std::max(peak, std::fabs(m.history_kw[i]));
  }
  peak = std::ceil(peak);
  const int first = HISTORY_POINTS - m.history_count;  // the newest point sits at the right edge
  const float mid = gy + gh / 2.0f;
  for (int i = 0; i < m.history_count; i++) {
    const float kw = m.history_kw[i];
    if (std::isnan(kw)) continue;
    const int x0 = x + (first + i) * w / HISTORY_POINTS, x1 = x + (first + i + 1) * w / HISTORY_POINTS;
    const int bh = (int) (std::fabs(kw) / peak * (gh / 2 - 2));
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
    if (have) c.line(px, py, cx, cy, 2.4f, t.text);
    px = cx;
    py = cy;
    have = true;
  }
  // The scales, on a patch of the card's colour so they read over the bars
  auto scale = [&](int sx, int sy, const char *label, Align align) {
    const int tw = Canvas::text_width(FONT_R14, label);
    c.fill_blend(align == RIGHT ? sx - tw - 4 : sx - 4, sy, tw + 8, 18, t.card, 215);
    c.text(FONT_R14, sx, sy, label, t.caption, align);
  };
  scale(x + 6, gy + 4, fmt("+%.0f kW", (double) peak), LEFT);
  scale(x + 6, gy + gh - 21, fmt("−%.0f kW", (double) peak), LEFT);
  scale(x + w - 6, gy + 4, "100 %", RIGHT);
  scale(x + w - 6, gy + gh - 21, "0 %", RIGHT);
}

void draw_power(Canvas &c, const Theme &t, const Model &m) {
  top_line(c, t, m, "Power");
  const int lw = 440, h = Y1 - Y0;

  // ---- current limits ----
  card(c, t, X0, Y0, lw, h);
  c.text(FONT_R14, X0 + 16, Y0 + 12, "CURRENT LIMITS", t.caption);
  c.text(FONT_R14, X0 + lw - 16, Y0 + 12, "the lowest part is in force", t.hint, RIGHT);
  int y = limit_block(c, t, X0 + 16, Y0 + 44, lw - 32, "Charge", m.charge_limit, m.charge_parts, m.charge_part_count, t.good);
  c.fill(X0 + 16, y + 4, lw - 32, 1, t.card_border);
  limit_block(c, t, X0 + 16, y + 20, lw - 32, "Discharge", m.discharge_limit, m.discharge_parts, m.discharge_part_count,
              t.out);

  // ---- the last 24 hours ----
  const int rx = X0 + lw + 12, rw = X1 - rx;
  card(c, t, rx, Y0, rw, 236);
  c.text(FONT_R14, rx + 16, Y0 + 12, "LAST 24 HOURS", t.caption);
  {
    int x = rx + rw - 16;
    x -= c.text(FONT_R14, x, Y0 + 12, "discharging", t.caption, RIGHT) + 6;
    c.fill(x - 10, Y0 + 16, 10, 10, mix(t.card, t.out, 0.6f));
    x -= 26;
    x -= c.text(FONT_R14, x, Y0 + 12, "charging", t.caption, RIGHT) + 6;
    c.fill(x - 10, Y0 + 16, 10, 10, mix(t.card, t.good, 0.6f));
    x -= 26;
    x -= c.text(FONT_R14, x, Y0 + 12, "state of charge", t.caption, RIGHT) + 6;
    c.fill(x - 14, Y0 + 20, 14, 2, t.text);
  }
  history_chart(c, t, m, rx + 16, Y0 + 40, rw - 32, 184);

  // ---- voltages; capacity and energy ----
  const int by = Y0 + 248, bh = Y1 - by, bw = (rw - 12) / 2;
  card(c, t, rx, by, bw, bh);
  c.text(FONT_R14, rx + 16, by + 12, "VOLTAGES", t.caption);
  row(c, t, rx + 16, by + 44, bw - 32, "Pack", value(m.pack_voltage, "%.1f V"));
  row(c, t, rx + 16, by + 80, bw - 32, "Link", value(m.link_voltage, "%.1f V"));
  row(c, t, rx + 16, by + 116, bw - 32, "Output", value(m.output_voltage, "%.1f V"));
  row(c, t, rx + 16, by + 152, bw - 32, "Contactor +", value(m.contactor_pos_v, "%.1f V"));
  row(c, t, rx + 16, by + 188, bw - 32, "Contactor −", value(m.contactor_neg_v, "%.1f V"));
  const int cx = rx + bw + 12;
  card(c, t, cx, by, bw, bh);
  c.text(FONT_R14, cx + 16, by + 12, "CAPACITY AND ENERGY", t.caption);
  row(c, t, cx + 16, by + 44, bw - 32, "Nameplate", value(m.nameplate_ah, "%.0f Ah"));
  row(c, t, cx + 16, by + 80, bw - 32, "In use", value(m.capacity_ah, "%.0f Ah"));
  row(c, t, cx + 16, by + 116, bw - 32, "Learned", value(m.learned_ah, "%.1f Ah", "not yet"));
  row(c, t, cx + 16, by + 152, bw - 32, "Lifetime in", value(m.lifetime_in_kwh, "%.0f kWh"));
  row(c, t, cx + 16, by + 188, bw - 32, "Lifetime out", value(m.lifetime_out_kwh, "%.0f kWh"));

  page_end(c, t, m, Page::POWER);
}

}  // namespace hmiui
