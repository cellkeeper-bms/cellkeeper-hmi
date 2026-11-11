#include "ui.h"

#include "ui_fonts.h"
#include "../hmi_bms/hmi_map.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace hmiui {

// printf into a small static buffer, for one drawing call at a time
static const char *fmt(const char *format, ...) __attribute__((format(printf, 1, 2)));
static const char *fmt(const char *format, ...) {
  static char buf[4][96];
  static int next = 0;
  char *out = buf[next];
  next = (next + 1) % 4;
  va_list args;
  va_start(args, format);
  vsnprintf(out, sizeof(buf[0]), format, args);
  va_end(args);
  return out;
}

// A value with its format, or the placeholder when it isn't known
static const char *value(float v, const char *format, const char *missing = "--") {
  return std::isnan(v) ? missing : fmt(format, (double) v);
}

static const char *system_state_name(SystemState s) {
  static const char *const NAMES[] = {"UNINIT", "INIT", "CALIB", "INACTIVE", "OPERATING", "FAULT", "--"};
  return NAMES[(int) s <= (int) SystemState::UNKNOWN ? (int) s : (int) SystemState::UNKNOWN];
}

static const char *level_name(Level level) {
  static const char *const NAMES[] = {"", "INFO", "WARNING", "CRITICAL", "FATAL"};
  return NAMES[(int) level];
}

const char *event_title(uint8_t type) {
  if (type >= esphome::hmi_bms::HMI_EVENT_COUNT) return "";
  // "CELL_VOLTAGES_STALE" -> "Cell Voltages Stale"
  static char buf[64];
  const char *name = esphome::hmi_bms::HMI_EVENT_NAMES[type];
  size_t i = 0;
  bool start = true;
  for (; name[i] && i < sizeof(buf) - 1; i++) {
    const char ch = name[i];
    if (ch == '_') {
      buf[i] = ' ';
      start = true;
    } else {
      buf[i] = (start || (ch >= '0' && ch <= '9')) ? ch : (char) (ch | 0x20);
      start = false;
    }
  }
  buf[i] = 0;
  return buf;
}

Rgb Ui::level_color_(Level level) const {
  switch (level) {
    case Level::FATAL: return theme.fatal;
    case Level::CRITICAL: return theme.critical;
    case Level::WARNING: return theme.warning;
    case Level::INFO: return theme.info;
    default: return theme.card;
  }
}

void Ui::card_(Canvas &c, int x, int y, int w, int h) {
  c.round_rect(x, y, w, h, 8, theme.card);
  c.round_frame(x, y, w, h, 8, 1, theme.card_border);
}

void Ui::render(Canvas &c, const Model &m, uint32_t now_ms) {
  (void) now_ms;
  c.reset_clip();
  c.clear(theme.background);
  switch (page) {
    case Page::MAIN: this->draw_main_(c, m); break;
    default: this->draw_placeholder_(c, "This page is not drawn yet"); break;
  }
  this->draw_tab_bar_(c, m);
}

void Ui::draw_placeholder_(Canvas &c, const char *title) {
  c.text(FONT_R24, SCREEN_W / 2, 260, title, theme.caption, CENTER);
}

void Ui::draw_tab_bar_(Canvas &c, const Model &m) {
  static const char *const TABS[] = {"Main", "Control", "Events", "Cells", "Power", "Health", "Config", "Log", "System"};
  const int n = (int) Page::COUNT;
  const int y = SCREEN_H - 34;
  c.fill(0, y, SCREEN_W, 34, theme.tab_bar);
  const int active = m.active_events();
  for (int i = 0; i < n; i++) {
    const int x0 = i * SCREEN_W / n, x1 = (i + 1) * SCREEN_W / n;
    const bool selected = (int) page == i;
    const bool alarm = i == (int) Page::EVENTS && m.worst_level() >= Level::WARNING;
    Rgb text = selected ? theme.accent : 0xA9B4AF;
    if (alarm) {
      c.fill(x0, y, x1 - x0, 34, level_color_(m.worst_level()));
      text = m.worst_level() == Level::WARNING ? 0x000000 : 0xFFFFFF;
    } else if (selected) {
      c.fill(x0, y, x1 - x0, 34, theme.tab_active);
      c.fill(x0, y, x1 - x0, 2, theme.accent);
    }
    const char *label = (i == (int) Page::EVENTS && active > 0) ? fmt("Events %d", active) : TABS[i];
    c.text(FONT_R18, (x0 + x1) / 2, y + 7, label, text, CENTER);
  }
}

// The main page, laid out as the LVGL "Main v2" page is today
void Ui::draw_main_(Canvas &c, const Model &m) {
  // Banner: the most severe active event
  const Level worst = m.worst_level();
  if (worst > Level::INACTIVE) {
    const Event *top = nullptr;
    for (int i = 0; i < m.event_count; i++) {
      if (m.events[i].level == worst && top == nullptr) top = &m.events[i];
    }
    const Rgb on = worst == Level::WARNING ? 0x000000 : 0xFFFFFF;
    c.round_rect(16, 8, 992, 44, 8, level_color_(worst));
    const int more = m.active_events() - 1;
    const char *line = more > 0 ? fmt("%s  %s   (+%d more)", level_name(worst), event_title(top->type), more)
                                : fmt("%s  %s", level_name(worst), event_title(top->type));
    c.text(FONT_R24, 44, 16, line, on);
    c.text(FONT_R20, 980, 19, "Events >", on, RIGHT);
  }

  // State of charge
  this->card_(c, 16, 60, 300, 272);
  const float cx = 166, cy = 190, r_out = 112, r_in = 98;
  c.ring(cx, cy, r_in, r_out, 135, 405, theme.track, true);
  if (!std::isnan(m.soc)) {
    const float soc = m.soc < 0 ? 0 : (m.soc > 100 ? 100 : m.soc);
    if (soc > 0.5f) c.ring(cx, cy, r_in, r_out, 135, 135 + 2.7f * soc, theme.accent, true);
  }
  {
    // The number in the big font, the % sign smaller beside it, centred together
    const char *num = value(m.soc, "%.0f");
    const int wn = Canvas::text_width(FONT_M72, num), wp = Canvas::text_width(FONT_R32, "%");
    const int x0 = (int) cx - (wn + wp) / 2;
    c.text(FONT_M72, x0, 148, num, theme.text);
    c.text(FONT_R32, x0 + wn, 180, "%", theme.text);
  }
  c.text(FONT_R16, (int) cx, 230, "SoC", theme.caption, CENTER);

  // Power
  this->card_(c, 332, 60, 676, 272);
  const bool fault = m.system_state == SystemState::FAULT;
  c.text(FONT_R20, 345, 74, system_state_name(m.system_state), fault ? theme.fatal : theme.accent);
  const float p = m.power;
  const char *power_text = std::isnan(p) ? "-- W"
                           : (std::fabs(p) >= 1000 ? fmt("%+.2f kW", (double) p / 1000.0) : fmt("%.0f W", (double) p));
  const Rgb power_color = std::isnan(p) || std::fabs(p) < 20 ? 0xA9B4AF : (p > 0 ? theme.accent : theme.current);
  c.text(FONT_M72, 345, 92, power_text, power_color);
  c.text(FONT_R20, 605, 74, m.contactors_closed ? "Contactors CLOSED" : "Contactors OPEN", theme.text_dim);
  const char *flow = std::isnan(p) || std::fabs(p) < 20 ? "IDLE" : (p > 0 ? "CHARGING" : "DISCHARGING");
  c.text(FONT_R24, 605, 120, flow, theme.text_dim);
  if (!std::isnan(m.soc_change_1h)) {
    c.text(FONT_R18, 995, 75, fmt("1 h: %+.1f%%", (double) m.soc_change_1h), theme.current, RIGHT);
  }
  c.text(FONT_R32, 995, 100, value(m.pack_voltage, "%.1f V", "-- V"), theme.voltage, RIGHT);
  c.text(FONT_R32, 995, 136, value(m.current, "%+.1f A", "-- A"), theme.current, RIGHT);

  // Power bar, centre = 0
  const int bx = 345, bw = 652, by = 187, bh = 16;
  const float full_scale = 18000.0f;
  c.round_rect(bx, by, bw, bh, 4, theme.track);
  if (!std::isnan(p)) {
    float frac = p / full_scale;
    frac = frac < -1 ? -1 : (frac > 1 ? 1 : frac);
    const int mid = bx + bw / 2, len = (int) (std::fabs(frac) * bw / 2);
    if (len > 0) c.round_rect(frac > 0 ? mid : mid - len, by, len, bh, 4, frac > 0 ? theme.accent : theme.current);
  }
  c.fill(bx + bw / 2 - 1, by - 2, 2, bh + 4, theme.text_dim);
  c.text(FONT_R14, bx, 208, "-18.0 kW", theme.hint);
  c.text(FONT_R14, bx + bw / 2, 208, "0", theme.hint, CENTER);
  c.text(FONT_R14, bx + bw, 208, "+18.0 kW", theme.hint, RIGHT);

  auto limit = [&](int y, const char *what, float amps, const char *by_what) {
    const char *line = std::isnan(amps) ? fmt("%s limit --", what)
                       : (std::isinf(amps) ? fmt("%s: no limit", what)
                                           : fmt("%s limit %.1f A  -  set by %s", what, (double) amps, by_what));
    c.text(FONT_R20, 345, y, line, theme.text_dim);
  };
  limit(240, "Charge", m.charge_limit, m.charge_limited_by);
  limit(274, "Discharge", m.discharge_limit, m.discharge_limited_by);

  // Cells
  this->card_(c, 16, 340, 992, 92);
  c.text(FONT_R16, 29, 353, "CELL VOLTAGE MIN / MAX", theme.caption);
  c.text(FONT_R24, 29, 380,
         std::isnan(m.cell_min) ? "--"
                                : fmt("%.3f / %.3f V   delta %.0f mV", (double) m.cell_min, (double) m.cell_max,
                                      (double) ((m.cell_max - m.cell_min) * 1000.0f)),
         theme.text);
  c.text(FONT_R16, 525, 353, "CELL TEMPERATURE MIN / MAX", theme.caption);
  c.text(FONT_R24, 525, 380,
         std::isnan(m.temp_min) ? "--" : fmt("%.1f / %.1f °C", (double) m.temp_min, (double) m.temp_max), theme.text);

  // Today, inverter, balance charge
  this->card_(c, 16, 440, 320, 118);
  c.text(FONT_R16, 29, 453, "TODAY", theme.caption);
  c.text(FONT_R24, 29, 476,
         std::isnan(m.today_in_kwh) ? "--"
                                    : fmt("In %.1f   Out %.1f kWh", (double) m.today_in_kwh, (double) m.today_out_kwh),
         theme.text);
  if (!std::isnan(m.today_in_kwh)) {
    c.text(FONT_R16, 29, 506, fmt("Net %+.1f kWh", (double) (m.today_in_kwh - m.today_out_kwh)), theme.text_dim);
  }
  this->card_(c, 352, 440, 320, 118);
  c.text(FONT_R16, 365, 453, "INVERTER SEES", theme.caption);
  c.text(FONT_R24, 365, 476,
         std::isnan(m.inverter_soc) ? "--"
                                    : fmt("%.0f%%   CVL %.1f V", (double) m.inverter_soc, (double) m.inverter_charge_v),
         theme.text);
  this->card_(c, 688, 440, 320, 118);
  c.text(FONT_R16, 701, 453, "BALANCE CHARGE", theme.caption);
  c.text(FONT_R18, 701, 476, m.balance_charge_status[0] ? m.balance_charge_status : "--", theme.text);
}

}  // namespace hmiui
