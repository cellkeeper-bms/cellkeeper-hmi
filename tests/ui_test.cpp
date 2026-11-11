// Host tests of the LCD UI's behaviour: a finger taps its way through the pages and the tests
// check what the UI shows and what it asks the device to do.
//
//   sh tests/run_tests.sh
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
  test_everything_draws();
  printf("ui tests: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
