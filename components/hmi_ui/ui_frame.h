// One frame of the UI: what a page or widget is handed to draw itself and to react to touch.
//
// The UI is "immediate mode": every frame the page's code runs top to bottom, and the same call
// that draws a button also tells whether it was just tapped:
//
//     if (button(f, x, y, w, h, "Stop")) ask_command(f.s, Command::STOP, ...);
//
// There are no widget objects and no layout tables. What must survive between frames (which
// page is open, a pop-up, a list's scroll position) lives in UiState; what the UI asks the rest
// of the firmware to do goes out through Actions.
#pragma once

#include "ui_canvas.h"
#include "ui_model.h"
#include "ui_theme.h"

namespace hmiui {

static const int SCREEN_W = 1024;
static const int SCREEN_H = 600;

// The pages, in the order the rail lists them
enum class Page : uint8_t { HOME, CONTROL, POWER, CELLS, HEALTH, EVENTS, SETUP, COUNT };

// ---- out: what the UI asks for ----

enum class Command : uint8_t {
  RUN,
  STOP,
  CALIBRATE_OFFLINE_LONG,
  CALIBRATE_OFFLINE_SHORT,
  CALIBRATE_ONLINE,
  ISOLATION_ZERO,
  BALANCE_CHARGE_START,
  BALANCE_CHARGE_STOP,
  RESTART_DISPLAY,
  WIFI_FORGET,
  WIFI_SCAN,
};
const char *command_name(Command command);

// Implemented by the ESPHome component on the device, and by a recorder in the simulator and
// the tests. The UI never does any of this itself.
class Actions {
 public:
  virtual ~Actions() {}
  virtual bool pin_ok(const char *pin) = 0;
  virtual void command(Command command) = 0;
  virtual void write_setting(int index, float value) = 0;
  virtual void set_settings_unlocked(bool unlocked) = 0;
  virtual void set_brightness(int percent) = 0;
  virtual void set_log_enabled(bool on) = 0;
  virtual void wifi_join(const char *ssid, const char *password) = 0;
};

// ---- in: the touch panel, as it is now ----

struct Input {
  bool down = false;
  int x = 0, y = 0;
};

// ---- state that outlives a frame ----

struct Scroll {
  int offset = 0;  // pixels the content is moved up
};

enum class Overlay : uint8_t { NONE, CONFIRM, PIN, NUMBER, WIFI_LIST, KEYBOARD, EVENT_DETAIL };

// What is open over the page
struct Popup {
  Overlay kind = Overlay::NONE;
  const char *title = "";    // CONFIRM: the question
  const char *text = "";     // CONFIRM: what will happen
  const char *action = "";   // CONFIRM: the button that does it
  const char *purpose = "";  // PIN: "Enter the PIN to <purpose>"
  bool danger = false;       // CONFIRM: the action interrupts the pack
  // What a correct PIN leads to
  enum After : uint8_t { NOTHING, COMMAND, COMMAND_WITHOUT_PIN, UNLOCK } after = NOTHING;
  Command command = Command::STOP;
  char entry[65] = "";       // what has been typed so far
  bool wrong = false;        // PIN: the last one entered was wrong
  bool shifted = false;      // KEYBOARD: capitals for the next letter
  bool symbols = false;      // KEYBOARD: the symbol keys instead of the letters
  bool show = false;         // KEYBOARD: the password is readable
  bool naming = false;       // KEYBOARD: typing a network's name, not yet its password
  char ssid[33] = "";        // KEYBOARD: the network being joined
  int index = -1;            // NUMBER: which setting. EVENT_DETAIL: which of the model's events
  int type = -1;             // EVENT_DETAIL: the event type, when the model has no event of it
};

struct UiState {
  Page page = Page::HOME;
  int setup_section = 0;       // Setup: 0 BMS settings, 1 display and Wi-Fi, 2 log, 3 about
  int events_view = 0;         // Events: 0 what is active, 1 every event type, 2 the fault logic chart
  bool raw_registers = false;  // Setup, Log: the registers instead of the log
  Popup popup;
  // Wrong PINs in a row. After PIN_TRIES of them no PIN is taken for PIN_LOCK_MS. Kept when
  // the UI goes back to Home by itself; a restart of the display clears it.
  static const int PIN_TRIES = 10;
  static const uint32_t PIN_LOCK_MS = 10 * 60 * 1000;
  int pin_wrong = 0;
  bool pin_locked = false;
  uint32_t pin_locked_at_ms = 0;
  Scroll events_scroll, types_scroll, settings_scroll, log_scroll, registers_scroll;
  bool log_follow = true;      // the log shows its newest lines until it is scrolled back
};

// ---- for the tests and the simulator: where each thing that can be tapped was drawn ----

struct Hit {
  char label[40];
  int x, y, w, h;
};
struct HitTrace {
  int count = 0;
  Hit hits[200];
  // The n-th thing drawn this frame with that label; nullptr if there is none
  const Hit *find(const char *label, int nth = 0) const;
};

// ---- the frame ----

struct Frame {
  Canvas &c;
  const Theme &t;
  const Model &m;
  UiState &s;
  Actions &act;
  uint32_t now_ms;

  // Touch, as the widgets see it this frame
  bool active = true;      // false: widgets draw but do not react (a pop-up is open over them)
  bool down = false;       // a finger is on the panel
  bool released = false;   // it was lifted this frame, and no widget has taken the tap yet
  bool dragging = false;   // it has moved too far from where it came down to be a tap
  int tx = 0, ty = 0;      // where it is (where it was, when lifted)
  int press_x = 0, press_y = 0;  // where it came down
  int dx = 0, dy = 0;      // how far it moved since the last frame
  // Widgets react only inside this rectangle (a scrolled list narrows it to what is visible)
  int hit_x0 = 0, hit_y0 = 0, hit_x1 = SCREEN_W, hit_y1 = SCREEN_H;
  HitTrace *trace = nullptr;
  bool changed = false;    // a widget changed the UI's state: the frame is drawn again

  // Is the finger down on this rectangle, as a press and not a drag?
  bool held(int x, int y, int w, int h) const;
  // Was this rectangle just tapped? Takes the tap, so only one widget gets it.
  bool tapped(int x, int y, int w, int h, const char *label);
  // Did the press that is going on start inside this rectangle?
  bool pressed_in(int x, int y, int w, int h) const;
};

}  // namespace hmiui
