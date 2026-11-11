// The pop-ups: what opens over a page to confirm a command, take the PIN, a number or a
// Wi-Fi password, or tell more about one event. The page behind is dimmed.
#include "ui_draw.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace hmiui {

static const Rgb BOX = 0x1A1F22, BOX_EDGE = 0x3A454A, KEY = 0x2A3338, FIELD = 0x0B0D0E, GO = 0x1B5E3F, STOP = 0x7A2222;

static void box(Canvas &c, const Theme &t, int w, int h, int &x, int &y) {
  c.fill_blend(0, 0, SCREEN_W, SCREEN_H, t.background, 205);
  x = (SCREEN_W - w) / 2;
  y = (SCREEN_H - h) / 2;
  c.round_rect(x, y, w, h, 16, BOX);
  c.round_frame(x, y, w, h, 16, 1, BOX_EDGE);
}

static void key(Canvas &c, const Theme &t, int x, int y, int w, int h, const char *label, const Font &font = FONT_M32,
                Rgb fill = KEY) {
  c.round_rect(x, y, w, h, 10, fill);
  c.text(font, x + w / 2, y + (h - font.line_height()) / 2, label, t.text, CENTER);
}

// The backspace key: an arrow to the left
static void backspace_key(Canvas &c, const Theme &t, int x, int y, int w, int h) {
  c.round_rect(x, y, w, h, 10, KEY);
  const float cx = x + w / 2.0f, cy = y + h / 2.0f;
  c.line(cx + 14, cy, cx - 12, cy, 3.5f, t.text);
  c.line(cx - 12, cy, cx - 2, cy - 10, 3.5f, t.text);
  c.line(cx - 12, cy, cx - 2, cy + 10, 3.5f, t.text);
}

static void close_cross(Canvas &c, const Theme &t, float cx, float cy) {
  c.line(cx - 9, cy - 9, cx + 9, cy + 9, 3.0f, t.caption);
  c.line(cx - 9, cy + 9, cx + 9, cy - 9, 3.0f, t.caption);
}

static void confirm(Canvas &c, const Theme &t, const Popup &p) {
  int x, y;
  const int w = 660, h = 316;
  box(c, t, w, h, x, y);
  c.text(FONT_M32, x + 32, y + 28, p.title, t.text);
  text_wrap(c, FONT_R20, x + 32, y + 86, w - 64, p.text, t.text_dim, 4);
  c.text(FONT_R16, x + 32, y + h - 142, "Next: the PIN.", t.caption);
  button(c, t, x + 32, y + h - 32 - 76, 190, 76, "Cancel");
  button(c, t, x + 32 + 190 + 16, y + h - 32 - 76, w - 64 - 190 - 16, 76, p.action, "", p.danger ? STOP : GO, 0xFFFFFF);
}

static void pin_pad(Canvas &c, const Theme &t, const Popup &p) {
  int x, y;
  const int w = 520, h = 552;
  box(c, t, w, h, x, y);
  close_cross(c, t, x + w - 36.0f, y + 36.0f);
  c.text(FONT_R18, x + w / 2, y + 20, "Enter the PIN to", t.caption, CENTER);
  c.text(FONT_M24, x + w / 2, y + 44, p.action, t.text, CENTER);
  const int n = (int) strlen(p.entry), slots = std::max(4, n);
  for (int i = 0; i < slots; i++) {
    const float cx = x + w / 2.0f + (i - (slots - 1) / 2.0f) * 36.0f, cy = y + 104.0f;
    if (i < n) {
      c.disc(cx, cy, 9.0f, t.text);
    } else {
      c.ring(cx, cy, 7.0f, 9.0f, 0, 360, t.hint);
    }
  }
  if (p.wrong) c.text(FONT_R18, x + w / 2, y + 126, "Wrong PIN. Try again.", t.warning, CENTER);
  const int kw = 148, kh = 80, gap = 10, kx = x + (w - 3 * kw - 2 * gap) / 2, ky = y + 162;
  static const char *const DIGITS[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
  for (int i = 0; i < 9; i++) key(c, t, kx + (i % 3) * (kw + gap), ky + (i / 3) * (kh + gap), kw, kh, DIGITS[i]);
  backspace_key(c, t, kx, ky + 3 * (kh + gap), kw, kh);
  key(c, t, kx + kw + gap, ky + 3 * (kh + gap), kw, kh, "0");
  key(c, t, kx + 2 * (kw + gap), ky + 3 * (kh + gap), kw, kh, "OK", FONT_M24, n >= 4 ? GO : KEY);
}

static void number_pad(Canvas &c, const Theme &t, const Model &m, const Popup &p) {
  if (m.settings == nullptr || p.index < 0 || p.index >= m.setting_count) return;
  const Setting &s = m.settings[p.index];
  int x, y;
  const int w = 800, h = 520, lw = 344;
  box(c, t, w, h, x, y);
  c.text(FONT_R14, x + 32, y + 28, s.group, t.caption);
  c.text(FONT_M24, x + 32, y + 48, s.name, t.text);
  c.round_rect(x + 32, y + 100, lw, 100, 12, FIELD);
  c.round_frame(x + 32, y + 100, lw, 100, 12, 2, t.good);
  num_unit(c, x + 32 + lw - 22, y + 168, FONT_M48, p.entry, FONT_R24, s.unit, t.text, t.caption, RIGHT, 10);
  const float typed = (float) atof(p.entry);
  const bool ok = p.entry[0] != 0 && typed >= s.min && typed <= s.max;
  row(c, t, x + 32, y + 224, lw, "Now", fmt("%.*f %s", s.decimals, (double) s.value, s.unit));
  row(c, t, x + 32, y + 260, lw, "Allowed", fmt("%.*f to %.*f %s", s.decimals, (double) s.min, s.decimals, (double) s.max, s.unit),
      p.entry[0] != 0 && !ok ? t.warning : 0);
  text_wrap(c, FONT_R16, x + 32, y + 304, lw, "The page shows the new value once the BMS has confirmed it.", t.caption, 3);
  button(c, t, x + 32, y + h - 32 - 76, 150, 76, "Cancel");
  button(c, t, x + 32 + 166, y + h - 32 - 76, lw - 166, 76, "Set", "", ok ? GO : 0, ok ? 0xFFFFFF : t.hint);

  const int kw = 118, kh = 84, gap = 10, kx = x + w - 32 - 3 * kw - 2 * gap, ky = y + 32;
  static const char *const KEYS[] = {"7", "8", "9", "4", "5", "6", "1", "2", "3", "±", "0", "."};
  for (int i = 0; i < 12; i++) key(c, t, kx + (i % 3) * (kw + gap), ky + (i / 3) * (kh + gap), kw, kh, KEYS[i]);
  backspace_key(c, t, kx, ky + 4 * (kh + gap), 3 * kw + 2 * gap, 72);
}

// Four bars, as many lit as the signal is strong
static void signal_bars(Canvas &c, const Theme &t, int x, int y, int rssi) {
  const int lit = rssi > -55 ? 4 : (rssi > -67 ? 3 : (rssi > -78 ? 2 : 1));
  for (int i = 0; i < 4; i++) c.round_rect(x + i * 8, y + 15 - i * 5, 5, 6 + i * 5, 2, i < lit ? t.text : t.track);
}

static void wifi_list(Canvas &c, const Theme &t, const Model &m) {
  int x, y;
  const int w = 660, h = 484;
  box(c, t, w, h, x, y);
  close_cross(c, t, x + w - 36.0f, y + 36.0f);
  c.text(FONT_M32, x + 32, y + 26, "Choose a network", t.text);
  const int rows = std::min(m.wifi_network_count, 5);
  for (int i = 0; i < rows; i++) {
    const WifiNetwork &n = m.wifi_networks[i];
    const int ry = y + 84 + i * 62;
    const bool joined = strcmp(n.ssid, m.wifi_ssid) == 0;
    c.round_rect(x + 24, ry, w - 48, 54, 10, joined ? t.nav_active : KEY);
    c.text(FONT_M20, x + 44, ry + 15, n.ssid, t.text);
    if (joined) c.text(FONT_R16, x + w - 150, ry + 17, "connected", t.good, RIGHT);
    if (n.secured) padlock(c, x + w - 116.0f, ry + 25.0f, false, t.caption);
    signal_bars(c, t, x + w - 84, ry + 14, n.rssi);
  }
  if (rows == 0) c.text(FONT_R20, x + 32, y + 100, "No networks found.", t.caption);
  button(c, t, x + 24, y + h - 24 - 64, 200, 64, "Scan again");
  button(c, t, x + w - 24 - 240, y + h - 24 - 64, 240, 64, "Another network...");
}

static void keyboard(Canvas &c, const Theme &t, const Popup &p) {
  c.fill_blend(0, 0, SCREEN_W, SCREEN_H, t.background, 225);
  // What is being typed
  c.round_rect(16, 16, SCREEN_W - 32, 190, 16, BOX);
  c.round_frame(16, 16, SCREEN_W - 32, 190, 16, 1, BOX_EDGE);
  c.text(FONT_R18, 40, 34, "Password for the network", t.caption);
  c.text(FONT_M24, 40, 58, p.title, t.text);
  c.round_rect(40, 106, 640, 76, 12, FIELD);
  c.round_frame(40, 106, 640, 76, 12, 2, t.good);
  const int tw = c.text(FONT_M32, 62, 125, p.entry, t.text);
  c.fill(62 + tw + 4, 122, 3, 44, t.good);
  button(c, t, 696, 106, 96, 76, "Show");
  button(c, t, 804, 106, 96, 76, "Cancel");
  button(c, t, 912, 106, 80, 76, "Join", "", GO, 0xFFFFFF);

  // The keys
  static const char *const ROWS[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};
  const int kh = 64, gap = 8, y0 = 224, pitch = (SCREEN_W - 32 + gap) / 10, kw = pitch - gap;
  for (int r = 0; r < 4; r++) {
    const int n = (int) strlen(ROWS[r]);
    const int x0 = 16 + (10 - n) * pitch / 2, y = y0 + r * (kh + gap);
    for (int i = 0; i < n; i++) {
      char ch = ROWS[r][i];
      if (p.shifted && ch >= 'a' && ch <= 'z') ch = (char) (ch - 32);
      const char label[2] = {ch, 0};
      key(c, t, x0 + i * pitch, y, kw, kh, label, FONT_M24);
    }
    if (r == 3) {
      // Shift on the left, backspace on the right of the last letter row
      const int sw = x0 - 16 - gap;
      c.round_rect(16, y, sw, kh, 10, p.shifted ? t.nav_active : KEY);
      const float cx = 16 + sw / 2.0f, cy = y + kh / 2.0f;
      c.triangle(cx, cy - 14, cx - 14, cy + 2, cx + 14, cy + 2, p.shifted ? t.good : t.text);
      c.fill((int) cx - 6, (int) cy + 2, 12, 11, p.shifted ? t.good : t.text);
      backspace_key(c, t, x0 + n * pitch, y, sw, kh);
    }
  }
  const int y = y0 + 4 * (kh + gap);
  key(c, t, 16, y, 140, kh, "#+=", FONT_M24);
  key(c, t, 16 + 148, y, SCREEN_W - 32 - 148 - 2 * (kw + gap), kh, "space", FONT_R20);
  key(c, t, SCREEN_W - 16 - 2 * kw - gap, y, kw, kh, "-", FONT_M24);
  key(c, t, SCREEN_W - 16 - kw, y, kw, kh, ".", FONT_M24);
}

static void event_detail(Canvas &c, const Theme &t, const Model &m, const Popup &p) {
  if (p.index < 0 || p.index >= m.event_count) return;
  const Event &e = m.events[p.index];
  int x, y;
  const int w = 700, h = 420;
  box(c, t, w, h, x, y);
  const bool active = e.level != Level::INACTIVE;
  const char *level = active ? level_name(e.level) : "NOT ACTIVE";
  const int tw = Canvas::text_width(FONT_M16, level) + 24;
  c.round_rect(x + 32, y + 28, tw, 28, 14, active && e.level != Level::INFO ? level_color(t, e.level) : t.track);
  c.text(FONT_M16, x + 32 + tw / 2, y + 32, level, active && e.level != Level::INFO ? level_text_color(e.level) : t.text_dim,
         CENTER);
  c.text(FONT_M32, x + 32, y + 68, event_title(e.type), t.text);
  text_wrap(c, FONT_R20, x + 32, y + 114, w - 64, event_help(e.type), t.text_dim, 3);
  c.fill(x + 32, y + 196, w - 64, 1, BOX_EDGE);
  row(c, t, x + 32, y + 212, w - 64, "Happened", fmt("%u times since the BMS started", (unsigned) e.count));
  c.text(FONT_R18, x + 32, y + 248, "The BMS's name for it", t.text_dim);
  c.text(FONT_MONO14, x + w - 32, y + 252, event_name(e.type), t.text, RIGHT);
  c.text(FONT_R18, x + 32, y + 284, "Data of the last time", t.text_dim);
  c.text(FONT_MONO14, x + w - 32, y + 288, e.data ? fmt("0x%llX", (unsigned long long) e.data) : "none", t.text, RIGHT);
  button(c, t, x + w - 32 - 190, y + h - 28 - 64, 190, 64, "Close");
}

void draw_popup(Canvas &c, const Theme &t, const Model &m, const Popup &p) {
  switch (p.kind) {
    case Overlay::CONFIRM: confirm(c, t, p); break;
    case Overlay::PIN: pin_pad(c, t, p); break;
    case Overlay::NUMBER: number_pad(c, t, m, p); break;
    case Overlay::WIFI_LIST: wifi_list(c, t, m); break;
    case Overlay::KEYBOARD: keyboard(c, t, p); break;
    case Overlay::EVENT_DETAIL: event_detail(c, t, m, p); break;
    default: break;
  }
}

}  // namespace hmiui
