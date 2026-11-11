#include "ui_draw.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace hmiui {

const char *fmt(const char *format, ...) {
  static char buf[8][128];
  static int next = 0;
  char *out = buf[next];
  next = (next + 1) % 8;
  va_list args;
  va_start(args, format);
  vsnprintf(out, sizeof(buf[0]), format, args);
  va_end(args);
  return out;
}

const char *value(float v, const char *format, const char *missing) {
  return std::isnan(v) ? missing : fmt(format, (double) v);
}

// ---- worked out from the model ----

Flow flow_of(const Model &m) {
  if (m.link != Link::OK || std::isnan(m.current)) return Flow::UNKNOWN;
  if (std::fabs(m.current) < 0.5f) return Flow::IDLE;
  return m.current > 0 ? Flow::CHARGING : Flow::DISCHARGING;
}

const char *flow_name(Flow f) {
  static const char *const NAMES[] = {"", "IDLE", "CHARGING", "DISCHARGING"};
  return NAMES[(int) f];
}

Rgb flow_color(const Theme &t, Flow f) {
  return f == Flow::CHARGING ? t.good : (f == Flow::DISCHARGING ? t.out : t.caption);
}

Rgb soc_color(const Theme &t, float soc) {
  if (std::isnan(soc)) return t.hint;
  if (soc < 10) return t.fatal;
  if (soc < 25) return t.warning;
  return t.good;
}

Rgb level_color(const Theme &t, Level level) {
  switch (level) {
    case Level::FATAL: return t.fatal;
    case Level::CRITICAL: return t.critical;
    case Level::WARNING: return t.warning;
    case Level::INFO: return t.info;
    default: return t.track;
  }
}

Rgb level_text_color(Level level) { return level == Level::WARNING ? 0x000000 : 0xFFFFFF; }

const char *level_name(Level level) {
  static const char *const NAMES[] = {"", "INFO", "WARNING", "CRITICAL", "FATAL"};
  return NAMES[(int) level];
}

const char *system_state_name(SystemState s) {
  static const char *const NAMES[] = {"Not started", "Starting", "Calibrating", "Inactive", "Operating", "Fault", "--"};
  return NAMES[(int) s <= (int) SystemState::UNKNOWN ? (int) s : (int) SystemState::UNKNOWN];
}

NumberUnit power_text(float watts) {
  if (std::isnan(watts)) return {"--", "W"};
  const double a = std::fabs((double) watts);
  if (a >= 9995.0) return {fmt("%.1f", a / 1000.0), "kW"};
  if (a >= 1000.0) return {fmt("%.2f", a / 1000.0), "kW"};
  return {fmt("%.0f", a), "W"};
}

float ah_left(const Model &m) { return m.soc / 100.0f * m.capacity_ah; }
float kwh_left(const Model &m) { return ah_left(m) * m.pack_voltage / 1000.0f; }

const char *time_to_text(const Model &m) {
  const Flow f = flow_of(m);
  if (std::isnan(m.soc) || std::isnan(m.capacity_ah) || (f != Flow::CHARGING && f != Flow::DISCHARGING)) return "";
  const float ah = (f == Flow::CHARGING ? (100.0f - m.soc) : m.soc) / 100.0f * m.capacity_ah;
  const float hours = ah / std::fabs(m.current);
  if (hours > 99.0f) return "";
  const int minutes = (int) std::lround(hours * 12.0f) * 5;
  const char *word = f == Flow::CHARGING ? "Full" : "Empty";
  if (minutes < 60) return fmt("%s in ≈ %d min", word, minutes);
  return fmt("%s in ≈ %d h %02d min", word, minutes / 60, minutes % 60);
}

CellStats cell_stats(const Model &m) {
  CellStats s;
  for (int i = 0; i < MAX_CELLS; i++) {
    const float mv = m.cell_mv[i];
    if (std::isnan(mv)) continue;
    s.count++;
    if (s.lowest < 0 || mv < s.low_mv) {
      s.lowest = i;
      s.low_mv = mv;
    }
    if (s.highest < 0 || mv > s.high_mv) {
      s.highest = i;
      s.high_mv = mv;
    }
  }
  return s;
}

int top_events(const Model &m, const Event *out[], int max, Level at_least) {
  int n = 0;
  for (int level = (int) Level::FATAL; level >= (int) at_least && level > (int) Level::INACTIVE; level--) {
    for (int i = 0; i < m.event_count && n < max; i++) {
      if ((int) m.events[i].level == level) out[n++] = &m.events[i];
    }
  }
  return n;
}

// ---- drawing ----

int text_base(Canvas &c, const Font &font, int x, int baseline, const char *s, Rgb color, Align align) {
  return c.text(font, x, baseline - font.ascent, s, color, align);
}

int num_unit(Canvas &c, int x, int baseline, const Font &big, const char *number, const Font &small, const char *unit,
             Rgb number_color, Rgb unit_color, Align align, int gap) {
  const int wn = Canvas::text_width(big, number), wu = Canvas::text_width(small, unit);
  const int total = wn + (wu > 0 ? gap + wu : 0);
  const int x0 = align == LEFT ? x : (align == CENTER ? x - total / 2 : x - total);
  text_base(c, big, x0, baseline, number, number_color);
  if (wu > 0) text_base(c, small, x0 + wn + gap, baseline, unit, unit_color);
  return total;
}

int text_wrap(Canvas &c, const Font &font, int x, int y, int width, const char *s, Rgb color, int max_lines) {
  char line[160];
  size_t len = 0;
  int lines = 0;
  const char *p = s;
  while (*p && lines < max_lines) {
    // The next word, with the space before it
    const char *end = p;
    while (*end == ' ') end++;
    while (*end && *end != ' ') end++;
    const size_t word = (size_t) (end - p);
    if (len + word >= sizeof(line)) break;
    memcpy(line + len, p, word);
    line[len + word] = 0;
    if (len > 0 && Canvas::text_width(font, line) > width) {
      line[len] = 0;
      c.text(font, x, y, line, color);
      y += font.line_height();
      lines++;
      len = 0;
      while (*p == ' ') p++;
      continue;
    }
    len += word;
    p = end;
  }
  if (len > 0 && lines < max_lines) {
    line[len] = 0;
    c.text(font, x, y, line, color);
    y += font.line_height();
  }
  return y;
}

void card(Canvas &c, const Theme &t, int x, int y, int w, int h) {
  c.round_rect(x, y, w, h, 10, t.card);
  c.round_frame(x, y, w, h, 10, 1, t.card_border);
}

static const int PILL_H = 34, PILL_PAD = 14, PILL_DOT = 16;

int pill_width(const char *label, bool dot) {
  return Canvas::text_width(FONT_M16, label) + 2 * PILL_PAD + (dot ? PILL_DOT : 0);
}

int pill(Canvas &c, int x, int y, const char *label, Rgb fg, Rgb bg, Align align, bool dot) {
  const int w = pill_width(label, dot);
  const int x0 = align == RIGHT ? x - w : x;
  c.round_rect(x0, y, w, PILL_H, PILL_H / 2, bg);
  int tx = x0 + PILL_PAD;
  if (dot) {
    c.disc(tx + 5.0f, y + PILL_H / 2.0f, 5.0f, fg);
    tx += PILL_DOT;
  }
  c.text(FONT_M16, tx, y + (PILL_H - FONT_M16.line_height()) / 2, label, fg);
  return w;
}

void warning_sign(Canvas &c, float cx, float cy, float size, Rgb color, Rgb mark) {
  const float half = size * 0.58f, top = cy - size / 2.0f, bottom = cy + size / 2.0f;
  c.triangle(cx, top, cx - half, bottom, cx + half, bottom, color);
  c.line(cx, top + size * 0.36f, cx, top + size * 0.66f, size * 0.11f, mark);
  c.disc(cx, top + size * 0.82f, size * 0.065f, mark);
}

void cell_skyline(Canvas &c, const Theme &t, const Model &m, int x, int y, int w, int h) {
  const CellStats s = cell_stats(m);
  if (s.count == 0) {
    c.text(FONT_R18, x + w / 2, y + h / 2 - 12, "No cell readings", t.hint, CENTER);
    return;
  }
  // The lowest cell stands at about half height, so a few millivolts of spread are visible
  const float span = std::max(s.high_mv - s.low_mv, 10.0f);
  const float floor_mv = s.low_mv - span, ceil_mv = s.high_mv + span * 0.15f;
  int last = 0;
  for (int i = 0; i < MAX_CELLS; i++) {
    if (!std::isnan(m.cell_mv[i])) last = i;
  }
  const int n = last + 1;
  for (int i = 0; i < n; i++) {
    const float mv = m.cell_mv[i];
    if (std::isnan(mv)) continue;
    const int x0 = x + i * w / n, x1 = x + (i + 1) * w / n;
    const int bw = std::max(1, x1 - x0 - 1);
    const int bh = std::max(2, (int) ((mv - floor_mv) / (ceil_mv - floor_mv) * h));
    const Rgb color = i == s.lowest ? t.cell_low : (i == s.highest ? t.cell_high : t.cell);
    c.fill(x0, y + h - bh, bw, bh, color);
  }
}

void no_link_overlay(Canvas &c, const Theme &t, const Model &m, int x, int y, int w, int h) {
  c.fill_blend(x, y, w, h, t.background, 190);
  const int bw = 620, bh = 196, bx = x + (w - bw) / 2, by = y + (h - bh) / 2;
  c.round_rect(bx, by, bw, bh, 14, t.card);
  c.round_frame(bx, by, bw, bh, 14, 2, t.warning);
  warning_sign(c, bx + 62.0f, by + 62.0f, 52.0f, t.warning, 0x000000);
  const bool wrong = m.link == Link::WRONG_PROTOCOL;
  c.text(FONT_M32, bx + 112, by + 28, wrong ? "This BMS speaks another protocol" : "No answer from the BMS", t.text);
  const char *help = wrong ? "The BMS firmware and this display don't match. Update one of them so both speak protocol 2."
                           : "The display hears nothing on the BMS link. Check that the BMS has power and that the "
                             "cable to the display is plugged in.";
  const int y1 = text_wrap(c, FONT_R20, bx + 112, by + 76, bw - 140, help, t.text_dim, 3);
  if (!wrong && m.link_silent_s > 0) {
    c.text(FONT_R18, bx + 112, y1 + 10, fmt("Silent for %u s. Still trying.", (unsigned) m.link_silent_s), t.caption);
  }
}

// ---- navigation ----

static const char *const TABS[] = {"Home", "Control", "Power", "Cells", "Health", "Events", "Setup"};
static const int TAB_COUNT = (int) Page::COUNT;

// The icons of the rail, each drawn in a box of about 28 px around (cx, cy)
static void tab_icon(Canvas &c, Page tab, float cx, float cy, Rgb color) {
  const float w = 2.6f;
  switch (tab) {
    case Page::HOME:  // a house
      c.line(cx - 13, cy - 1, cx, cy - 12, w, color);
      c.line(cx, cy - 12, cx + 13, cy - 1, w, color);
      c.line(cx - 9, cy - 3, cx - 9, cy + 11, w, color);
      c.line(cx + 9, cy - 3, cx + 9, cy + 11, w, color);
      c.line(cx - 9, cy + 11, cx + 9, cy + 11, w, color);
      break;
    case Page::CELLS:  // cell bars
      c.round_rect((int) cx - 13, (int) cy - 3, 5, 15, 2, color);
      c.round_rect((int) cx - 6, (int) cy - 11, 5, 23, 2, color);
      c.round_rect((int) cx + 1, (int) cy - 6, 5, 18, 2, color);
      c.round_rect((int) cx + 8, (int) cy - 9, 5, 21, 2, color);
      break;
    case Page::POWER:  // a bolt
      c.triangle(cx + 4, cy - 14, cx - 9, cy + 3, cx + 2, cy + 3, color);
      c.triangle(cx - 4, cy + 14, cx + 9, cy - 3, cx - 2, cy - 3, color);
      break;
    case Page::HEALTH:  // a pulse line
      c.line(cx - 14, cy, cx - 7, cy, w, color);
      c.line(cx - 7, cy, cx - 3, cy - 10, w, color);
      c.line(cx - 3, cy - 10, cx + 3, cy + 10, w, color);
      c.line(cx + 3, cy + 10, cx + 7, cy, w, color);
      c.line(cx + 7, cy, cx + 14, cy, w, color);
      break;
    case Page::EVENTS:  // a warning triangle, outlined
      c.line(cx, cy - 12, cx - 13, cy + 10, w, color);
      c.line(cx - 13, cy + 10, cx + 13, cy + 10, w, color);
      c.line(cx + 13, cy + 10, cx, cy - 12, w, color);
      c.line(cx, cy - 4, cx, cy + 3, w, color);
      c.disc(cx, cy + 6.5f, 1.6f, color);
      break;
    case Page::CONTROL:  // the power symbol
      c.ring(cx, cy + 1, 8.5f, 11.0f, 305, 595, color, true);
      c.line(cx, cy - 13, cx, cy - 2, w, color);
      break;
    default:  // sliders
      for (int i = 0; i < 3; i++) {
        const float y = cy - 8 + i * 8;
        c.line(cx - 12, y, cx + 12, y, 2.0f, color);
        c.disc(cx + (i == 0 ? 5.0f : (i == 1 ? -5.0f : 2.0f)), y, 3.6f, color);
      }
      break;
  }
}

// The count of active warnings and worse, as a small badge. Returns its width (0 = none).
static int events_badge(Canvas &c, const Theme &t, const Model &m, int x, int y, bool draw) {
  const Event *list[MAX_EVENTS];
  const int n = top_events(m, list, MAX_EVENTS, Level::WARNING);
  if (n == 0) return 0;
  const char *label = fmt("%d", n);
  const int w = std::max(24, Canvas::text_width(FONT_M16, label) + 14);
  if (draw) {
    const Level worst = m.worst_level();
    c.round_rect(x, y, w, 24, 12, level_color(t, worst));
    c.text(FONT_M16, x + w / 2, y + 2, label, level_text_color(worst), CENTER);
  }
  return w;
}

void nav_rail(Frame &f, Page selected) {
  Canvas &c = f.c;
  const Theme &t = f.t;
  const Model &m = f.m;
  c.fill(0, 0, NAV_RAIL_W, SCREEN_H, t.nav);
  c.fill(NAV_RAIL_W - 1, 0, 1, SCREEN_H, t.card_border);
  const int step = 82, top = (SCREEN_H - TAB_COUNT * step) / 2;
  for (int i = 0; i < TAB_COUNT; i++) {
    const int y = top + i * step;
    const bool on = i == (int) selected;
    if (hot(f, 0, y, NAV_RAIL_W, step, TABS[i]) && !on) {
      f.s.page = (Page) i;
      // A page opens on its first view
      f.s.events_view = 0;
      f.s.raw_registers = false;
    }
    if (on) {
      c.round_rect(7, y + 3, NAV_RAIL_W - 15, step - 6, 12, t.nav_active);
    } else if (f.held(0, y, NAV_RAIL_W, step)) {
      c.round_rect(7, y + 3, NAV_RAIL_W - 15, step - 6, 12, 0x171B1D);
    }
    const Rgb color = on ? t.good : t.text_dim;
    tab_icon(c, (Page) i, NAV_RAIL_W / 2.0f - 0.5f, y + 31.0f, color);
    c.text(on ? FONT_M16 : FONT_R16, NAV_RAIL_W / 2, y + 51, TABS[i], color, CENTER);
    if (i == (int) Page::EVENTS) events_badge(c, t, m, NAV_RAIL_W / 2 + 8, y + 8, true);
  }
}

void padlock(Canvas &c, float cx, float cy, bool open, Rgb color) {
  const float dx = open ? 9.0f : 0.0f;
  c.ring(cx + dx, cy - 5, 5.5f, 8.5f, 180, 360, color);
  c.line(cx - 7 + dx, cy - 5, cx - 7 + dx, cy, 3.0f, color);
  if (!open) c.line(cx + 7, cy - 5, cx + 7, cy, 3.0f, color);
  c.round_rect((int) cx - 11, (int) cy - 1, 22, 17, 4, color);
}

void scrollbar(Canvas &c, const Theme &t, int x, int y, int h, float start, float shown) {
  c.round_rect(x, y, 4, h, 2, t.track);
  c.round_rect(x, y + (int) (h * start), 4, std::max(12, (int) (h * shown)), 2, t.caption);
}

void top_line(Frame &f, const char *title, const char *subtitle) {
  Canvas &c = f.c;
  const Theme &t = f.t;
  const Model &m = f.m;
  const bool linked = m.link == Link::OK;
  const Event *list[MAX_EVENTS];
  const int alarms = top_events(m, list, MAX_EVENTS, Level::WARNING);
  if (alarms > 0) {
    const Level worst = list[0]->level;
    const Rgb on = level_text_color(worst);
    // The alarm strip leads to the events
    if (hot(f, X0, 12, X1 - X0, 44, "Alarm") && f.s.page != Page::EVENTS) {
      f.s.page = Page::EVENTS;
      f.s.events_view = 0;
    }
    c.round_rect(X0, 12, X1 - X0, 44, 10, level_color(t, worst));
    warning_sign(c, X0 + 30.0f, 34.0f, 26.0f, on, level_color(t, worst));
    int x = X0 + 56;
    x += c.text(FONT_B20, x, 22, level_name(worst), on) + 14;
    c.text(FONT_R20, x, 22, event_title(list[0]->type), on);
    c.text(FONT_R20, X1 - 18, 22, alarms > 1 ? fmt("+%d more  ›", alarms - 1) : "Events  ›", on, RIGHT);
    return;
  }
  int x = X0 + 4;
  x += c.text(FONT_M24, x, 18, title, t.text) + 12;
  c.text(FONT_R18, x, 24, subtitle, t.caption);
  int rx = X1;
  if (m.clock[0]) rx -= c.text(FONT_R20, rx, 22, m.clock, t.text_dim, RIGHT) + 16;
  if (linked) {
    const bool run = m.system_state == SystemState::OPERATING, fault = m.system_state == SystemState::FAULT;
    pill(c, rx, 15, system_state_name(m.system_state), fault ? 0xFFFFFF : (run ? t.good : t.text_dim),
         fault ? t.fatal : (run ? mix(t.background, t.good, 0.16f) : t.track), RIGHT, true);
  } else {
    pill(c, rx, 15, m.link == Link::CONNECTING ? "Connecting" : (m.link == Link::WRONG_PROTOCOL ? "Old BMS protocol" : "No BMS link"),
         0x000000, t.warning, RIGHT, true);
  }
}

void page_end(Frame &f, Page selected, bool needs_bms) {
  Canvas &c = f.c;
  const Theme &t = f.t;
  const Model &m = f.m;
  if (needs_bms && m.link != Link::OK) no_link_overlay(c, t, m, NAV_RAIL_W, 62, SCREEN_W - NAV_RAIL_W, SCREEN_H - 62);
  nav_rail(f, selected);
}

void stat(Canvas &c, const Theme &t, int x, int y, const char *caption, const char *number, const char *unit, Rgb color) {
  c.text(FONT_R14, x, y, caption, t.caption);
  num_unit(c, x, y + 46, FONT_M24, number, FONT_R18, unit, color, t.caption);
}

void row(Canvas &c, const Theme &t, int x, int y, int w, const char *label, const char *val, Rgb color) {
  c.text(FONT_R18, x, y, label, t.text_dim);
  c.text(FONT_M20, x + w, y - 1, val, color ? color : t.text, RIGHT);
}

}  // namespace hmiui
