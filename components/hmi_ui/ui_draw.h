// What the pages share: text and number helpers, the values worked out from the model, the
// small widgets (card, pill, cell chart), the navigation and the pages' entry points.
#pragma once

#include "ui_canvas.h"
#include "ui_fonts.h"
#include "ui_model.h"
#include "ui_theme.h"

namespace hmiui {

static const int SCREEN_W = 1024;
static const int SCREEN_H = 600;

// printf into one of a few static buffers: good for the arguments of one drawing call
const char *fmt(const char *format, ...) __attribute__((format(printf, 1, 2)));
// A value with its format, or the placeholder when it isn't known
const char *value(float v, const char *format, const char *missing = "--");

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---- worked out from the model ----

enum class Flow : uint8_t { UNKNOWN, IDLE, CHARGING, DISCHARGING };
Flow flow_of(const Model &m);
const char *flow_name(Flow f);
Rgb flow_color(const Theme &t, Flow f);
Rgb soc_color(const Theme &t, float soc);

Rgb level_color(const Theme &t, Level level);
Rgb level_text_color(Level level);  // text that reads on a level colour
const char *level_name(Level level);
const char *system_state_name(SystemState s);

// Power as a number and its unit: "8.41" "kW", "840" "W"
struct NumberUnit {
  const char *number;
  const char *unit;
};
NumberUnit power_text(float watts);

float ah_left(const Model &m);
float kwh_left(const Model &m);
// "Full in ≈ 2 h 50 min", "Empty in ≈ 11 h 15 min"; "" when it can't be said
const char *time_to_text(const Model &m);

struct CellStats {
  int count = 0;
  int lowest = -1, highest = -1;  // cell index, from 0
  float low_mv = NAN, high_mv = NAN;
};
CellStats cell_stats(const Model &m);

// The active events at `at_least` or above, most severe first. Returns how many were written.
int top_events(const Model &m, const Event *out[], int max, Level at_least);
// The firmware's name for an event type as words ("Cell Voltages Stale"); "" if unknown
const char *event_title(uint8_t type);
// One line on what the event means; "" if unknown
const char *event_help(uint8_t type);

// ---- drawing ----

int text_base(Canvas &c, const Font &font, int x, int baseline, const char *s, Rgb color, Align align = LEFT);
// A number and its unit on one baseline. Returns the width of both.
int num_unit(Canvas &c, int x, int baseline, const Font &big, const char *number, const Font &small, const char *unit,
             Rgb number_color, Rgb unit_color, Align align = LEFT, int gap = 5);
// Text broken at spaces to fit `width`. Returns the y below the last line.
int text_wrap(Canvas &c, const Font &font, int x, int y, int width, const char *s, Rgb color, int max_lines = 4);

void card(Canvas &c, const Theme &t, int x, int y, int w, int h);
// A rounded label 34 px high; x is its left or right edge by `align`. Returns its width.
int pill(Canvas &c, int x, int y, const char *label, Rgb fg, Rgb bg, Align align = LEFT, bool dot = false);
int pill_width(const char *label, bool dot = false);
// A warning triangle with an exclamation mark, `size` pixels high
void warning_sign(Canvas &c, float cx, float cy, float size, Rgb color, Rgb mark);
// Every cell as one bar, scaled so the differences between cells show
void cell_skyline(Canvas &c, const Theme &t, const Model &m, int x, int y, int w, int h);
// Dims a region and says the BMS doesn't answer
void no_link_overlay(Canvas &c, const Theme &t, const Model &m, int x, int y, int w, int h);

// ---- navigation ----

// The pages, in the order the rail lists them
enum class Page : uint8_t { HOME, CONTROL, POWER, CELLS, HEALTH, EVENTS, SETUP, COUNT };

static const int NAV_RAIL_W = 88;
// Where a page draws: right of the rail, under the top line
static const int X0 = NAV_RAIL_W + 16, X1 = SCREEN_W - 16, Y0 = 66, Y1 = SCREEN_H - 16;

void nav_rail(Canvas &c, const Theme &t, const Model &m, Page selected);  // down the left side
// The line above every page: its title and the BMS state, or the worst active alarm
void top_line(Canvas &c, const Theme &t, const Model &m, const char *title, const char *subtitle = "");
// What every page ends with: the no-link box when the BMS is silent, and the rail
void page_end(Canvas &c, const Theme &t, const Model &m, Page selected, bool needs_bms = true);

// A caption with a number and unit under it
void stat(Canvas &c, const Theme &t, int x, int y, const char *caption, const char *number, const char *unit, Rgb color);
// A label on the left and a value on the right of one row
void row(Canvas &c, const Theme &t, int x, int y, int w, const char *label, const char *val, Rgb color = 0);
// A touch button
void button(Canvas &c, const Theme &t, int x, int y, int w, int h, const char *label, const char *note = "",
            Rgb fill = 0, Rgb text = 0);

// ---- pages: one source file each ----
void draw_home(Canvas &c, const Theme &t, const Model &m);
void draw_cells(Canvas &c, const Theme &t, const Model &m);
void draw_power(Canvas &c, const Theme &t, const Model &m);
void draw_health(Canvas &c, const Theme &t, const Model &m);
void draw_events(Canvas &c, const Theme &t, const Model &m);
void draw_control(Canvas &c, const Theme &t, const Model &m);
void draw_setup(Canvas &c, const Theme &t, const Model &m, int section);

}  // namespace hmiui
