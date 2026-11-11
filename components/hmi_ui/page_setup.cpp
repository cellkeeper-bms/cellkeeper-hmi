// Setup: everything that is set rather than watched, in four sections chosen along the top.
// BMS settings (locked until the PIN is entered), the display and its Wi-Fi, the BMS log, and
// what the hardware and firmware are.
#include "ui_draw.h"

#include <algorithm>
#include <cstring>

namespace hmiui {

static const char *const SECTIONS[] = {"BMS settings", "Display and Wi-Fi", "Log", "About"};
static const int SECTION_COUNT = 4, TABS_H = 50, BODY_Y = Y0 + TABS_H + 12;

static const char *uptime_text(uint32_t s) {
  if (s >= 86400) return fmt("%u d %02u h", (unsigned) (s / 86400), (unsigned) (s % 86400 / 3600));
  return fmt("%u:%02u:%02u", (unsigned) (s / 3600), (unsigned) (s % 3600 / 60), (unsigned) (s % 60));
}

static void settings_section(Canvas &c, const Theme &t, const Model &m) {
  const int w = X1 - X0;
  // The lock
  card(c, t, X0, BODY_Y, w, 64);
  padlock(c, X0 + 36.0f, BODY_Y + 30.0f, m.settings_unlocked, m.settings_unlocked ? t.good : t.text_dim);
  c.text(FONT_M20, X0 + 64, BODY_Y + 10, m.settings_unlocked ? "Settings are unlocked" : "Settings are locked", t.text);
  c.text(FONT_R16, X0 + 64, BODY_Y + 36,
         m.settings_unlocked ? "Tap a value to change it. They lock again after 30 seconds."
                             : "Unlock with the PIN to change a value. Values are what the BMS has confirmed.",
         t.caption);
  button(c, t, X1 - 16 - 150, BODY_Y + 8, 150, 48, m.settings_unlocked ? "Lock" : "Unlock");

  // The values, in two columns, grouped
  const int top = BODY_Y + 76, colw = (w - 12) / 2, row_h = 44;
  card(c, t, X0, top, colw, Y1 - top);
  card(c, t, X0 + colw + 12, top, w - colw - 12, Y1 - top);
  if (m.settings == nullptr || m.setting_count == 0) {
    c.text(FONT_R18, X0 + 16, top + 16, "The BMS has not listed its settings yet.", t.hint);
    return;
  }
  int col = 0, y = top + 10;
  const char *group = "";
  for (int i = 0; i < m.setting_count; i++) {
    const Setting &s = m.settings[i];
    const bool new_group = strcmp(group, s.group) != 0;
    const int need = row_h + (new_group ? 26 : 0);
    if (y + need > Y1 - 8) {
      if (col == 1) {
        c.text(FONT_R14, X1 - 22, Y1 - 26, fmt("%d more below", m.setting_count - i), t.out, RIGHT);
        scrollbar(c, t, X1 - 10, top + 8, Y1 - top - 16, 0.0f, (float) i / m.setting_count);
        break;
      }
      col = 1;
      y = top + 10;
    }
    const int x = X0 + col * (colw + 12) + 16, cw = (col == 0 ? colw : w - colw - 12) - 32;
    if (new_group || y == top + 10) {
      c.text(FONT_R14, x, y + 4, s.group, t.caption);
      y += 26;
      group = s.group;
    }
    c.text(FONT_R18, x, y + 9, s.name, m.settings_unlocked ? t.text : t.text_dim);
    const int vw = 132;
    c.round_rect(x + cw - vw, y + 2, vw, row_h - 8, 8, m.settings_unlocked ? 0x222A2E : t.card);
    c.round_frame(x + cw - vw, y + 2, vw, row_h - 8, 8, 1, m.settings_unlocked ? 0x3A464B : t.card_border);
    c.text(FONT_M20, x + cw - vw / 2, y + 8, fmt("%.*f %s", s.decimals, (double) s.value, s.unit),
           m.settings_unlocked ? t.text : t.text_dim, CENTER);
    y += row_h;
  }
}

static void display_section(Canvas &c, const Theme &t, const Model &m) {
  const int w = X1 - X0, lw = (w - 12) / 2, rx = X0 + lw + 12, rw = X1 - rx, h = Y1 - BODY_Y;
  card(c, t, X0, BODY_Y, lw, h);
  c.text(FONT_R14, X0 + 16, BODY_Y + 12, "DISPLAY", t.caption);
  c.text(FONT_R20, X0 + 16, BODY_Y + 44, "Brightness", t.text);
  c.text(FONT_M24, X0 + lw - 16, BODY_Y + 40, fmt("%d %%", m.brightness), t.text, RIGHT);
  {
    const int sx = X0 + 32, sw = lw - 64, sy = BODY_Y + 104, at = sx + sw * m.brightness / 100;
    c.round_rect(sx, sy - 6, sw, 12, 6, t.track);
    c.round_rect(sx, sy - 6, at - sx, 12, 6, t.good);
    c.disc((float) at, (float) sy, 20.0f, t.text);
  }
  text_wrap(c, FONT_R16, X0 + 16, BODY_Y + 144, lw - 32,
            "Kept across restarts. After 5 minutes without a touch the display goes back to Home and dims to 10 % "
            "until it is touched.",
            t.caption, 3);
  button(c, t, X0 + 16, Y1 - 16 - 60, lw - 32, 60, "Restart the display");

  card(c, t, rx, BODY_Y, rw, h);
  c.text(FONT_R14, rx + 16, BODY_Y + 12, "WI-FI", t.caption);
  const bool on = m.wifi_ssid[0] != 0;
  row(c, t, rx + 16, BODY_Y + 44, rw - 32, "Network", on ? m.wifi_ssid : "not connected", on ? 0 : t.caption);
  row(c, t, rx + 16, BODY_Y + 84, rw - 32, "Address", on ? m.ip : "--");
  row(c, t, rx + 16, BODY_Y + 124, rw - 32, "Signal", on ? fmt("%d dBm", m.wifi_rssi) : "--");
  row(c, t, rx + 16, BODY_Y + 164, rw - 32, "Access point", on ? m.wifi_ap : "--");
  text_wrap(c, FONT_R16, rx + 16, BODY_Y + 208, rw - 32,
            "The display works without Wi-Fi. Wi-Fi is for Home Assistant and updates.", t.caption, 3);
  const int bw = (rw - 32 - 12) / 2;
  button(c, t, rx + 16, Y1 - 16 - 60, bw, 60, "Set up a network");
  button(c, t, rx + 16 + bw + 12, Y1 - 16 - 60, bw, 60, "Forget");
}

// Every register the BMS lists, with its number and what it reads now
static void registers_view(Canvas &c, const Theme &t, const Model &m) {
  const int w = X1 - X0, h = Y1 - BODY_Y;
  card(c, t, X0, BODY_Y, w, h);
  c.text(FONT_R14, X0 + 16, BODY_Y + 20, "RAW REGISTERS", t.caption);
  c.text(FONT_R14, X0 + 144, BODY_Y + 20, fmt("%d listed by the BMS, as it sends them", m.register_count), t.hint);
  button(c, t, X1 - 16 - 170, BODY_Y + 8, 170, 44, "‹  Log");
  const int ty = BODY_Y + 62, th = h - 62 - 12, rows = (th - 16) / 21, colw = (w - 24) / 2;
  c.round_rect(X0 + 12, ty, w - 24, th, 8, 0x050606);
  for (int i = 0; i < m.register_count && i < 2 * rows; i++) {
    const RegisterRow &r = m.registers[i];
    const int x = X0 + 26 + (i / rows) * colw, y = ty + 10 + (i % rows) * 21;
    c.text(FONT_MONO14, x, y, fmt("%04X", r.id), t.caption);
    c.text(FONT_MONO14, x + 48, y, r.name, t.text_dim);
    c.text(FONT_MONO14, x + colw - 30, y, r.value, t.text, RIGHT);
  }
  if (m.register_count > 2 * rows) scrollbar(c, t, X1 - 22, ty + 8, th - 16, 0.0f, 2.0f * rows / m.register_count);
}

static void log_section(Canvas &c, const Theme &t, const Model &m) {
  const int w = X1 - X0, h = Y1 - BODY_Y;
  card(c, t, X0, BODY_Y, w, h);
  c.text(FONT_R14, X0 + 16, BODY_Y + 20, "BMS LOG", t.caption);
  c.text(FONT_R14, X0 + 96, BODY_Y + 20, m.log_enabled ? "newest at the bottom" : "off", t.hint);
  button(c, t, X1 - 16 - 2 * 170 - 12, BODY_Y + 8, 170, 44, m.log_enabled ? "Log: on" : "Log: off", "",
         m.log_enabled ? 0x1B4A35 : 0);
  button(c, t, X1 - 16 - 170, BODY_Y + 8, 170, 44, "Raw registers");
  const int ty = BODY_Y + 62, th = h - 62 - 12;
  c.round_rect(X0 + 12, ty, w - 24, th, 8, 0x050606);
  if (!m.log_enabled || m.log_count == 0) {
    c.text(FONT_MONO14, X0 + 26, ty + 12, m.log_enabled ? "Waiting for the BMS..." : "The BMS log is off. Tap \"Log: off\" to start it.",
           t.caption);
    return;
  }
  const int lines = std::min(m.log_count, (th - 20) / 21);
  c.set_clip(X0 + 12, ty, w - 24, th);
  for (int i = 0; i < lines; i++) {
    c.text(FONT_MONO14, X0 + 26, ty + 12 + i * 21, m.log_lines[m.log_count - lines + i], t.text_dim);
  }
  c.reset_clip();
}

static void about_section(Canvas &c, const Theme &t, const Model &m) {
  const int w = X1 - X0, lw = (w - 12) / 2, rx = X0 + lw + 12, rw = X1 - rx, h = Y1 - BODY_Y;
  card(c, t, X0, BODY_Y, lw, h);
  c.text(FONT_R14, X0 + 16, BODY_Y + 12, "BMS", t.caption);
  auto item = [&](int x, int y, const char *label, const char *val) {
    c.text(FONT_R14, x, y, label, t.caption);
    c.text(FONT_R20, x, y + 19, val[0] ? val : "--", t.text);
  };
  item(X0 + 16, BODY_Y + 44, "Pack", m.cell_count > 0 ? fmt("%s  ·  %d cells in series", m.pack_name, m.cell_count) : m.pack_name);
  item(X0 + 16, BODY_Y + 102, "Board", m.board_revision);
  item(X0 + 16, BODY_Y + 160, "Serial number", m.bms_serial);
  item(X0 + 16, BODY_Y + 218, "Firmware", m.bms_firmware);
  item(X0 + 16, BODY_Y + 276, "Running for", m.bms_uptime_s ? uptime_text(m.bms_uptime_s) : "");
  item(X0 + 16, BODY_Y + 334, "Protocol", m.link == Link::OK ? "2" : (m.link == Link::WRONG_PROTOCOL ? "1 or older" : ""));

  card(c, t, rx, BODY_Y, rw, h);
  c.text(FONT_R14, rx + 16, BODY_Y + 12, "DISPLAY", t.caption);
  item(rx + 16, BODY_Y + 44, "Firmware", m.hmi_firmware);
  item(rx + 16, BODY_Y + 102, "Running for", uptime_text(m.hmi_uptime_s));
  item(rx + 16, BODY_Y + 160, "Chip temperature", value(m.hmi_chip_temp, "%.1f °C", ""));
  item(rx + 16, BODY_Y + 218, "Address", m.ip);
  c.text(FONT_R14, rx + 16, Y1 - 60, "CellKeeper: an open BMS for Tesla Model 3/Y packs.", t.hint);
  c.text(FONT_R14, rx + 16, Y1 - 40, "github.com/cellkeeper-bms", t.hint);
}

void draw_setup(Canvas &c, const Theme &t, const Model &m, int section, bool raw_registers) {
  top_line(c, t, m, "Setup");
  // The sections, along the top
  const int w = X1 - X0;
  c.round_rect(X0, Y0, w, TABS_H, 12, t.card);
  for (int i = 0; i < SECTION_COUNT; i++) {
    const int x0 = X0 + 4 + i * (w - 8) / SECTION_COUNT, x1 = X0 + 4 + (i + 1) * (w - 8) / SECTION_COUNT;
    const bool on = i == section;
    if (on) c.round_rect(x0, Y0 + 4, x1 - x0, TABS_H - 8, 9, t.nav_active);
    c.text(on ? FONT_M20 : FONT_R20, (x0 + x1) / 2, Y0 + 13, SECTIONS[i], on ? t.good : t.text_dim, CENTER);
  }
  switch (section) {
    case 0: settings_section(c, t, m); break;
    case 1: display_section(c, t, m); break;
    case 2: raw_registers ? registers_view(c, t, m) : log_section(c, t, m); break;
    default: about_section(c, t, m); break;
  }
  // The BMS settings and its log need the BMS; the display's own pages don't
  page_end(c, t, m, Page::SETUP, section == 0 || section == 2);
}

}  // namespace hmiui
