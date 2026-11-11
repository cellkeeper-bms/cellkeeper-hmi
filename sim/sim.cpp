// PC simulator of the LCD UI: draws the pages with the same code the display runs and writes
// them as image files, one per scenario and page.
//
//   sh sim/run.sh            builds this, runs it, and leaves PNG files in sim/out/
//
// It writes PPM files (the simplest image format there is); run.sh turns them into PNG.
#include "../components/hmi_ui/ui.h"
#include "scenarios.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace hmiui;

static bool write_ppm(const std::string &path, const Canvas &c) {
  FILE *f = fopen(path.c_str(), "wb");
  if (f == nullptr) return false;
  fprintf(f, "P6\n%d %d\n255\n", c.width(), c.height());
  std::vector<uint8_t> row((size_t) c.width() * 3);
  for (int y = 0; y < c.height(); y++) {
    for (int x = 0; x < c.width(); x++) {
      // As the panel shows a 565 pixel
      const Rgb p = from565(c.pixels()[(size_t) y * c.width() + x]);
      row[(size_t) x * 3] = (uint8_t) (p >> 16);
      row[(size_t) x * 3 + 1] = (uint8_t) (p >> 8);
      row[(size_t) x * 3 + 2] = (uint8_t) p;
    }
    fwrite(row.data(), 1, row.size(), f);
  }
  fclose(f);
  return true;
}

int main(int argc, char **argv) {
  const std::string out = argc > 1 ? argv[1] : "sim/out";
  std::vector<uint16_t> frame((size_t) SCREEN_W * SCREEN_H);
  Canvas canvas(frame.data(), SCREEN_W, SCREEN_H);

  const sim::Scenario scenarios[] = {sim::charging(),         sim::discharging(), sim::idle(),
                                     sim::precharge(),        sim::lfp(),         sim::fault_with_cells(),
                                     sim::desk_fault(),       sim::wrong_protocol(), sim::no_link()};
  // What to draw: the page, the name its files get, for Setup which section, and anything
  // open over or instead of the page. A view with `only` is drawn for that scenario alone.
  enum Extra { PLAIN, LOGIC, ALL_TYPES, REGISTERS, UNLOCKED, CONFIRM_STOP, PIN, PIN_WRONG, NUMBER, WIFI_LIST, KEYBOARD, EVENT };
  struct View {
    Page page;
    const char *name;
    int section;
    Extra extra;
    const char *only;
  };
  static const View VIEWS[] = {
      {Page::HOME, "home", 0, PLAIN, nullptr},
      {Page::CONTROL, "control", 0, PLAIN, nullptr},
      {Page::POWER, "power", 0, PLAIN, nullptr},
      {Page::CELLS, "cells", 0, PLAIN, nullptr},
      {Page::HEALTH, "health", 0, PLAIN, nullptr},
      {Page::EVENTS, "events", 0, PLAIN, nullptr},
      {Page::SETUP, "setup-settings", 0, PLAIN, nullptr},
      {Page::SETUP, "setup-display", 1, PLAIN, nullptr},
      {Page::SETUP, "setup-log", 2, PLAIN, nullptr},
      {Page::SETUP, "setup-about", 3, PLAIN, nullptr},
      {Page::EVENTS, "events-logic", 0, LOGIC, nullptr},
      {Page::EVENTS, "events-all-types", 0, ALL_TYPES, "fault-cells"},
      {Page::EVENTS, "events-all-types", 0, ALL_TYPES, "desk-fault"},
      {Page::EVENTS, "popup-event", 0, EVENT, "desk-fault"},
      {Page::CONTROL, "popup-confirm", 0, CONFIRM_STOP, "charging"},
      {Page::CONTROL, "popup-pin", 0, PIN, "charging"},
      {Page::CONTROL, "popup-pin-wrong", 0, PIN_WRONG, "charging"},
      {Page::SETUP, "setup-settings-unlocked", 0, UNLOCKED, "charging"},
      {Page::SETUP, "popup-number", 0, NUMBER, "charging"},
      {Page::SETUP, "setup-registers", 2, REGISTERS, "charging"},
      {Page::SETUP, "popup-wifi-list", 1, WIFI_LIST, "charging"},
      {Page::SETUP, "popup-keyboard", 1, KEYBOARD, "charging"},
  };
  int written = 0;
  for (const auto &s : scenarios) {
    for (const auto &v : VIEWS) {
      if (v.only != nullptr && std::string(v.only) != s.name) continue;
      Ui ui;
      Model model = s.model;
      ui.page = v.page;
      ui.setup_section = v.section;
      ui.events_view = v.extra == ALL_TYPES ? 1 : (v.extra == LOGIC ? 2 : 0);
      ui.raw_registers = v.extra == REGISTERS;
      model.settings_unlocked = v.extra == UNLOCKED || v.extra == NUMBER;
      Popup &p = ui.popup;
      switch (v.extra) {
        case CONFIRM_STOP:
          p.kind = Overlay::CONFIRM;
          p.title = "Open the contactors?";
          p.text = "The BMS disconnects the pack from the inverter. The inverter loses its battery until you press Run.";
          p.action = "Open the contactors";
          p.danger = true;
          break;
        case PIN:
        case PIN_WRONG:
          p.kind = Overlay::PIN;
          p.action = "open the contactors";
          p.entry = v.extra == PIN ? "12" : "";
          p.wrong = v.extra == PIN_WRONG;
          break;
        case NUMBER:
          p.kind = Overlay::NUMBER;
          p.index = 0;
          p.entry = "85";
          break;
        case WIFI_LIST: p.kind = Overlay::WIFI_LIST; break;
        case KEYBOARD:
          p.kind = Overlay::KEYBOARD;
          p.title = "Hakalau Farm";
          p.entry = "mauka-2";
          break;
        case EVENT:
          p.kind = Overlay::EVENT_DETAIL;
          p.index = 4;
          break;
        default: break;
      }
      ui.render(canvas, model, 0);
      const std::string path = out + "/" + v.name + "-" + s.name + ".ppm";
      if (!write_ppm(path, canvas)) {
        fprintf(stderr, "cannot write %s\n", path.c_str());
        return 1;
      }
      printf("%s\n", path.c_str());
      written++;
    }
  }
  printf("%d images\n", written);
  return 0;
}
