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

static void settings_section(Frame &f) {
  Canvas &c = f.c;
  const Theme &t = f.t;
  const Model &m = f.m;
  const int w = X1 - X0;
  // The lock
  card(c, t, X0, BODY_Y, w, 64);
  padlock(c, X0 + 36.0f, BODY_Y + 30.0f, m.settings_unlocked, m.settings_unlocked ? t.good : t.text_dim);
  c.text(FONT_M20, X0 + 64, BODY_Y + 10, m.settings_unlocked ? "Settings are unlocked" : "Settings are locked", t.text);
  c.text(FONT_R16, X0 + 64, BODY_Y + 36,
         m.settings_unlocked ? "Tap a value to change it. They lock again after 30 seconds."
                             : "Unlock with the PIN to change a value. Values are what the BMS has confirmed.",
         t.caption);
  if (button(f, X1 - 16 - 150, BODY_Y + 8, 150, 48, m.settings_unlocked ? "Lock" : "Unlock")) {
    if (m.settings_unlocked) {
      f.act.set_settings_unlocked(false);
    } else {
      ask_unlock(f.s);
    }
  }

  // The values: two columns of groups, which scroll together when there are more than fit
  const int top = BODY_Y + 76, colw = (w - 12) / 2, row_h = 44, group_h = 26, view_h = Y1 - top;
  card(c, t, X0, top, colw, view_h);
  card(c, t, X0 + colw + 12, top, w - colw - 12, view_h);
  if (m.settings == nullptr || m.setting_count == 0) {
    c.text(FONT_R18, X0 + 16, top + 16, "The BMS has not listed its settings yet.", t.hint);
    return;
  }
  // The second column starts with the first group that begins past half of the whole list
  int total = 0;
  const char *group = "";
  for (int i = 0; i < m.setting_count; i++) {
    if (strcmp(group, m.settings[i].group) != 0) total += group_h;
    group = m.settings[i].group;
    total += row_h;
  }
  int split = m.setting_count, first = 0;
  group = "";
  for (int i = 0; i < m.setting_count; i++) {
    const bool new_group = strcmp(group, m.settings[i].group) != 0;
    if (new_group && first >= total / 2) {
      split = i;
      break;
    }
    if (new_group) first += group_h;
    group = m.settings[i].group;
    first += row_h;
  }
  const int y0 = scroll_begin(f, f.s.settings_scroll, X0, top + 2, w, view_h - 4) + 8;
  for (int col = 0; col < 2; col++) {
    const int x = X0 + col * (colw + 12) + 16, cw = (col == 0 ? colw : w - colw - 12) - 32, vw = 132;
    int y = y0;
    group = "";
    for (int i = col == 0 ? 0 : split; i < (col == 0 ? split : m.setting_count); i++) {
      const Setting &s = m.settings[i];
      if (strcmp(group, s.group) != 0) {
        c.text(FONT_R14, x, y + 4, s.group, t.caption);
        y += group_h;
        group = s.group;
      }
      c.text(FONT_R18, x, y + 9, s.name, m.settings_unlocked ? t.text : t.text_dim);
      const bool held = m.settings_unlocked && f.held(x + cw - vw, y + 2, vw, row_h - 8);
      c.round_rect(x + cw - vw, y + 2, vw, row_h - 8, 8, held ? 0x38444A : (m.settings_unlocked ? 0x222A2E : t.card));
      c.round_frame(x + cw - vw, y + 2, vw, row_h - 8, 8, 1, m.settings_unlocked ? 0x3A464B : t.card_border);
      c.text(FONT_M20, x + cw - vw / 2, y + 8, std::isnan(s.value) ? "--" : fmt("%.*f %s", s.decimals, (double) s.value, s.unit),
             m.settings_unlocked ? t.text : t.text_dim, CENTER);
      if (m.settings_unlocked && hot(f, x + cw - vw, y + 2, vw, row_h - 8, s.name)) open_number(f.s, i);
      y += row_h;
    }
  }
  scroll_end(f, f.s.settings_scroll, X0, top + 2, w, view_h - 4, std::max(first, total - first) + 20);
}

static void display_section(Frame &f) {
  Canvas &c = f.c;
  const Theme &t = f.t;
  const Model &m = f.m;
  const int w = X1 - X0, lw = (w - 12) / 2, rx = X0 + lw + 12, rw = X1 - rx, h = Y1 - BODY_Y;
  card(c, t, X0, BODY_Y, lw, h);
  c.text(FONT_R14, X0 + 16, BODY_Y + 12, "DISPLAY", t.caption);
  c.text(FONT_R20, X0 + 16, BODY_Y + 44, "Brightness", t.text);
  c.text(FONT_M24, X0 + lw - 16, BODY_Y + 40, fmt("%d %%", m.brightness), t.text, RIGHT);
  {
    // Never all the way down: a black screen can't be found again
    int percent = m.brightness;
    if (slider(f, X0 + 32, BODY_Y + 104, lw - 64, percent, "Brightness")) f.act.set_brightness(percent < 5 ? 5 : percent);
  }
  text_wrap(c, FONT_R16, X0 + 16, BODY_Y + 144, lw - 32,
            "Kept across restarts. After 5 minutes without a touch the display goes back to Home and dims to 10 % "
            "until it is touched.",
            t.caption, 3);
  if (button(f, X0 + 16, Y1 - 16 - 60, lw - 32, 60, "Restart the display")) {
    ask_plain(f.s, Command::RESTART_DISPLAY, "Restart the display?",
              "Only the display restarts. The BMS keeps running and the contactors stay as they are.", "Restart");
  }

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
  if (button(f, rx + 16, Y1 - 16 - 60, bw, 60, "Set up a network")) open_wifi(f.s);
  if (button(f, rx + 16 + bw + 12, Y1 - 16 - 60, bw, 60, "Forget", "", 0, 0, on)) {
    ask_plain(f.s, Command::WIFI_FORGET, "Forget this network?",
              "The display leaves the Wi-Fi network and forgets its password. Home Assistant loses the display until a "
              "network is set up again.",
              "Forget the network");
  }
}

// Every register the BMS lists, with its number and what it reads now
static void registers_view(Frame &f) {
  Canvas &c = f.c;
  const Theme &t = f.t;
  const Model &m = f.m;
  const int w = X1 - X0, h = Y1 - BODY_Y;
  card(c, t, X0, BODY_Y, w, h);
  c.text(FONT_R14, X0 + 16, BODY_Y + 20, "RAW REGISTERS", t.caption);
  c.text(FONT_R14, X0 + 144, BODY_Y + 20, fmt("%d listed by the BMS, as it sends them", m.register_count), t.hint);
  if (button(f, X1 - 16 - 170, BODY_Y + 8, 170, 44, "‹  Log")) f.s.raw_registers = false;
  const int ty = BODY_Y + 62, th = h - 62 - 12, colw = (w - 24) / 2, rows = (m.register_count + 1) / 2;
  c.round_rect(X0 + 12, ty, w - 24, th, 8, 0x050606);
  const int y0 = scroll_begin(f, f.s.registers_scroll, X0 + 12, ty + 2, w - 24, th - 4) + 8;
  for (int i = 0; i < m.register_count; i++) {
    const RegisterRow &r = m.registers[i];
    const int x = X0 + 26 + (i / rows) * colw, y = y0 + (i % rows) * 21;
    c.text(FONT_MONO14, x, y, fmt("%04X", r.id), t.caption);
    c.text(FONT_MONO14, x + 48, y, r.name, t.text_dim);
    c.text(FONT_MONO14, x + colw - 30, y, r.value, t.text, RIGHT);
  }
  scroll_end(f, f.s.registers_scroll, X0 + 12, ty + 2, w - 24, th - 4, rows * 21 + 16);
}

static void log_section(Frame &f) {
  Canvas &c = f.c;
  const Theme &t = f.t;
  const Model &m = f.m;
  const int w = X1 - X0, h = Y1 - BODY_Y;
  card(c, t, X0, BODY_Y, w, h);
  c.text(FONT_R14, X0 + 16, BODY_Y + 20, "BMS LOG", t.caption);
  c.text(FONT_R14, X0 + 96, BODY_Y + 20, !m.log_enabled ? "off" : (f.s.log_follow ? "newest at the bottom" : "scrolled back"),
         t.hint);
  if (button(f, X1 - 16 - 2 * 170 - 12, BODY_Y + 8, 170, 44, m.log_enabled ? "Log: on" : "Log: off", "",
             m.log_enabled ? 0x1B4A35 : 0)) {
    f.act.set_log_enabled(!m.log_enabled);
  }
  if (button(f, X1 - 16 - 170, BODY_Y + 8, 170, 44, "Raw registers")) f.s.raw_registers = true;
  const int ty = BODY_Y + 62, th = h - 62 - 12;
  c.round_rect(X0 + 12, ty, w - 24, th, 8, 0x050606);
  if (!m.log_enabled || m.log_count == 0) {
    c.text(FONT_MONO14, X0 + 26, ty + 12, m.log_enabled ? "Waiting for the BMS..." : "The BMS log is off. Tap \"Log: off\" to start it.",
           t.caption);
    return;
  }
  // The newest line stays in view until the log is scrolled back, and again once it is
  // scrolled to its end
  const int content = m.log_count * 21 + 16, view = th - 4, most = std::max(0, content - view);
  if (f.s.log_follow) f.s.log_scroll.offset = most;
  const int y0 = scroll_begin(f, f.s.log_scroll, X0 + 12, ty + 2, w - 24, view) + 8;
  for (int i = 0; i < m.log_count; i++) c.text(FONT_MONO14, X0 + 26, y0 + i * 21, m.log_lines[i], t.text_dim);
  scroll_end(f, f.s.log_scroll, X0 + 12, ty + 2, w - 24, view, content);
  f.s.log_follow = f.s.log_scroll.offset >= most;
}

static void about_section(Frame &f) {
  Canvas &c = f.c;
  const Theme &t = f.t;
  const Model &m = f.m;
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

void draw_setup(Frame &f) {
  top_line(f, "Setup");
  // The sections, along the top
  if (tabs(f, X0, Y0, X1 - X0, TABS_H, SECTIONS, SECTION_COUNT, f.s.setup_section)) f.s.raw_registers = false;
  const int section = f.s.setup_section;
  switch (section) {
    case 0: settings_section(f); break;
    case 1: display_section(f); break;
    case 2: f.s.raw_registers ? registers_view(f) : log_section(f); break;
    default: about_section(f); break;
  }
  // The BMS settings and its log need the BMS; the display's own pages don't
  page_end(f, Page::SETUP, section == 0 || section == 2);
}

}  // namespace hmiui
