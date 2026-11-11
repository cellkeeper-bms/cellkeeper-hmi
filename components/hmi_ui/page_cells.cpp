// Cells: every cell as a bar on a scale zoomed to the spread between them, which cells are
// being balanced, and each module's temperature under its cells.
#include "ui_draw.h"

#include <algorithm>

namespace hmiui {

static const char *minutes_seconds(uint32_t s) { return fmt("%u:%02u", (unsigned) (s / 60), (unsigned) (s % 60)); }

void draw_cells(Canvas &c, const Theme &t, const Model &m) {
  const CellStats s = cell_stats(m);
  top_line(c, t, m, "Cells", s.count > 0 ? fmt("%d in series", s.count) : "");
  const int w = X1 - X0;
  int balancing = 0, queued = 0;
  for (int i = 0; i < MAX_CELLS; i++) {
    if (m.balancing_now[i]) {
      balancing++;
    } else if (m.balance_s[i] > 0) {
      queued++;
    }
  }

  // ---- the numbers ----
  card(c, t, X0, Y0, w, 78);
  if (s.count > 0) {
    const float spread = s.high_mv - s.low_mv;
    c.disc(X0 + 21.0f, Y0 + 21.0f, 4.5f, t.cell_low);
    stat(c, t, X0 + 32, Y0 + 12, fmt("LOWEST  ·  #%d", s.lowest + 1), fmt("%.3f", (double) s.low_mv / 1000.0), "V", t.text);
    c.disc(X0 + 189.0f, Y0 + 21.0f, 4.5f, t.cell_high);
    stat(c, t, X0 + 200, Y0 + 12, fmt("HIGHEST  ·  #%d", s.highest + 1), fmt("%.3f", (double) s.high_mv / 1000.0), "V",
         t.text);
    stat(c, t, X0 + 362, Y0 + 12, "SPREAD", fmt("%.0f", (double) spread), "mV", spread > 50 ? t.warning : t.good);
    stat(c, t, X0 + 472, Y0 + 12, "AVERAGE", value(m.cell_avg, "%.3f"), "V", t.text);
    stat(c, t, X0 + 622, Y0 + 12, "BALANCING", balancing + queued > 0 ? fmt("%d", balancing) : "off",
         balancing + queued > 0 ? "cells now" : "", balancing > 0 ? t.balance : t.text_dim);
    if (balancing + queued > 0) {
      c.text(FONT_R14, X1 - 16, Y0 + 20, fmt("%d waiting", queued), t.caption, RIGHT);
      c.text(FONT_R14, X1 - 16, Y0 + 42, fmt("cycle ends in %s", minutes_seconds(m.balance_cycle_left_s)), t.caption, RIGHT);
    }
  } else {
    c.text(FONT_R18, X0 + 16, Y0 + 28, "No cell readings", t.hint);
  }

  // ---- the chart ----
  const int cy = Y0 + 90, ch = Y1 - cy;
  card(c, t, X0, cy, w, ch);
  if (s.count == 0) {
    c.text(FONT_R24, X0 + w / 2, cy + ch / 2 - 30, "No cell readings", t.hint, CENTER);
    c.text(FONT_R18, X0 + w / 2, cy + ch / 2 + 6, "The cell boards are not answering. See Events for the cause.", t.hint,
           CENTER);
    page_end(c, t, m, 1);
    return;
  }
  const int px = X0 + 66, pw = X1 - 16 - px, py = cy + 20, ph = ch - 20 - 108;
  const float span = std::max(s.high_mv - s.low_mv, 10.0f);
  const float lo = s.low_mv - span * 0.9f, hi = s.high_mv + span * 0.3f;
  auto y_of = [&](float mv) { return py + ph - (mv - lo) / (hi - lo) * ph; };
  for (int i = 0; i <= 3; i++) {
    const float mv = lo + (hi - lo) * i / 3.0f;
    const int y = (int) y_of(mv);
    c.fill(px, y, pw, 1, i == 0 ? t.caption : t.card_border);
    c.text(FONT_R14, px - 10, y - 9, fmt("%.3f", (double) mv / 1000.0), t.caption, RIGHT);
  }
  c.text(FONT_R14, px - 10, py + ph + 10, "V", t.hint, RIGHT);
  int last = 0;
  for (int i = 0; i < MAX_CELLS; i++) {
    if (!std::isnan(m.cell_mv[i])) last = i;
  }
  const int n = last + 1;
  // Modules: a gap line between them when the cells divide evenly, and each one's temperature
  const int mods = std::min(m.module_count, MAX_MODULES);
  const bool even = mods > 1 && n % mods == 0;
  for (int k = 0; k < mods; k++) {
    const int x0 = px + k * pw / mods, x1 = px + (k + 1) * pw / mods;
    if (even && k > 0) c.fill(x0 - 1, py - 6, 1, ph + 34, t.track);
    if (even) c.text(FONT_R14, x0 + 3, py + ph + 6, fmt("%d", k * (n / mods) + 1), t.hint);
    const int mx = (x0 + x1) / 2;
    c.text(FONT_R14, mx, py + ph + 34, fmt("MODULE %d", k + 1), t.caption, CENTER);
    num_unit(c, mx, py + ph + 76, FONT_M24, value(m.module_temp[k], "%.1f"), FONT_R18, "°C", t.text, t.caption, CENTER);
  }
  c.text(FONT_R14, px + pw, py + ph + 6, fmt("%d", n), t.hint, RIGHT);
  for (int i = 0; i < n; i++) {
    const float mv = m.cell_mv[i];
    if (std::isnan(mv)) continue;
    const int x0 = px + i * pw / n, x1 = px + (i + 1) * pw / n;
    const int bw = std::max(2, x1 - x0 - 2), top = (int) y_of(mv);
    c.fill(x0 + 1, top, bw, py + ph - top, i == s.lowest ? t.cell_low : (i == s.highest ? t.cell_high : t.cell));
    if (m.balancing_now[i]) {
      c.fill(x0 + 1, top - 10, bw, 6, t.balance);
    } else if (m.balance_s[i] > 0) {
      c.fill(x0 + 1, top - 10, bw, 6, mix(t.card, t.balance, 0.4f));
    }
  }
  if (!std::isnan(m.cell_avg)) {
    const int y = (int) y_of(m.cell_avg * 1000.0f);
    for (int x = px; x < px + pw; x += 10) c.fill(x, y, 5, 1, t.text);
  }
  // What the marks mean
  int x = px;
  const int ly = cy + ch - 28;
  auto key = [&](Rgb color, const char *label, bool dashed) {
    if (dashed) {
      for (int i = 0; i < 3; i++) c.fill(x + i * 7, ly + 9, 4, 1, color);
      x += 26;
    } else {
      c.fill(x, ly + 4, 10, 10, color);
      x += 16;
    }
    x += c.text(FONT_R14, x, ly, label, t.caption) + 22;
  };
  key(t.cell, "cell", false);
  key(t.cell_low, "lowest", false);
  key(t.cell_high, "highest", false);
  key(t.text, fmt("average %.3f V", (double) m.cell_avg), true);
  key(t.balance, "balancing now", false);
  key(mix(t.card, t.balance, 0.4f), "waiting to balance", false);

  page_end(c, t, m, 1);
}

}  // namespace hmiui
