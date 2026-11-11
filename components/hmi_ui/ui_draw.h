// What the pages share: text and number helpers, the values worked out from the model, the
// small widgets (card, pill, cell chart), the navigation and the pages' entry points.
#pragma once

#include "ui_canvas.h"
#include "ui_fonts.h"
#include "ui_frame.h"
#include "ui_model.h"
#include "ui_theme.h"

namespace hmiui {

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
// The firmware's own name ("CELL_VOLTAGES_STALE"); "" if unknown
const char *event_name(uint8_t type);
// How many event types the firmware has
int event_type_count();

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

static const int NAV_RAIL_W = 88;
// Where a page draws: right of the rail, under the top line
static const int X0 = NAV_RAIL_W + 16, X1 = SCREEN_W - 16, Y0 = 66, Y1 = SCREEN_H - 16;

void nav_rail(Frame &f, Page selected);  // down the left side; a tap on it changes the page
// The line above every page: its title and the BMS state, or the worst active alarm
void top_line(Frame &f, const char *title, const char *subtitle = "");
// What every page ends with: the no-link box when the BMS is silent, and the rail
void page_end(Frame &f, Page selected, bool needs_bms = true);

// A caption with a number and unit under it
void stat(Canvas &c, const Theme &t, int x, int y, const char *caption, const char *number, const char *unit, Rgb color);
// A label on the left and a value on the right of one row
void row(Canvas &c, const Theme &t, int x, int y, int w, const char *label, const char *val, Rgb color = 0);
// A padlock, open or closed
void padlock(Canvas &c, float cx, float cy, bool open, Rgb color);
// How far a list is scrolled: a thin bar with a thumb
void scrollbar(Canvas &c, const Theme &t, int x, int y, int h, float start, float shown);

// ---- widgets: they draw themselves and return what the touch did (ui_widgets.cpp) ----

// A button. True when it was tapped. A disabled one is drawn dim and never reacts.
bool button(Frame &f, int x, int y, int w, int h, const char *label, const char *note = "", Rgb fill = 0, Rgb text = 0,
            bool enabled = true);
// An area that can be tapped but draws nothing itself (a list row, a link). `label` names it
// for the tests.
bool hot(Frame &f, int x, int y, int w, int h, const char *label);
// A row of choices, one selected. True when the selection changed.
bool tabs(Frame &f, int x, int y, int w, int h, const char *const *labels, int count, int &selected);
// A slider from 0 to 100 with its track's centre line at y. True while the finger moves it.
bool slider(Frame &f, int x, int y, int w, int &percent, const char *label);
// A list that can be dragged up and down. Draw the content between the two calls, starting at
// the y that scroll_begin returns; tell scroll_end how tall the content was.
int scroll_begin(Frame &f, Scroll &s, int x, int y, int w, int h);
void scroll_end(Frame &f, Scroll &s, int x, int y, int w, int h, int content_h);

// ---- pop-ups: opened by a page, drawn over it, closed by themselves (ui_popups.cpp) ----

// Asks "are you sure", then for the PIN, then sends the command
void ask_command(UiState &s, Command command, const char *title, const char *text, const char *action, const char *purpose,
                 bool danger);
// Asks "are you sure" and sends the command: for what does not touch the pack
void ask_plain(UiState &s, Command command, const char *title, const char *text, const char *action);
// Asks for the PIN, then unlocks the BMS settings
void ask_unlock(UiState &s);
// The number pad for one BMS setting
void open_number(UiState &s, int setting);
// More about one of the model's events
void open_event(UiState &s, int event);
// More about an event type, whether or not it ever happened
void open_event_type(UiState &s, const Model &m, int type);
// The list of Wi-Fi networks, then the keyboard for the password
void open_wifi(UiState &s);
void close_popup(UiState &s);
void draw_popup(Frame &f);

// ---- pages: one source file each ----
void draw_home(Frame &f);
void draw_cells(Frame &f);
void draw_power(Frame &f);
void draw_health(Frame &f);
void draw_events(Frame &f);
void draw_control(Frame &f);
void draw_setup(Frame &f);

}  // namespace hmiui
