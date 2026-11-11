// The pop-ups: what opens over a page to confirm a command, take the PIN, a number or a
// Wi-Fi password, or tell more about one event. The page behind is dimmed and does not react.
//
// A page opens one with ask_command(), ask_unlock(), open_number(), open_event() or open_wifi();
// the pop-up closes itself and, when it leads somewhere, does what it was opened for through
// Actions.
#include "ui_draw.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hmiui {

static const Rgb BOX = 0x1A1F22, BOX_EDGE = 0x3A454A, KEY = 0x2A3338, FIELD = 0x0B0D0E, GO = 0x1B5E3F, STOP = 0x7A2222;
static const int PIN_MAX = 8, NUMBER_MAX = 9, TEXT_MAX = 63;

// ---- opening and closing ----

void close_popup(UiState &s) { s.popup = Popup(); }

void ask_command(UiState &s, Command command, const char *title, const char *text, const char *action, const char *purpose,
                 bool danger) {
  s.popup = Popup();
  s.popup.kind = Overlay::CONFIRM;
  s.popup.title = title;
  s.popup.text = text;
  s.popup.action = action;
  s.popup.purpose = purpose;
  s.popup.danger = danger;
  s.popup.after = Popup::COMMAND;
  s.popup.command = command;
}

void ask_plain(UiState &s, Command command, const char *title, const char *text, const char *action) {
  ask_command(s, command, title, text, action, "", false);
  s.popup.after = Popup::COMMAND_WITHOUT_PIN;
}

void ask_unlock(UiState &s) {
  s.popup = Popup();
  s.popup.kind = Overlay::PIN;
  s.popup.purpose = "change BMS settings";
  s.popup.after = Popup::UNLOCK;
}

void open_number(UiState &s, int setting) {
  s.popup = Popup();
  s.popup.kind = Overlay::NUMBER;
  s.popup.index = setting;
}

void open_event(UiState &s, int event) {
  s.popup = Popup();
  s.popup.kind = Overlay::EVENT_DETAIL;
  s.popup.index = event;
}

void open_event_type(UiState &s, const Model &m, int type) {
  for (int i = 0; i < m.event_count; i++) {
    if (m.events[i].type == type) {
      open_event(s, i);
      return;
    }
  }
  s.popup = Popup();
  s.popup.kind = Overlay::EVENT_DETAIL;
  s.popup.type = type;
}

void open_wifi(UiState &s) {
  s.popup = Popup();
  s.popup.kind = Overlay::WIFI_LIST;
}

// ---- the pieces ----

static void box(Frame &f, int w, int h, int &x, int &y) {
  f.c.fill_blend(0, 0, SCREEN_W, SCREEN_H, f.t.background, 205);
  x = (SCREEN_W - w) / 2;
  y = (SCREEN_H - h) / 2;
  f.c.round_rect(x, y, w, h, 16, BOX);
  f.c.round_frame(x, y, w, h, 16, 1, BOX_EDGE);
}

// A key of a pad or the keyboard. `name` is what the tests call it when the label is a drawing.
static bool key(Frame &f, int x, int y, int w, int h, const char *label, const Font &font = FONT_M32, Rgb fill = KEY,
                const char *name = nullptr) {
  f.c.round_rect(x, y, w, h, 10, f.held(x, y, w, h) ? mix(fill, 0xFFFFFF, 0.2f) : fill);
  f.c.text(font, x + w / 2, y + (h - font.line_height()) / 2, label, f.t.text, CENTER);
  return f.tapped(x, y, w, h, name != nullptr ? name : label);
}

// The backspace key: an arrow to the left
static bool backspace_key(Frame &f, int x, int y, int w, int h) {
  const bool tapped = key(f, x, y, w, h, "", FONT_M32, KEY, "Backspace");
  const float cx = x + w / 2.0f, cy = y + h / 2.0f;
  f.c.line(cx + 14, cy, cx - 12, cy, 3.5f, f.t.text);
  f.c.line(cx - 12, cy, cx - 2, cy - 10, 3.5f, f.t.text);
  f.c.line(cx - 12, cy, cx - 2, cy + 10, 3.5f, f.t.text);
  return tapped;
}

static bool close_cross(Frame &f, float cx, float cy) {
  f.c.line(cx - 9, cy - 9, cx + 9, cy + 9, 3.0f, f.t.caption);
  f.c.line(cx - 9, cy + 9, cx + 9, cy - 9, 3.0f, f.t.caption);
  return f.tapped((int) cx - 30, (int) cy - 30, 60, 60, "Close");
}

static void append(char *entry, int max, char ch) {
  const int n = (int) strlen(entry);
  if (n >= max) return;
  entry[n] = ch;
  entry[n + 1] = 0;
}

static void erase_last(char *entry) {
  const int n = (int) strlen(entry);
  if (n > 0) entry[n - 1] = 0;
}

// ---- confirm ----

static void confirm(Frame &f) {
  Popup &p = f.s.popup;
  int x, y;
  const int w = 660, h = 316;
  box(f, w, h, x, y);
  f.c.text(FONT_M32, x + 32, y + 28, p.title, f.t.text);
  text_wrap(f.c, FONT_R20, x + 32, y + 86, w - 64, p.text, f.t.text_dim, 4);
  const bool pin = p.after != Popup::COMMAND_WITHOUT_PIN;
  if (pin) f.c.text(FONT_R16, x + 32, y + h - 142, "Next: the PIN.", f.t.caption);
  if (button(f, x + 32, y + h - 32 - 76, 190, 76, "Cancel")) {
    close_popup(f.s);
    return;
  }
  if (button(f, x + 32 + 190 + 16, y + h - 32 - 76, w - 64 - 190 - 16, 76, p.action, "", p.danger ? STOP : GO, 0xFFFFFF)) {
    if (pin) {
      p.kind = Overlay::PIN;
      p.entry[0] = 0;
      p.wrong = false;
    } else {
      const Command command = p.command;
      close_popup(f.s);
      f.act.command(command);
    }
  }
}

// ---- the PIN ----

static void pin_pad(Frame &f) {
  Popup &p = f.s.popup;
  int x, y;
  const int w = 520, h = 552;
  box(f, w, h, x, y);
  if (close_cross(f, x + w - 36.0f, y + 36.0f)) {
    close_popup(f.s);
    return;
  }
  f.c.text(FONT_R18, x + w / 2, y + 20, "Enter the PIN to", f.t.caption, CENTER);
  f.c.text(FONT_M24, x + w / 2, y + 44, p.purpose, f.t.text, CENTER);
  const int n = (int) strlen(p.entry), slots = std::max(4, n);
  for (int i = 0; i < slots; i++) {
    const float cx = x + w / 2.0f + (i - (slots - 1) / 2.0f) * 36.0f, cy = y + 104.0f;
    if (i < n) {
      f.c.disc(cx, cy, 9.0f, f.t.text);
    } else {
      f.c.ring(cx, cy, 7.0f, 9.0f, 0, 360, f.t.hint);
    }
  }
  if (p.wrong) f.c.text(FONT_R18, x + w / 2, y + 126, "Wrong PIN. Try again.", f.t.warning, CENTER);
  const int kw = 148, kh = 80, gap = 10, kx = x + (w - 3 * kw - 2 * gap) / 2, ky = y + 162;
  static const char *const DIGITS[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
  for (int i = 0; i < 9; i++) {
    if (key(f, kx + (i % 3) * (kw + gap), ky + (i / 3) * (kh + gap), kw, kh, DIGITS[i])) {
      append(p.entry, PIN_MAX, DIGITS[i][0]);
      p.wrong = false;
    }
  }
  if (backspace_key(f, kx, ky + 3 * (kh + gap), kw, kh)) erase_last(p.entry);
  if (key(f, kx + kw + gap, ky + 3 * (kh + gap), kw, kh, "0")) {
    append(p.entry, PIN_MAX, '0');
    p.wrong = false;
  }
  if (key(f, kx + 2 * (kw + gap), ky + 3 * (kh + gap), kw, kh, "OK", FONT_M24, n >= 4 ? GO : KEY) && n > 0) {
    if (!f.act.pin_ok(p.entry)) {
      p.entry[0] = 0;
      p.wrong = true;
      return;
    }
    // Copied first: closing the pop-up clears it
    const Popup::After after = p.after;
    const Command command = p.command;
    close_popup(f.s);
    if (after == Popup::COMMAND) f.act.command(command);
    if (after == Popup::UNLOCK) f.act.set_settings_unlocked(true);
  }
}

// ---- a number for a setting ----

static void number_pad(Frame &f) {
  Popup &p = f.s.popup;
  const Model &m = f.m;
  if (m.settings == nullptr || p.index < 0 || p.index >= m.setting_count) {
    close_popup(f.s);
    return;
  }
  const Setting &s = m.settings[p.index];
  int x, y;
  const int w = 800, h = 520, lw = 344;
  box(f, w, h, x, y);
  f.c.text(FONT_R14, x + 32, y + 28, s.group, f.t.caption);
  f.c.text(FONT_M24, x + 32, y + 48, s.name, f.t.text);
  f.c.round_rect(x + 32, y + 100, lw, 100, 12, FIELD);
  f.c.round_frame(x + 32, y + 100, lw, 100, 12, 2, f.t.good);
  num_unit(f.c, x + 32 + lw - 22, y + 168, FONT_M48, p.entry, FONT_R24, s.unit, f.t.text, f.t.caption, RIGHT, 10);
  const float typed = (float) atof(p.entry);
  const bool has_digit = strpbrk(p.entry, "0123456789") != nullptr;
  const bool ok = has_digit && typed >= s.min && typed <= s.max;
  row(f.c, f.t, x + 32, y + 224, lw, "Now", fmt("%.*f %s", s.decimals, (double) s.value, s.unit));
  row(f.c, f.t, x + 32, y + 260, lw, "Allowed", fmt("%.*f to %.*f %s", s.decimals, (double) s.min, s.decimals, (double) s.max, s.unit),
      has_digit && !ok ? f.t.warning : 0);
  text_wrap(f.c, FONT_R16, x + 32, y + 304, lw, "The page shows the new value once the BMS has confirmed it.", f.t.caption, 3);
  if (button(f, x + 32, y + h - 32 - 76, 150, 76, "Cancel")) {
    close_popup(f.s);
    return;
  }
  if (button(f, x + 32 + 166, y + h - 32 - 76, lw - 166, 76, "Set", "", ok ? GO : 0, 0xFFFFFF, ok)) {
    const int index = p.index;
    close_popup(f.s);
    f.act.write_setting(index, typed);
    return;
  }

  const int kw = 118, kh = 84, gap = 10, kx = x + w - 32 - 3 * kw - 2 * gap, ky = y + 32;
  static const char *const KEYS[] = {"7", "8", "9", "4", "5", "6", "1", "2", "3", "±", "0", "."};
  for (int i = 0; i < 12; i++) {
    if (!key(f, kx + (i % 3) * (kw + gap), ky + (i / 3) * (kh + gap), kw, kh, KEYS[i])) continue;
    if (i == 9) {
      // The sign goes in front, or comes off
      if (p.entry[0] == '-') {
        memmove(p.entry, p.entry + 1, strlen(p.entry));
      } else if ((int) strlen(p.entry) < NUMBER_MAX) {
        memmove(p.entry + 1, p.entry, strlen(p.entry) + 1);
        p.entry[0] = '-';
      }
    } else if (i == 11) {
      if (s.decimals > 0 && strchr(p.entry, '.') == nullptr) append(p.entry, NUMBER_MAX, '.');
    } else {
      append(p.entry, NUMBER_MAX, KEYS[i][0]);
    }
  }
  if (backspace_key(f, kx, ky + 4 * (kh + gap), 3 * kw + 2 * gap, 72)) erase_last(p.entry);
}

// ---- Wi-Fi ----

// Four bars, as many lit as the signal is strong
static void signal_bars(Canvas &c, const Theme &t, int x, int y, int rssi) {
  const int lit = rssi > -55 ? 4 : (rssi > -67 ? 3 : (rssi > -78 ? 2 : 1));
  for (int i = 0; i < 4; i++) c.round_rect(x + i * 8, y + 15 - i * 5, 5, 6 + i * 5, 2, i < lit ? t.text : t.track);
}

static void type_for(Popup &p, const char *ssid, bool naming) {
  p.kind = Overlay::KEYBOARD;
  p.entry[0] = 0;
  p.naming = naming;
  p.shifted = p.symbols = p.show = false;
  strncpy(p.ssid, ssid, sizeof(p.ssid) - 1);
  p.ssid[sizeof(p.ssid) - 1] = 0;
}

static void wifi_list(Frame &f) {
  Popup &p = f.s.popup;
  const Model &m = f.m;
  int x, y;
  const int w = 660, h = 484;
  box(f, w, h, x, y);
  if (close_cross(f, x + w - 36.0f, y + 36.0f)) {
    close_popup(f.s);
    return;
  }
  f.c.text(FONT_M32, x + 32, y + 26, "Choose a network", f.t.text);
  const int rows = std::min(m.wifi_network_count, 5);
  for (int i = 0; i < rows; i++) {
    const WifiNetwork &n = m.wifi_networks[i];
    const int ry = y + 84 + i * 62;
    const bool joined = strcmp(n.ssid, m.wifi_ssid) == 0;
    const Rgb face = joined ? f.t.nav_active : KEY;
    f.c.round_rect(x + 24, ry, w - 48, 54, 10, f.held(x + 24, ry, w - 48, 54) ? mix(face, 0xFFFFFF, 0.2f) : face);
    f.c.text(FONT_M20, x + 44, ry + 15, n.ssid, f.t.text);
    if (joined) f.c.text(FONT_R16, x + w - 150, ry + 17, "connected", f.t.good, RIGHT);
    if (n.secured) padlock(f.c, x + w - 116.0f, ry + 25.0f, false, f.t.caption);
    signal_bars(f.c, f.t, x + w - 84, ry + 14, n.rssi);
    if (f.tapped(x + 24, ry, w - 48, 54, n.ssid)) {
      if (n.secured) {
        type_for(p, n.ssid, false);
      } else {
        f.act.wifi_join(n.ssid, "");
        close_popup(f.s);
      }
      return;
    }
  }
  if (rows == 0) f.c.text(FONT_R20, x + 32, y + 100, "No networks found.", f.t.caption);
  if (button(f, x + 24, y + h - 24 - 64, 200, 64, "Scan again")) f.act.command(Command::WIFI_SCAN);
  if (button(f, x + w - 24 - 240, y + h - 24 - 64, 240, 64, "Another network...")) type_for(p, "", true);
}

static void keyboard(Frame &f) {
  Popup &p = f.s.popup;
  Canvas &c = f.c;
  const Theme &t = f.t;
  c.fill_blend(0, 0, SCREEN_W, SCREEN_H, t.background, 225);
  // What is being typed
  c.round_rect(16, 16, SCREEN_W - 32, 190, 16, BOX);
  c.round_frame(16, 16, SCREEN_W - 32, 190, 16, 1, BOX_EDGE);
  c.text(FONT_R18, 40, 34, p.naming ? "Wi-Fi" : "Password for the network", t.caption);
  c.text(FONT_M24, 40, 58, p.naming ? "Name of the network" : p.ssid, t.text);
  c.round_rect(40, 106, 640, 76, 12, FIELD);
  c.round_frame(40, 106, 640, 76, 12, 2, t.good);
  {
    // A password is dots unless Show is on
    char shown[sizeof(p.entry)];
    const int n = (int) strlen(p.entry);
    for (int i = 0; i < n; i++) shown[i] = p.naming || p.show ? p.entry[i] : '*';
    shown[n] = 0;
    c.set_clip(44, 106, 632, 76);
    const int tw = c.text(FONT_M32, 62, 125, shown, t.text);
    c.fill(62 + tw + 4, 122, 3, 44, t.good);
    c.reset_clip();
  }
  if (!p.naming && button(f, 696, 106, 96, 76, p.show ? "Hide" : "Show")) p.show = !p.show;
  if (button(f, 804, 106, 96, 76, "Cancel")) {
    close_popup(f.s);
    return;
  }
  const bool can_go = p.entry[0] != 0;
  if (button(f, 912, 106, 80, 76, p.naming ? "Next" : "Join", "", can_go ? GO : 0, 0xFFFFFF, can_go)) {
    if (p.naming) {
      char name[sizeof(p.ssid)];
      snprintf(name, sizeof(name), "%.32s", p.entry);
      type_for(p, name, false);
    } else {
      f.act.wifi_join(p.ssid, p.entry);
      close_popup(f.s);
    }
    return;
  }

  // The keys: digits, then three rows of letters or of symbols
  static const char *const LETTERS[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};
  static const char *const SYMBOLS[] = {"1234567890", "!@#$%^&*()", "-_=+;:'\"/", ",.?<>[]"};
  const char *const *rows = p.symbols ? SYMBOLS : LETTERS;
  const int kh = 64, gap = 8, y0 = 224, pitch = (SCREEN_W - 32 + gap) / 10, kw = pitch - gap;
  for (int r = 0; r < 4; r++) {
    const int n = (int) strlen(rows[r]);
    const int x0 = 16 + (10 - n) * pitch / 2, y = y0 + r * (kh + gap);
    for (int i = 0; i < n; i++) {
      char ch = rows[r][i];
      if (p.shifted && ch >= 'a' && ch <= 'z') ch = (char) (ch - 32);
      const char label[2] = {ch, 0};
      if (key(f, x0 + i * pitch, y, kw, kh, label, FONT_M24)) {
        append(p.entry, p.naming ? 32 : TEXT_MAX, ch);
        p.shifted = false;
      }
    }
    if (r == 3) {
      // Shift on the left, backspace on the right of the last row
      const int sw = x0 - 16 - gap;
      c.round_rect(16, y, sw, kh, 10, p.shifted ? t.nav_active : (f.held(16, y, sw, kh) ? mix(KEY, 0xFFFFFF, 0.2f) : KEY));
      const float cx = 16 + sw / 2.0f, cy = y + kh / 2.0f;
      c.triangle(cx, cy - 14, cx - 14, cy + 2, cx + 14, cy + 2, p.shifted ? t.good : t.text);
      c.fill((int) cx - 6, (int) cy + 2, 12, 11, p.shifted ? t.good : t.text);
      if (f.tapped(16, y, sw, kh, "Shift")) p.shifted = !p.shifted;
      if (backspace_key(f, x0 + n * pitch, y, sw, kh)) erase_last(p.entry);
    }
  }
  const int y = y0 + 4 * (kh + gap);
  if (key(f, 16, y, 140, kh, p.symbols ? "abc" : "#+=", FONT_M24)) p.symbols = !p.symbols;
  if (key(f, 16 + 148, y, SCREEN_W - 32 - 148 - 2 * (kw + gap), kh, "space", FONT_R20)) append(p.entry, p.naming ? 32 : TEXT_MAX, ' ');
  if (key(f, SCREEN_W - 16 - 2 * kw - gap, y, kw, kh, "-", FONT_M24, KEY, "Hyphen")) append(p.entry, p.naming ? 32 : TEXT_MAX, '-');
  if (key(f, SCREEN_W - 16 - kw, y, kw, kh, ".", FONT_M24, KEY, "Full stop")) append(p.entry, p.naming ? 32 : TEXT_MAX, '.');
}

// ---- one event ----

static void event_detail(Frame &f) {
  const Popup &p = f.s.popup;
  const Model &m = f.m;
  const bool listed = p.index >= 0 && p.index < m.event_count;
  if (!listed && (p.type < 0 || p.type >= event_type_count())) {
    close_popup(f.s);
    return;
  }
  // An event type the BMS has not reported has never happened
  const Event never = {(uint8_t) p.type, Level::INACTIVE, 0, 0};
  const Event &e = listed ? m.events[p.index] : never;
  Canvas &c = f.c;
  const Theme &t = f.t;
  int x, y;
  const int w = 700, h = 420;
  box(f, w, h, x, y);
  const bool active = e.level != Level::INACTIVE, tinted = active && e.level != Level::INFO;
  const char *level = active ? level_name(e.level) : (e.count > 0 ? "NOT ACTIVE" : "NEVER HAPPENED");
  const int tw = Canvas::text_width(FONT_M16, level) + 24;
  c.round_rect(x + 32, y + 28, tw, 28, 14, tinted ? level_color(t, e.level) : t.track);
  c.text(FONT_M16, x + 32 + tw / 2, y + 32, level, tinted ? level_text_color(e.level) : t.text_dim, CENTER);
  c.text(FONT_M32, x + 32, y + 68, event_title(e.type), t.text);
  text_wrap(c, FONT_R20, x + 32, y + 114, w - 64, event_help(e.type), t.text_dim, 3);
  c.fill(x + 32, y + 196, w - 64, 1, BOX_EDGE);
  row(c, t, x + 32, y + 212, w - 64, "Happened",
      e.count == 0 ? "not since the BMS started" : fmt("%u time%s since the BMS started", (unsigned) e.count, e.count == 1 ? "" : "s"));
  c.text(FONT_R18, x + 32, y + 248, "The BMS's name for it", t.text_dim);
  c.text(FONT_MONO14, x + w - 32, y + 252, event_name(e.type), t.text, RIGHT);
  c.text(FONT_R18, x + 32, y + 284, "Data of the last time", t.text_dim);
  c.text(FONT_MONO14, x + w - 32, y + 288, e.data ? fmt("0x%llX", (unsigned long long) e.data) : "none", t.text, RIGHT);
  if (button(f, x + w - 32 - 190, y + h - 28 - 64, 190, 64, "Close")) close_popup(f.s);
}

void draw_popup(Frame &f) {
  switch (f.s.popup.kind) {
    case Overlay::CONFIRM: confirm(f); break;
    case Overlay::PIN: pin_pad(f); break;
    case Overlay::NUMBER: number_pad(f); break;
    case Overlay::WIFI_LIST: wifi_list(f); break;
    case Overlay::KEYBOARD: keyboard(f); break;
    case Overlay::EVENT_DETAIL: event_detail(f); break;
    default: break;
  }
}

}  // namespace hmiui
