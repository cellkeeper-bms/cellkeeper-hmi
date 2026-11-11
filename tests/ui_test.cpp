// Host tests of the LCD UI's behaviour: a finger taps its way through the pages and the tests
// check what the UI shows and what it asks the device to do.
//
//   sh tests/run_tests.sh
#include "../components/hmi_ui/ui_present.h"
#include "../sim/harness.h"
#include "../sim/scenarios.h"

#include <cstdio>
#include <cstring>

using namespace hmiui;
using sim::Driver;

static int checks = 0, failures = 0;

#define CHECK(what) \
  do { \
    checks++; \
    if (!(what)) { \
      failures++; \
      printf("FAILED %s:%d  %s\n", __FILE__, __LINE__, #what); \
    } \
  } while (0)

static Driver on_page(const sim::Scenario &s, Page page) {
  Driver d(s.model);
  d.ui.state.page = page;
  d.draw();
  return d;
}

// ---- commands ----

static void test_stop_goes_confirm_pin_send() {
  Driver d = on_page(sim::charging(), Page::CONTROL);
  CHECK(d.tap("Stop, open the contactors"));
  CHECK(d.ui.state.popup.kind == Overlay::CONFIRM);
  CHECK(d.actions.log.empty());
  CHECK(d.tap("Open the contactors"));
  CHECK(d.ui.state.popup.kind == Overlay::PIN);
  CHECK(d.type("1234"));
  CHECK(d.actions.log.empty());  // nothing is sent before OK
  CHECK(d.tap("OK"));
  CHECK(d.actions.did("command STOP"));
  CHECK(d.ui.state.popup.kind == Overlay::NONE);
  CHECK(d.actions.count("command") == 1);
}

static void test_wrong_pin_sends_nothing() {
  Driver d = on_page(sim::charging(), Page::CONTROL);
  d.tap("Stop, open the contactors");
  d.tap("Open the contactors");
  d.type("9999");
  d.tap("OK");
  CHECK(d.actions.count("command") == 0);
  CHECK(d.ui.state.popup.kind == Overlay::PIN);
  CHECK(d.ui.state.popup.wrong);
  CHECK(d.ui.state.popup.entry[0] == 0);  // the wrong digits are gone
  // A second try with the right PIN works
  d.type("1234");
  d.tap("OK");
  CHECK(d.actions.did("command STOP"));
}

static void test_cancel_and_close_send_nothing() {
  Driver d = on_page(sim::charging(), Page::CONTROL);
  d.tap("Stop, open the contactors");
  CHECK(d.tap("Cancel"));
  CHECK(d.ui.state.popup.kind == Overlay::NONE);
  d.tap("Stop, open the contactors");
  d.tap("Open the contactors");
  d.type("12");
  CHECK(d.tap("Close"));
  CHECK(d.ui.state.popup.kind == Overlay::NONE);
  CHECK(d.actions.log.empty());
}

static void test_pin_backspace_and_length() {
  Driver d = on_page(sim::charging(), Page::CONTROL);
  d.tap("Stop, open the contactors");
  d.tap("Open the contactors");
  d.type("125");
  d.tap("Backspace");
  CHECK(strcmp(d.ui.state.popup.entry, "12") == 0);
  d.type("3456789012");  // more than a PIN can hold
  CHECK(strlen(d.ui.state.popup.entry) == 8);
}

static void test_commands_follow_the_bms_state() {
  // Operating: Stop works, Run and the calibrations would be ignored by the BMS
  Driver run = on_page(sim::charging(), Page::CONTROL);
  CHECK(run.shows("Stop, open the contactors"));
  CHECK(!run.shows("Run, close the contactors"));
  CHECK(!run.shows("Isolation, zero"));
  // Inactive: the other way round
  sim::Scenario s = sim::charging();
  s.model.system_state = SystemState::INACTIVE;
  s.model.contactors_closed = false;
  Driver idle = on_page(s, Page::CONTROL);
  CHECK(idle.shows("Run, close the contactors"));
  CHECK(!idle.shows("Stop, open the contactors"));
  CHECK(idle.tap("Current sensor, offline, long"));
  idle.tap("Calibrate");
  idle.type("1234");
  idle.tap("OK");
  CHECK(idle.actions.did("command CALIBRATE_OFFLINE_LONG"));
  // FAULT: neither
  Driver fault = on_page(sim::desk_fault(), Page::CONTROL);
  CHECK(!fault.shows("Run, close the contactors"));
  CHECK(!fault.shows("Stop, open the contactors"));
  CHECK(fault.shows("Start now"));  // the balance charge is the display's own
  // No link: nothing at all reacts under the "no answer" box except the rail
  Driver dead = on_page(sim::no_link(), Page::CONTROL);
  CHECK(!dead.shows("Start now"));
  CHECK(dead.shows("Home"));
}

static void test_two_buttons_with_one_label() {
  // "Stop" for the contactors and "Stop" for the balance charge are different buttons
  Driver d = on_page(sim::charging(), Page::CONTROL);
  CHECK(d.tap("Stop"));
  CHECK(d.ui.state.popup.command == Command::BALANCE_CHARGE_STOP);
}

// ---- touch ----

static void test_a_drag_is_not_a_tap() {
  Driver d = on_page(sim::charging(), Page::CONTROL);
  const Hit *stop = d.trace.find("Stop, open the contactors");
  CHECK(stop != nullptr);
  if (stop == nullptr) return;
  const int x = stop->x + 20, y = stop->y + 20;
  d.drag(x, y, x + 60, y);  // still on the button when lifted, but it moved
  CHECK(d.ui.state.popup.kind == Overlay::NONE);
  // A finger that only wobbles a little is still tapping. (The panel gives no position when
  // the finger lifts, so the tap is where it last was.)
  d.step(true, x, y);
  d.step(true, x + 6, y - 5);
  d.step(false, 0, 0);
  CHECK(d.ui.state.popup.kind == Overlay::CONFIRM);
}

static void test_the_page_does_not_react_under_a_popup() {
  Driver d = on_page(sim::charging(), Page::CONTROL);
  const Hit *start = d.trace.find("Start now");
  CHECK(start != nullptr);
  if (start == nullptr) return;
  const int x = start->x + 10, y = start->y + 10;
  d.tap("Stop, open the contactors");
  CHECK(!d.shows("Start now"));
  d.tap_at(x, y);  // where "Start now" is, behind the dimmed page
  CHECK(d.ui.state.popup.kind == Overlay::CONFIRM);
  CHECK(d.ui.state.popup.command == Command::STOP);
  // And the rail does not change the page either
  d.tap_at(40, 300);
  CHECK(d.ui.state.page == Page::CONTROL);
}

static void test_one_tap_reaches_one_widget() {
  // The confirm box's buttons lie where page buttons are: the tap that opens the box must
  // not also press what appears under the finger
  Driver d = on_page(sim::charging(), Page::CONTROL);
  for (int i = 0; i < 3; i++) {
    d.tap("Stop, open the contactors");
    CHECK(d.ui.state.popup.kind == Overlay::CONFIRM);
    d.tap("Cancel");
    CHECK(d.ui.state.popup.kind == Overlay::NONE);
  }
}

// ---- navigation ----

static void test_rail_and_links() {
  Driver d(sim::charging().model);
  CHECK(d.ui.state.page == Page::HOME);
  static const char *const NAMES[] = {"Home", "Control", "Power", "Cells", "Health", "Events", "Setup"};
  for (int i = (int) Page::COUNT - 1; i >= 0; i--) {
    CHECK(d.tap(NAMES[i]));
    CHECK((int) d.ui.state.page == i);
  }
  CHECK(d.tap("All cells"));
  CHECK(d.ui.state.page == Page::CELLS);
  // The alarm strip leads to the events
  Driver alarm(sim::desk_fault().model);
  CHECK(alarm.tap("Alarm"));
  CHECK(alarm.ui.state.page == Page::EVENTS);
}

static void test_events_views() {
  Driver d = on_page(sim::desk_fault(), Page::EVENTS);
  CHECK(d.tap("How a fault happens"));
  CHECK(d.ui.state.events_view == 2);
  CHECK(d.tap("‹  Active"));
  CHECK(d.tap("All event types"));
  CHECK(d.ui.state.events_view == 1);
  // Leaving the page and coming back opens the list of active events again
  d.tap("Home");
  d.tap("Events");
  CHECK(d.ui.state.events_view == 0);
  CHECK(d.tap("BMB Read Error"));
  CHECK(d.ui.state.popup.kind == Overlay::EVENT_DETAIL);
  CHECK(d.model.events[d.ui.state.popup.index].count == 37283);
  d.tap("Close");
  CHECK(d.ui.state.popup.kind == Overlay::NONE);
}

static void test_idle_returns_home() {
  Driver d = on_page(sim::charging(), Page::CONTROL);
  d.tap("Stop, open the contactors");
  d.now += Ui::IDLE_MS - 1000;
  d.draw();
  CHECK(d.ui.state.page == Page::CONTROL);
  CHECK(!d.ui.idle(d.now));
  d.now += 2000;
  d.draw();
  CHECK(d.ui.idle(d.now));
  CHECK(d.ui.state.page == Page::HOME);
  CHECK(d.ui.state.popup.kind == Overlay::NONE);
  d.tap("Cells");
  CHECK(!d.ui.idle(d.now));
}

// ---- settings ----

static void test_settings_need_the_pin() {
  Driver d = on_page(sim::charging(), Page::SETUP);
  CHECK(!d.shows("Max charge current"));  // locked: values are not buttons
  CHECK(d.tap("Unlock"));
  d.type("1234");
  d.tap("OK");
  CHECK(d.actions.did("unlock settings"));
  CHECK(d.shows("Max charge current"));
  CHECK(d.tap("Max charge current"));
  CHECK(d.ui.state.popup.kind == Overlay::NUMBER);
  d.type("85");
  CHECK(d.tap("Set"));
  CHECK(d.actions.did("setting 0 = 85"));
  CHECK(d.tap("Lock"));
  CHECK(d.actions.did("lock settings"));
  CHECK(!d.shows("Max charge current"));
}

static void test_number_entry() {
  Driver d = on_page(sim::charging(), Page::SETUP);
  d.model.settings_unlocked = true;
  d.draw();
  d.tap("Max charge current");  // allowed 0 to 150 A, one decimal
  CHECK(!d.shows("Set"));       // nothing typed yet
  d.type("999");
  CHECK(!d.shows("Set"));       // out of range
  d.tap("Backspace");
  d.tap("Backspace");
  d.type("7.5.5");              // a second point is ignored
  CHECK(strcmp(d.ui.state.popup.entry, "97.55") == 0);
  d.tap("±");
  CHECK(strcmp(d.ui.state.popup.entry, "-97.55") == 0);
  CHECK(!d.shows("Set"));       // below the range
  d.tap("±");
  CHECK(d.tap("Set"));
  CHECK(d.actions.count("setting 0 = 97.5") == 1);
  // A setting without decimals takes no point
  d.tap("SoC scaling, top");
  d.type("9.5");
  CHECK(strcmp(d.ui.state.popup.entry, "95") == 0);
  d.tap("Cancel");
  CHECK(d.actions.count("setting") == 1);
}

// ---- display and Wi-Fi ----

static void test_brightness_slider() {
  Driver d = on_page(sim::charging(), Page::SETUP);
  d.ui.state.setup_section = 1;
  d.draw();
  const Hit *track = d.trace.find("Brightness");
  CHECK(track != nullptr);
  if (track == nullptr) return;
  const int y = track->y + track->h / 2;
  d.drag(track->x + track->w / 2, y, track->x + track->w - 22, y);
  CHECK(d.model.brightness >= 95);
  d.drag(track->x + track->w / 2, y, track->x - 100, y);
  CHECK(d.model.brightness == 5);  // never fully dark
}

static void test_wifi_join() {
  Driver d = on_page(sim::charging(), Page::SETUP);
  d.ui.state.setup_section = 1;
  d.draw();
  CHECK(d.tap("Set up a network"));
  CHECK(d.tap("Hakalau Farm"));  // secured: asks for the password
  CHECK(d.ui.state.popup.kind == Overlay::KEYBOARD);
  CHECK(!d.shows("Join"));       // nothing typed
  d.tap("Shift");
  d.type("Mauka");               // shift lasts one letter
  d.tap("#+=");
  d.type("-2!");
  d.tap("abc");
  d.type("x");
  d.tap("Backspace");
  CHECK(d.tap("Join"));
  CHECK(d.actions.did("join Hakalau Farm / Mauka-2!"));
  // An open network joins at once
  d.tap("Set up a network");
  d.tap("LANZ-guest");
  CHECK(d.actions.did("join LANZ-guest / "));
  // A network that is not listed: its name, then its password
  d.tap("Set up a network");
  d.tap("Another network...");
  d.type("barn");
  d.tap("Next");
  d.type("hay");
  d.tap("Join");
  CHECK(d.actions.did("join barn / hay"));
}

static void test_plain_confirm_needs_no_pin() {
  Driver d = on_page(sim::charging(), Page::SETUP);
  d.ui.state.setup_section = 1;
  d.draw();
  d.tap("Restart the display");
  CHECK(d.ui.state.popup.kind == Overlay::CONFIRM);
  d.tap("Restart");
  CHECK(d.actions.did("command RESTART_DISPLAY"));
  CHECK(d.actions.count("pin") == 0);
}

static void test_log_and_registers() {
  Driver d = on_page(sim::charging(), Page::SETUP);
  CHECK(d.tap("Log"));
  CHECK(d.tap("Log: on"));
  CHECK(d.actions.did("log off"));
  CHECK(d.tap("Raw registers"));
  CHECK(d.ui.state.raw_registers);
  CHECK(d.tap("‹  Log"));
  CHECK(!d.ui.state.raw_registers);
}

// ---- scrolling ----

static void test_event_list_scrolls() {
  using namespace esphome::hmi_bms;
  sim::Scenario s = sim::charging();
  for (int type = 20; type < 34; type++) sim::add_event(s.model, type, Level::WARNING, 1);
  Driver d = on_page(s, Page::EVENTS);
  CHECK(d.ui.state.events_scroll.offset == 0);
  d.drag(400, 400, 400, 150);
  const int moved = d.ui.state.events_scroll.offset;
  CHECK(moved > 200);
  d.drag(400, 400, 400, -3000);  // far past the end
  const int end = d.ui.state.events_scroll.offset;
  CHECK(end >= moved);
  d.drag(400, 400, 400, 380);    // a little further: it stays at the end
  CHECK(d.ui.state.events_scroll.offset <= end);
  d.drag(400, 200, 400, 5000);   // back past the start
  CHECK(d.ui.state.events_scroll.offset == 0);
  // The drag did not open an event's detail
  CHECK(d.ui.state.popup.kind == Overlay::NONE);
  // A short list does not scroll
  Driver few = on_page(sim::charging(), Page::EVENTS);
  few.drag(400, 400, 400, 150);
  CHECK(few.ui.state.events_scroll.offset == 0);
}

// ---- every page, every scenario: it draws, and nothing it draws reaches outside ----

static void test_everything_draws() {
  const sim::Scenario all[] = {sim::charging(),  sim::discharging(),      sim::idle(),
                               sim::precharge(), sim::lfp(),              sim::fault_with_cells(),
                               sim::desk_fault(), sim::wrong_protocol(), sim::no_link()};
  for (const auto &s : all) {
    for (int p = 0; p < (int) Page::COUNT; p++) {
      for (int section = 0; section < ((Page) p == Page::SETUP ? 4 : 1); section++) {
        for (int view = 0; view < ((Page) p == Page::EVENTS ? 3 : 1); view++) {
          Driver d(s.model);
          d.ui.state.page = (Page) p;
          d.ui.state.setup_section = section;
          d.ui.state.events_view = view;
          d.draw();
          checks++;
          for (int i = 0; i < d.trace.count; i++) {
            const Hit &h = d.trace.hits[i];
            if (h.x < 0 || h.y < 0 || h.x + h.w > SCREEN_W || h.y + h.h > SCREEN_H || h.w <= 0 || h.h <= 0) {
              failures++;
              printf("FAILED: \"%s\" is off the screen on page %d of %s\n", h.label, p, s.name);
            }
          }
        }
      }
    }
  }
}

// ---- lists that are longer than the screen ----

static void test_all_settings_can_be_reached() {
  Driver d = on_page(sim::charging(), Page::SETUP);
  d.model.settings_unlocked = true;
  d.draw();
  CHECK(d.shows("Max charge current"));
  CHECK(!d.shows("Upper offset"));  // the last one is below the edge
  d.drag(500, 520, 500, 250);
  CHECK(d.ui.state.settings_scroll.offset > 0);
  CHECK(d.shows("Upper offset"));
  CHECK(d.tap("Upper offset"));
  d.type("3");
  d.tap("Set");
  CHECK(d.actions.did("setting 15 = 3"));
  // Every setting can be brought into view, at the top or at the end of the list
  d.drag(500, 300, 500, 3000);
  int seen = 0;
  for (int i = 0; i < d.model.setting_count; i++) seen += d.shows(d.model.settings[i].name);
  d.drag(500, 520, 500, -3000);
  int at_end = 0;
  for (int i = 0; i < d.model.setting_count; i++) at_end += d.shows(d.model.settings[i].name);
  CHECK(seen < d.model.setting_count);
  CHECK(seen + at_end >= d.model.setting_count);
  // Locked, the list still scrolls but its values are not buttons
  d.model.settings_unlocked = false;
  d.draw();
  CHECK(!d.shows("Upper offset"));
}

static void test_log_follows_until_scrolled_back() {
  static char text[60][24];
  static const char *lines[60];
  for (int i = 0; i < 60; i++) {
    snprintf(text[i], sizeof(text[i]), "line %d", i);
    lines[i] = text[i];
  }
  sim::Scenario s = sim::charging();
  s.model.log_lines = lines;
  s.model.log_count = 50;
  Driver d = on_page(s, Page::SETUP);
  d.ui.state.setup_section = 2;
  d.draw();
  CHECK(d.ui.state.log_follow);
  const int end = d.ui.state.log_scroll.offset;
  CHECK(end > 0);
  d.model.log_count = 60;  // more lines arrive: it stays at the end
  d.draw();
  CHECK(d.ui.state.log_scroll.offset > end);
  d.drag(500, 300, 500, 480);  // pulled down to read older lines
  CHECK(!d.ui.state.log_follow);
  const int back = d.ui.state.log_scroll.offset;
  d.draw();
  CHECK(d.ui.state.log_scroll.offset == back);  // and it stays there
  d.drag(500, 480, 500, -3000);
  CHECK(d.ui.state.log_follow);
}

static void test_registers_scroll() {
  static hmiui::RegisterRow rows[140];
  for (int i = 0; i < 140; i++) rows[i] = {(uint16_t) (0x1000 + i), "REGISTER", "0"};
  sim::Scenario s = sim::charging();
  s.model.registers = rows;
  s.model.register_count = 140;
  Driver d = on_page(s, Page::SETUP);
  d.ui.state.setup_section = 2;
  d.ui.state.raw_registers = true;
  d.draw();
  d.drag(500, 500, 500, 200);
  CHECK(d.ui.state.registers_scroll.offset > 200);
  d.drag(500, 500, 500, -9000);
  const int end = d.ui.state.registers_scroll.offset;
  CHECK(end > 0 && end < 70 * 21);  // it stops at the last row
}

static void test_every_event_type_tells_more() {
  using namespace esphome::hmi_bms;
  Driver d = on_page(sim::desk_fault(), Page::EVENTS);
  d.ui.state.events_view = 1;
  d.draw();
  CHECK(d.tap("type BMB_READ_ERROR"));  // one the BMS reports
  CHECK(d.ui.state.popup.kind == Overlay::EVENT_DETAIL);
  CHECK(d.ui.state.popup.index == 4);
  d.tap("Close");
  CHECK(d.tap("type ESTOP_PRESSED"));  // one that never happened
  CHECK(d.ui.state.popup.kind == Overlay::EVENT_DETAIL);
  CHECK(d.ui.state.popup.index < 0 && d.ui.state.popup.type == HMI_EVENT_ESTOP_PRESSED);
  d.tap("Close");
  CHECK(d.ui.state.popup.kind == Overlay::NONE);
  CHECK(d.shows("type INVERTER_DETECTED"));  // the last one is on the screen
}

static void test_home_fault_list() {
  Driver d(sim::desk_fault().model);
  CHECK(d.tap("Cell Voltages Stale"));
  CHECK(d.ui.state.popup.kind == Overlay::EVENT_DETAIL);
  d.tap("Close");
  CHECK(d.tap("More events"));
  CHECK(d.ui.state.page == Page::EVENTS);
}

// ---- what is sent to the display ----

static void test_changed_bands() {
  const int w = 16, h = 100;
  std::vector<uint16_t> shown((size_t) w * h, 0), drawn((size_t) w * h, 0);
  Band bands[4];
  CHECK(changed_bands(drawn.data(), shown.data(), w, h, bands, 4) == 0);
  auto touch = [&](int y) { drawn[(size_t) y * w + 3] = 0xFFFF; };
  touch(10);
  touch(11);
  touch(15);  // close to the rows above: one band
  touch(60);  // far away: its own band
  CHECK(changed_bands(drawn.data(), shown.data(), w, h, bands, 4) == 2);
  CHECK(bands[0].y0 == 10 && bands[0].y1 == 16);
  CHECK(bands[1].y0 == 60 && bands[1].y1 == 61);
  // More separate changes than there is room for: the last band takes the rest
  touch(0);
  touch(30);
  touch(99);
  CHECK(changed_bands(drawn.data(), shown.data(), w, h, bands, 3) == 3);
  CHECK(bands[0].y0 == 0 && bands[0].y1 == 16);  // row 0 is close enough to rows 10 to 15
  CHECK(bands[1].y0 == 30 && bands[1].y1 == 31);
  CHECK(bands[2].y0 == 60 && bands[2].y1 == 100);
  // Every changed row is inside a band
  const int n = changed_bands(drawn.data(), shown.data(), w, h, bands, 4);
  for (int y = 0; y < h; y++) {
    bool differs = false, covered = false;
    for (int x = 0; x < w; x++) differs = differs || drawn[(size_t) y * w + x] != shown[(size_t) y * w + x];
    for (int i = 0; i < n; i++) covered = covered || (y >= bands[i].y0 && y < bands[i].y1);
    CHECK(!differs || covered);
  }
}

// A real pair of frames: a tap changes little of the screen
static void test_a_tap_changes_few_rows() {
  Driver d = on_page(sim::charging(), Page::CONTROL);
  const std::vector<uint16_t> before = d.pixels;
  d.model.balance_charge_status = "Running, 12 min left";
  d.draw();
  Band bands[8];
  const int n = changed_bands(d.pixels.data(), before.data(), SCREEN_W, SCREEN_H, bands, 8);
  int rows = 0;
  for (int i = 0; i < n; i++) rows += bands[i].y1 - bands[i].y0;
  CHECK(n >= 1);
  CHECK(rows < 60);
}

// ---- drawing only when the frame would change ----

static void test_a_frame_is_drawn_only_when_it_changes() {
  Driver d = on_page(sim::charging(), Page::HOME);
  Input none;
  CHECK(!d.ui.frame(d.canvas, d.model, none, d.actions, d.now));  // nothing changed since on_page drew it
  const std::vector<uint16_t> before = d.pixels;
  d.model.current += 0.1f;
  CHECK(d.ui.frame(d.canvas, d.model, none, d.actions, d.now));
  CHECK(d.pixels != before);
  CHECK(!d.ui.frame(d.canvas, d.model, none, d.actions, d.now));
  // A value the page does not show changes nothing
  d.model.v3v3 += 0.01f;
  CHECK(!d.ui.frame(d.canvas, d.model, none, d.actions, d.now));
  // A text that changes in place (same pointer) is noticed
  static char status[32] = "Off";
  d.ui.state.page = Page::CONTROL;
  d.model.balance_charge_status = status;
  CHECK(d.ui.frame(d.canvas, d.model, none, d.actions, d.now));
  snprintf(status, sizeof(status), "Running");
  CHECK(d.ui.frame(d.canvas, d.model, none, d.actions, d.now, &d.trace));
  // A finger coming down on a button changes how it looks
  const Hit *stop = d.trace.find("Stop");
  CHECK(stop != nullptr);
  if (stop != nullptr) {
    Input finger;
    finger.down = true;
    finger.x = stop->x + 10;
    finger.y = stop->y + 10;
    CHECK(d.ui.frame(d.canvas, d.model, finger, d.actions, d.now));
    CHECK(!d.ui.frame(d.canvas, d.model, finger, d.actions, d.now));  // held still: the same frame
  }
  // After redraw() it draws again whatever it finds
  d.ui.redraw();
  CHECK(d.ui.frame(d.canvas, d.model, none, d.actions, d.now));
}

// The dry run must draw nothing, and sum up the same calls to the same numbers
static void some_drawing(Canvas &canvas, const char *text) {
  canvas.clear(0xFFFFFF);
  canvas.fill(0, 0, 100, 100, 0xFF0000);
  canvas.round_rect(10, 10, 80, 80, 8, 0x00FF00);
  canvas.ring(100, 100, 20, 30, 0, 270, 0x0000FF, true);
  canvas.line(0, 0, 200, 200, 4, 0xFFFFFF);
  canvas.triangle(5, 5, 50, 5, 25, 40, 0xFFFFFF);
  canvas.text(FONT_M24, 20, 300, text, 0xFFFFFF);
}

static void test_the_dry_run_draws_nothing() {
  std::vector<uint16_t> pixels((size_t) SCREEN_W * SCREEN_H, 0x1234);
  Canvas canvas(pixels.data(), SCREEN_W, SCREEN_H);
  canvas.begin_dry();
  some_drawing(canvas, "12.5 V");
  const std::vector<uint32_t> first(canvas.end_dry(), canvas.end_dry() + canvas.bands());
  bool untouched = true;
  for (const uint16_t px : pixels) untouched = untouched && px == 0x1234;
  CHECK(untouched);
  canvas.begin_dry();
  CHECK(canvas.text(FONT_M24, 20, 20, "12.5 V", 0xFFFFFF) == Canvas::text_width(FONT_M24, "12.5 V"));
  canvas.end_dry();
  // The same calls give the same numbers
  canvas.begin_dry();
  some_drawing(canvas, "12.5 V");
  const std::vector<uint32_t> again(canvas.end_dry(), canvas.end_dry() + canvas.bands());
  CHECK(first == again);
  // Another text changes the bands the text is in, and no other
  canvas.begin_dry();
  some_drawing(canvas, "12.6 V");
  const uint32_t *other = canvas.end_dry();
  int changed = 0;
  for (int band = 0; band < canvas.bands(); band++) {
    if (other[band] == first[band]) continue;
    changed++;
    CHECK(band >= (300 - 2) / Canvas::BAND_ROWS && band <= (300 + FONT_M24.line_height() + 2) / Canvas::BAND_ROWS);
  }
  CHECK(changed >= 1);
}

// Drawing only the bands that changed must leave exactly the frame a full draw gives
static bool same_as_a_full_draw(Driver &d) {
  Driver fresh(d.model);
  fresh.ui.state = d.ui.state;
  fresh.now = d.now;
  fresh.ui.redraw();
  fresh.draw();
  return d.pixels == fresh.pixels;
}

static void test_drawing_in_bands_matches_a_full_draw() {
  const sim::Scenario all[] = {sim::charging(), sim::idle(), sim::lfp(), sim::fault_with_cells(), sim::desk_fault(), sim::no_link()};
  for (const auto &s : all) {
    for (int p = 0; p < (int) Page::COUNT; p++) {
      Driver d = on_page(s, (Page) p);
      // Values change
      d.model.current += 3.7f;
      d.model.power = d.model.pack_voltage * d.model.current;
      d.model.soc -= 4.0f;
      d.model.v5 -= 0.3f;
      d.model.cell_mv[7] += 30.0f;
      d.model.clock = "23:59";
      d.model.balance_charge_status = "Balance charging, 2h05m left";
      d.draw();
      CHECK(same_as_a_full_draw(d));
      // An alarm appears and takes the top line
      sim::add_event(d.model, esphome::hmi_bms::HMI_EVENT_CELL_VOLTAGE_HIGH, Level::WARNING, 3);
      d.draw();
      CHECK(same_as_a_full_draw(d));
      // The link goes
      d.model.link = Link::NO_DATA;
      d.draw();
      CHECK(same_as_a_full_draw(d));
    }
  }
  // Pop-ups opening, being typed in and closing
  Driver d = on_page(sim::charging(), Page::CONTROL);
  d.tap("Stop, open the contactors");
  CHECK(same_as_a_full_draw(d));
  d.tap("Open the contactors");
  d.type("12");
  CHECK(same_as_a_full_draw(d));
  d.tap("Close");
  CHECK(same_as_a_full_draw(d));
  // Pages, sections, a scrolled list
  d.tap("Setup");
  CHECK(same_as_a_full_draw(d));
  d.drag(500, 520, 500, 300);
  CHECK(d.ui.state.settings_scroll.offset > 0);
  CHECK(same_as_a_full_draw(d));
  d.tap("Display and Wi-Fi");
  CHECK(same_as_a_full_draw(d));
  d.tap("Events");
  d.tap("All event types");
  CHECK(same_as_a_full_draw(d));
  d.tap("Home");
  CHECK(same_as_a_full_draw(d));
  // A press on a button draws few rows again, not the frame
  Driver c = on_page(sim::charging(), Page::CONTROL);
  const Hit *start = c.trace.find("Start now");
  CHECK(start != nullptr);
  if (start != nullptr) {
    c.step(true, start->x + 10, start->y + 10);
    int rows = 0;
    for (int i = 0; i < c.ui.redrawn_count; i++) rows += c.ui.redrawn[i].y1 - c.ui.redrawn[i].y0;
    CHECK(rows > 0 && rows <= start->h + 2 * Canvas::BAND_ROWS);
    c.step(false, 0, 0);
  }
}

static void test_changed_bands_by_row_sums() {
  const int w = 15, h = 60;  // an odd width: the last pixel of a row counts too
  std::vector<uint16_t> frame((size_t) w * h, 0);
  std::vector<uint32_t> sums((size_t) h, 0);
  Band bands[4];
  // Before anything was shown, everything is new
  CHECK(changed_bands(frame.data(), sums.data(), w, h, bands, 4) == 1);
  CHECK(bands[0].y0 == 0 && bands[0].y1 == h);
  CHECK(changed_bands(frame.data(), sums.data(), w, h, bands, 4) == 0);
  frame[(size_t) 20 * w + 14] = 1;  // the last pixel of row 20
  frame[(size_t) 50 * w] = 1;
  CHECK(changed_bands(frame.data(), sums.data(), w, h, bands, 4) == 2);
  CHECK(bands[0].y0 == 20 && bands[0].y1 == 21);
  CHECK(bands[1].y0 == 50 && bands[1].y1 == 51);
  CHECK(changed_bands(frame.data(), sums.data(), w, h, bands, 4) == 0);
  // Two pixels swapped within a row is a change
  frame[(size_t) 5 * w + 2] = 7;
  changed_bands(frame.data(), sums.data(), w, h, bands, 4);
  frame[(size_t) 5 * w + 2] = 0;
  frame[(size_t) 5 * w + 3] = 7;
  CHECK(changed_bands(frame.data(), sums.data(), w, h, bands, 4) == 1);
}

// ---- too many wrong PINs ----

static void wrong_pins(Driver &d, int times) {
  for (int i = 0; i < times; i++) {
    d.type("0000");
    d.tap("OK");
  }
}

static void test_pin_lockout() {
  Driver d = on_page(sim::charging(), Page::CONTROL);
  d.tap("Stop, open the contactors");
  d.tap("Open the contactors");
  wrong_pins(d, 9);
  CHECK(!d.ui.state.pin_locked);
  CHECK(d.ui.state.pin_wrong == 9);
  wrong_pins(d, 1);  // the tenth
  CHECK(d.ui.state.pin_locked);
  CHECK(d.actions.count("pin wrong") == 10);
  // Locked: the right PIN is not even looked at
  d.type("1234");
  d.tap("OK");
  CHECK(d.actions.count("pin ok") == 0);
  CHECK(d.actions.count("command") == 0);
  CHECK(d.ui.state.popup.entry[0] == 0);
  // Closing the box, opening another, or the UI going back to Home does not help
  d.tap("Close");
  d.tap("Setup");
  d.tap("Unlock");
  d.type("1234");
  d.tap("OK");
  CHECK(d.actions.count("pin ok") == 0);
  d.now += Ui::IDLE_MS + 1000;
  d.draw();
  CHECK(d.ui.state.page == Page::HOME);
  CHECK(d.ui.state.pin_locked);
  // Ten minutes after the tenth wrong PIN it takes PINs again, with a fresh count
  d.now += UiState::PIN_LOCK_MS;
  d.tap("Setup");
  d.tap("Unlock");
  d.type("1234");
  d.tap("OK");
  CHECK(d.actions.did("unlock settings"));
  CHECK(!d.ui.state.pin_locked);
  CHECK(d.ui.state.pin_wrong == 0);
}

static void test_a_right_pin_clears_the_count() {
  Driver d = on_page(sim::charging(), Page::CONTROL);
  d.tap("Stop");
  d.tap("Stop the balance charge");
  wrong_pins(d, 9);
  d.type("1234");
  d.tap("OK");
  CHECK(d.actions.did("command BALANCE_CHARGE_STOP"));
  CHECK(d.ui.state.pin_wrong == 0);
  d.tap("Stop");
  d.tap("Stop the balance charge");
  wrong_pins(d, 9);
  CHECK(!d.ui.state.pin_locked);  // nine again, not eighteen
}

int main() {
  test_stop_goes_confirm_pin_send();
  test_wrong_pin_sends_nothing();
  test_cancel_and_close_send_nothing();
  test_pin_backspace_and_length();
  test_commands_follow_the_bms_state();
  test_two_buttons_with_one_label();
  test_a_drag_is_not_a_tap();
  test_the_page_does_not_react_under_a_popup();
  test_one_tap_reaches_one_widget();
  test_rail_and_links();
  test_events_views();
  test_idle_returns_home();
  test_settings_need_the_pin();
  test_number_entry();
  test_brightness_slider();
  test_wifi_join();
  test_plain_confirm_needs_no_pin();
  test_log_and_registers();
  test_event_list_scrolls();
  test_all_settings_can_be_reached();
  test_log_follows_until_scrolled_back();
  test_registers_scroll();
  test_every_event_type_tells_more();
  test_home_fault_list();
  test_changed_bands();
  test_a_tap_changes_few_rows();
  test_a_frame_is_drawn_only_when_it_changes();
  test_the_dry_run_draws_nothing();
  test_drawing_in_bands_matches_a_full_draw();
  test_changed_bands_by_row_sums();
  test_pin_lockout();
  test_a_right_pin_clears_the_count();
  test_everything_draws();
  printf("ui tests: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
