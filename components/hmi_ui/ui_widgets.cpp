// The widgets: the things on a page that can be touched. Each draws itself and returns what
// the touch did to it this frame.
#include "ui_draw.h"

#include <algorithm>
#include <cstring>

namespace hmiui {

const char *command_name(Command command) {
  static const char *const NAMES[] = {"RUN",           "STOP",                 "CALIBRATE_OFFLINE_LONG", "CALIBRATE_OFFLINE_SHORT",
                                      "CALIBRATE_ONLINE", "ISOLATION_ZERO",    "BALANCE_CHARGE_START",   "BALANCE_CHARGE_STOP",
                                      "RESTART_DISPLAY",  "WIFI_FORGET",       "WIFI_SCAN"};
  return NAMES[(int) command];
}

const Hit *HitTrace::find(const char *label, int nth) const {
  for (int i = 0; i < count; i++) {
    if (strcmp(hits[i].label, label) == 0 && nth-- == 0) return &hits[i];
  }
  return nullptr;
}

static bool in_rect(int px, int py, int x, int y, int w, int h) { return px >= x && px < x + w && py >= y && py < y + h; }

bool Frame::pressed_in(int x, int y, int w, int h) const {
  return active && in_rect(press_x, press_y, x, y, w, h) && in_rect(press_x, press_y, hit_x0, hit_y0, hit_x1 - hit_x0, hit_y1 - hit_y0);
}

bool Frame::held(int x, int y, int w, int h) const { return down && !dragging && this->pressed_in(x, y, w, h); }

bool Frame::tapped(int x, int y, int w, int h, const char *label) {
  if (!active) return false;
  // Only what can be seen can be tapped
  const bool visible = x < hit_x1 && x + w > hit_x0 && y < hit_y1 && y + h > hit_y0;
  if (trace != nullptr && visible && label[0] && trace->count < (int) (sizeof(trace->hits) / sizeof(trace->hits[0]))) {
    Hit &hit = trace->hits[trace->count++];
    strncpy(hit.label, label, sizeof(hit.label) - 1);
    hit.label[sizeof(hit.label) - 1] = 0;
    hit.x = std::max(x, hit_x0);
    hit.y = std::max(y, hit_y0);
    hit.w = std::min(x + w, hit_x1) - hit.x;
    hit.h = std::min(y + h, hit_y1) - hit.y;
  }
  if (!released || dragging || !this->pressed_in(x, y, w, h) || !in_rect(tx, ty, x, y, w, h)) return false;
  released = false;
  changed = true;
  return true;
}

bool hot(Frame &f, int x, int y, int w, int h, const char *label) { return f.tapped(x, y, w, h, label); }

bool button(Frame &f, int x, int y, int w, int h, const char *label, const char *note, Rgb fill, Rgb text, bool enabled) {
  Canvas &c = f.c;
  const Theme &t = f.t;
  Rgb face = fill ? fill : 0x222A2E;
  if (!enabled) {
    face = mix(face, t.card, 0.72f);
  } else if (f.held(x, y, w, h)) {
    face = mix(face, 0xFFFFFF, 0.16f);
  }
  c.round_rect(x, y, w, h, 10, face);
  c.round_frame(x, y, w, h, 10, 1, mix(face, 0xFFFFFF, enabled ? 0.14f : 0.06f));
  const Font &font = h >= 100 ? FONT_M32 : FONT_M20;
  const int th = font.line_height() + (note[0] ? 22 : 0);
  c.text(font, x + w / 2, y + (h - th) / 2, label, !enabled ? t.hint : (text ? text : t.text), CENTER);
  if (note[0]) {
    c.text(FONT_R16, x + w / 2, y + (h - th) / 2 + font.line_height() + 3, note, mix(face, 0xFFFFFF, enabled ? 0.7f : 0.3f), CENTER);
  }
  // Two buttons on a page may share a label ("Current sensor"): the note tells them apart
  const char *name = note[0] ? fmt("%s, %s", label, note) : label;
  return enabled && f.tapped(x, y, w, h, name);
}

bool tabs(Frame &f, int x, int y, int w, int h, const char *const *labels, int count, int &selected) {
  Canvas &c = f.c;
  const Theme &t = f.t;
  bool changed = false;
  c.round_rect(x, y, w, h, 12, t.card);
  for (int i = 0; i < count; i++) {
    const int x0 = x + 4 + i * (w - 8) / count, x1 = x + 4 + (i + 1) * (w - 8) / count;
    if (f.tapped(x0, y, x1 - x0, h, labels[i]) && selected != i) {
      selected = i;
      changed = true;
    }
    const bool on = i == selected;
    if (on) {
      c.round_rect(x0, y + 4, x1 - x0, h - 8, 9, t.nav_active);
    } else if (f.held(x0, y, x1 - x0, h)) {
      c.round_rect(x0, y + 4, x1 - x0, h - 8, 9, 0x1E2427);
    }
    c.text(on ? FONT_M20 : FONT_R20, (x0 + x1) / 2, y + (h - FONT_R20.line_height()) / 2, labels[i], on ? t.good : t.text_dim, CENTER);
  }
  return changed;
}

bool slider(Frame &f, int x, int y, int w, int &percent, const char *label) {
  Canvas &c = f.c;
  const Theme &t = f.t;
  // The whole band around the track takes the finger, not only the knob
  const int band_y = y - 30, band_h = 60;
  f.tapped(x - 20, band_y, w + 40, band_h, label);
  bool changed = false;
  if (f.down && f.pressed_in(x - 20, band_y, w + 40, band_h)) {
    const int value = std::max(0, std::min(100, (f.tx - x) * 100 / w));
    if (value != percent) {
      percent = value;
      changed = true;
    }
  }
  const int at = x + w * percent / 100;
  c.round_rect(x, y - 6, w, 12, 6, t.track);
  if (at > x + 6) c.round_rect(x, y - 6, at - x, 12, 6, t.good);
  c.disc((float) at, (float) y, 20.0f, t.text);
  return changed;
}

int scroll_begin(Frame &f, Scroll &s, int x, int y, int w, int h) {
  if (f.active && f.down && f.dragging && in_rect(f.press_x, f.press_y, x, y, w, h) && f.dy != 0) {
    s.offset -= f.dy;
    f.changed = true;
  }
  if (s.offset < 0) s.offset = 0;
  f.c.set_clip(x, y, w, h);
  f.hit_x0 = x;
  f.hit_y0 = y;
  f.hit_x1 = x + w;
  f.hit_y1 = y + h;
  return y - s.offset;
}

void scroll_end(Frame &f, Scroll &s, int x, int y, int w, int h, int content_h) {
  f.c.reset_clip();
  f.hit_x0 = 0;
  f.hit_y0 = 0;
  f.hit_x1 = SCREEN_W;
  f.hit_y1 = SCREEN_H;
  const int most = std::max(0, content_h - h);
  if (s.offset > most) {
    s.offset = most;
    f.changed = true;
  }
  if (most > 0) scrollbar(f.c, f.t, x + w - 6, y + 4, h - 8, (float) s.offset / content_h, (float) h / content_h);
}

}  // namespace hmiui
