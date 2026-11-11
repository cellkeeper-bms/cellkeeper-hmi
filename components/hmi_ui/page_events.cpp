// Events: what is active now, most severe first, each with one line on what it means; and
// what happened earlier and has cleared.
#include "ui_draw.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

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

// The fault logic as a flow chart, with the pack's own situation marked on it. The rules are
// the firmware's (bms/sys/events, bms/app/state_machines): only a FATAL event acts, a CRITICAL
// one turns FATAL if it stays, a FATAL one never clears, and FAULT ends only with a restart.
static void flow_box(Canvas &c, const Theme &t, int x, int y, int w, int h, const char *title, const char *note, Rgb color,
                     bool lit) {
  c.round_rect(x, y, w, h, 10, lit ? mix(t.card, color, 0.24f) : 0x1B2023);
  c.round_frame(x, y, w, h, 10, lit ? 2 : 1, lit ? color : t.track);
  c.text(FONT_M16, x + w / 2, y + (note[0] ? 9 : (h - 19) / 2), title, lit ? t.text : t.caption, CENTER);
  if (note[0]) c.text(FONT_R14, x + w / 2, y + 31, note, lit ? t.text_dim : t.hint, CENTER);
}

static void arrow_head(Canvas &c, int x, int y, Rgb color) {
  c.triangle((float) x, y + 0.5f, x - 9.0f, y - 5.5f, x - 9.0f, y + 6.5f, color);
}

static void fault_logic(Canvas &c, const Theme &t, const Model &m) {
  const int w = X1 - X0, h = Y1 - Y0, left = X0 + 16, right = X1 - 16;
  card(c, t, X0, Y0, w, h);
  button(c, t, X1 - 16 - 150, Y0 + 6, 150, 40, "‹  Active");
  int active[5] = {0, 0, 0, 0, 0};
  for (int i = 0; i < m.event_count; i++) active[(int) m.events[i].level]++;
  const int warnings = active[(int) Level::WARNING], criticals = active[(int) Level::CRITICAL], fatals = active[(int) Level::FATAL];
  const bool fault = m.system_state == SystemState::FAULT;
  const Rgb wire = t.caption;

  // ---- 1: what an event does ----
  c.text(FONT_R14, left, Y0 + 16, "1   WHAT AN EVENT DOES", t.caption);
  const int ey = Y0 + 46;
  flow_box(c, t, left, ey, 250, 54, warnings ? fmt("WARNING  ·  %d active", warnings) : "WARNING", "informs, nothing more", t.warning,
           warnings > 0);
  flow_box(c, t, left, ey + 66, 250, 54, criticals ? fmt("CRITICAL  ·  %d active", criticals) : "CRITICAL",
           "a hard limit passed, a reading lost", t.critical, criticals > 0);
  const int fx = left + 440, fy = ey + 33;
  flow_box(c, t, fx, fy, 176, 54, fatals ? fmt("FATAL  ·  %d active", fatals) : "FATAL", "never clears", t.fatal, fatals > 0);
  const int sx = right - 196;
  flow_box(c, t, sx, fy, 196, 54, "BMS in FAULT", "contactors forced open", t.fatal, fault);
  // WARNING and CRITICAL each lead into FATAL
  c.fill(left + 250, ey + 26, 110, 2, wire);
  c.fill(left + 360, ey + 26, 2, 22, wire);
  c.fill(left + 360, ey + 46, fx - left - 360 - 8, 2, wire);
  arrow_head(c, fx, ey + 47, wire);
  c.fill(left + 250, ey + 92, 110, 2, wire);
  c.fill(left + 360, ey + 72, 2, 22, wire);
  c.fill(left + 360, ey + 72, fx - left - 360 - 8, 2, wire);
  arrow_head(c, fx, ey + 73, wire);
  c.text(FONT_R14, left + 262, ey + 4, "a few, after 20 repeats", t.caption);
  c.text(FONT_R14, left + 262, ey + 98, "if it stays active (most 1 to 20 s)", t.caption);
  c.fill(fx + 176, fy + 26, sx - fx - 176 - 8, 2, wire);
  arrow_head(c, sx, fy + 27, wire);
  c.text(FONT_R14, right, fy + 62, "Only a BMS restart ends FAULT; then it waits for Run.", t.caption, RIGHT);

  // ---- 2: from start to closed contactors ----
  const int py = Y0 + 196;
  c.text(FONT_R14, left, py, "2   FROM START TO CLOSED CONTACTORS", t.caption);
  static const char *const STEPS[] = {"Starting", "Inactive", "Operating", "Supervisor", "Tests", "Precharge", "Closed"};
  static const char *const WHAT[] = {
      "readings must be fresh within 10 s", "waits for Run",                       "asks to close, and retries",
      "the second chip must allow it",      "each contactor is opened and closed", "output must reach pack voltage",
      "current is allowed half a second later",
  };
  // Where the pack is on that path: -1 = nowhere (FAULT or no link)
  int at = -1;
  bool held = false;
  if (m.link == Link::OK && !fault) {
    switch (m.system_state) {
      case SystemState::UNINITIALIZED:
      case SystemState::INITIALIZING: at = 0; break;
      case SystemState::CALIBRATING:
      case SystemState::INACTIVE: at = 1; break;
      case SystemState::OPERATING:
        if (m.contactors_closed) {
          at = 6;
        } else if (m.precharging) {
          at = 5;
        } else if (m.supervisor_state != 1) {
          at = 3;
          held = true;
        } else {
          at = 2;
        }
        break;
      default: break;
    }
  }
  const int bw = 112, pitch = (right - left - bw) / 6;
  for (int i = 0; i < 7; i++) {
    const int x = left + i * pitch;
    const bool here = i == at, done = at > i;
    const Rgb color = here && held ? t.warning : t.good;
    c.round_rect(x, py + 28, bw, 50, 10, here ? mix(t.card, color, 0.24f) : 0x1B2023);
    c.round_frame(x, py + 28, bw, 50, 10, here ? 2 : 1, here ? color : (done ? mix(t.card, t.good, 0.55f) : t.track));
    c.text(FONT_M16, x + bw / 2, py + 43, STEPS[i], here ? t.text : (done ? t.text_dim : t.caption), CENTER);
    text_wrap(c, FONT_R14, x + 2, py + 86, bw + 6, WHAT[i], here ? t.text_dim : t.hint, 3);
    if (i < 6) {
      c.fill(x + bw + 1, py + 52, pitch - bw - 6, 2, done ? mix(t.card, t.good, 0.55f) : wire);
      arrow_head(c, x + pitch - 1, py + 53, done ? mix(t.card, t.good, 0.55f) : wire);
    }
  }

  // ---- now ----
  const int ny = Y0 + 352;
  c.fill(left, ny, right - left, 1, t.card_border);
  c.text(FONT_R14, left, ny + 12, "NOW", t.caption);
  static char text[360];
  const char *now = "";
  if (fault) {
    const Event *list[MAX_EVENTS];
    const int n = top_events(m, list, MAX_EVENTS, Level::FATAL);
    if (n == 0) {
      now = "In FAULT with no fatal event: the readings were not all fresh 10 seconds after the start. The contactors are "
            "forced open. Restart the BMS; it then waits in Inactive for Run.";
    } else {
      char names[160] = "";
      for (int i = 0; i < n && i < 3; i++) {
        snprintf(names + strlen(names), sizeof(names) - strlen(names), "%s%s", i ? ", " : "", event_title(list[i]->type));
      }
      snprintf(text, sizeof(text),
               "In FAULT because of %s%s. The contactors are forced open and stay open. Restart the BMS when the cause "
               "is fixed; it then waits in Inactive for Run.",
               names, n > 3 ? fmt(" and %d more", n - 3) : "");
      now = text;
    }
  } else if (at == 0) {
    now = "Starting: waiting for every reading to be fresh.";
  } else if (at == 1) {
    now = m.system_state == SystemState::CALIBRATING ? "Calibrating. It goes back to Inactive when that is done."
                                                     : "Inactive: the contactors stay open until Run is pressed on the Control page.";
  } else if (at == 2) {
    now = "Operating: asking the contactors to close.";
  } else if (at == 3) {
    snprintf(text, sizeof(text),
             "Waiting for the supervisor, which holds the contactors off: %s. It lets go by itself when its checks pass.",
             m.supervisor_reasons[0] ? m.supervisor_reasons : "no answer from it");
    now = text;
  } else if (at == 5) {
    now = fmt("Precharging: the output is at %.0f V and must reach the pack's %.0f V.", (double) m.output_voltage,
              (double) m.pack_voltage);
  } else if (at == 6) {
    now = criticals > 0 ? "Running, contactors closed. A CRITICAL event is active: if it stays, the BMS goes to FAULT."
          : (warnings > 0 ? "Running, contactors closed. A WARNING is active; it does not stop the pack."
                          : "Running, contactors closed. Nothing is in the way.");
  }
  text_wrap(c, FONT_R20, left, ny + 38, right - left, now, t.text, 4);
}

void draw_events(Canvas &c, const Theme &t, const Model &m, int view) {
  top_line(c, t, m, "Events");
  if (view == 2) {
    fault_logic(c, t, m);
    page_end(c, t, m, Page::EVENTS);
    return;
  }
  if (view == 1) {
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
    if (y + 34 > Y1 - 150) continue;
    c.text(FONT_R18, rx + 16, y, event_title(e.type), t.text_dim);
    c.text(FONT_M16, rx + rw - 16, y + 1, fmt("×%u", (unsigned) e.count), t.caption, RIGHT);
    y += 34;
  }
  if (earlier == 0) c.text(FONT_R18, rx + 16, y, "Nothing since the BMS started", t.hint);
  c.text(FONT_R14, rx + 16, Y1 - 156, "Counts are since the BMS started.", t.hint);
  button(c, t, rx + 16, Y1 - 132, rw - 32, 52, "How a fault happens");
  button(c, t, rx + 16, Y1 - 68, rw - 32, 52, "All event types");

  page_end(c, t, m, Page::EVENTS);
}

}  // namespace hmiui
