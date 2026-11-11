// Events: what is active now, most severe first, each with one line on what it means; and
// what happened earlier and has cleared.
#include "ui_draw.h"

#include <algorithm>

namespace hmiui {

// Every event type the firmware has, in its order: which are active, which happened earlier
static void all_types(Canvas &c, const Theme &t, const Model &m) {
  const int w = X1 - X0, h = Y1 - Y0, rows = 20, colw = (w - 32) / 3;
  card(c, t, X0, Y0, w, h);
  c.text(FONT_R14, X0 + 16, Y0 + 16, fmt("ALL EVENT TYPES  ·  %d", event_type_count()), t.caption);
  int lx = X0 + 250;
  c.disc(lx, Y0 + 25.0f, 5.0f, t.fatal);
  lx += 12 + c.text(FONT_R14, lx + 10, Y0 + 16, "active, in its level's colour", t.caption) + 24;
  c.disc(lx, Y0 + 25.0f, 5.0f, t.caption);
  lx += 12 + c.text(FONT_R14, lx + 10, Y0 + 16, "happened earlier", t.caption) + 24;
  c.ring(lx, Y0 + 25.0f, 3.5f, 5.0f, 0, 360, t.track);
  c.text(FONT_R14, lx + 10, Y0 + 16, "never", t.caption);
  button(c, t, X1 - 16 - 150, Y0 + 6, 150, 40, "‹  Active");
  for (int i = 0; i < event_type_count(); i++) {
    const Event *e = nullptr;
    for (int k = 0; k < m.event_count; k++) {
      if (m.events[k].type == i) e = &m.events[k];
    }
    const int x = X0 + 16 + (i / rows) * colw, y = Y0 + 56 + (i % rows) * 23;
    const bool active = e != nullptr && e->level != Level::INACTIVE, earlier = e != nullptr && !active && e->count > 0;
    if (active) {
      c.disc(x + 6.0f, y + 9.0f, 5.0f, level_color(t, e->level));
    } else if (earlier) {
      c.disc(x + 6.0f, y + 9.0f, 5.0f, t.caption);
    } else {
      c.ring(x + 6.0f, y + 9.0f, 3.5f, 5.0f, 0, 360, t.track);
    }
    c.text(active ? FONT_M16 : FONT_R14, x + 20, active ? y - 1 : y, event_title((uint8_t) i),
           active ? t.text : (earlier ? t.text_dim : t.hint));
    if (e != nullptr && e->count > 1) c.text(FONT_R14, x + colw - 14, y, fmt("×%u", (unsigned) e->count), t.caption, RIGHT);
  }
}

void draw_events(Canvas &c, const Theme &t, const Model &m, bool all_types_view) {
  top_line(c, t, m, "Events");
  if (all_types_view) {
    all_types(c, t, m);
    page_end(c, t, m, Page::EVENTS);
    return;
  }
  const Event *list[MAX_EVENTS];
  const int n = top_events(m, list, MAX_EVENTS, Level::INFO);
  const int lw = 592, h = Y1 - Y0;

  // ---- active now ----
  card(c, t, X0, Y0, lw, h);
  c.text(FONT_R14, X0 + 16, Y0 + 12, fmt("ACTIVE NOW  ·  %d", n), t.caption);
  if (n == 0) {
    const float cx = X0 + lw / 2.0f, cy = Y0 + h / 2.0f - 30;
    c.ring(cx, cy, 30, 34, 0, 360, t.good);
    c.line(cx - 14, cy + 1, cx - 4, cy + 11, 4.0f, t.good);
    c.line(cx - 4, cy + 11, cx + 15, cy - 10, 4.0f, t.good);
    c.text(FONT_R24, (int) cx, (int) cy + 52, "Nothing is active", t.text_dim, CENTER);
  }
  int y = Y0 + 42, shown = 0;
  for (; shown < n; shown++) {
    const Event &e = *list[shown];
    if (y + 62 > Y1 - 12) break;
    c.text(FONT_M20, X0 + 36, y, event_title(e.type), t.text);
    const Rgb color = level_color(t, e.level);
    const char *level = level_name(e.level);
    const int tw = Canvas::text_width(FONT_M16, level) + 20;
    c.round_rect(X0 + lw - 16 - tw, y, tw, 24, 12, e.level == Level::INFO ? t.track : color);
    c.text(FONT_M16, X0 + lw - 16 - tw / 2, y + 2, level, e.level == Level::INFO ? t.text_dim : level_text_color(e.level),
           CENTER);
    if (e.count > 1) c.text(FONT_R16, X0 + lw - 28 - tw, y + 3, fmt("×%u", (unsigned) e.count), t.caption, RIGHT);
    const int y1 = text_wrap(c, FONT_R16, X0 + 36, y + 27, lw - 60, event_help(e.type), t.text_dim, 2);
    c.round_rect(X0 + 16, y + 2, 6, y1 - y - 2, 3, color);
    y = y1 + 14;
  }
  if (shown < n) c.text(FONT_R14, X0 + lw - 16, Y0 + 12, fmt("%d more below  ↓", n - shown), t.out, RIGHT);

  // ---- earlier ----
  const int rx = X0 + lw + 12, rw = X1 - rx;
  card(c, t, rx, Y0, rw, h);
  c.text(FONT_R14, rx + 16, Y0 + 12, "EARLIER, NOT ACTIVE NOW", t.caption);
  y = Y0 + 44;
  int earlier = 0;
  for (int i = 0; i < m.event_count; i++) {
    const Event &e = m.events[i];
    if (e.level != Level::INACTIVE || e.count == 0) continue;
    earlier++;
    if (y + 34 > Y1 - 84) continue;
    c.text(FONT_R18, rx + 16, y, event_title(e.type), t.text_dim);
    c.text(FONT_M16, rx + rw - 16, y + 1, fmt("×%u", (unsigned) e.count), t.caption, RIGHT);
    y += 34;
  }
  if (earlier == 0) c.text(FONT_R18, rx + 16, y, "Nothing since the BMS started", t.hint);
  c.text(FONT_R14, rx + 16, Y1 - 92, "Counts are since the BMS started.", t.hint);
  button(c, t, rx + 16, Y1 - 68, rw - 32, 52, "All event types");

  page_end(c, t, m, Page::EVENTS);
}

}  // namespace hmiui
