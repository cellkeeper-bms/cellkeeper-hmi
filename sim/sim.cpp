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

  const sim::Scenario scenarios[] = {sim::charging(), sim::discharging(), sim::idle(), sim::desk_fault(),
                                     sim::no_link()};
  // What to draw: the page, the name its files get, and for Setup which section
  struct View {
    Page page;
    const char *name;
    int section;
  };
  static const View VIEWS[] = {
      {Page::HOME, "home", 0},
      {Page::CELLS, "cells", 0},
      {Page::POWER, "power", 0},
      {Page::HEALTH, "health", 0},
      {Page::EVENTS, "events", 0},
      {Page::CONTROL, "control", 0},
      {Page::SETUP, "setup-settings", 0},
      {Page::SETUP, "setup-display", 1},
      {Page::SETUP, "setup-log", 2},
      {Page::SETUP, "setup-about", 3},
  };
  int written = 0;
  for (const auto &s : scenarios) {
    for (const auto &v : VIEWS) {
      Ui ui;
      ui.page = v.page;
      ui.setup_section = v.section;
      ui.render(canvas, s.model, 0);
      const std::string path = out + "/" + v.name + "-" + s.name + ".ppm";
      if (!write_ppm(path, canvas)) {
        fprintf(stderr, "cannot write %s\n", path.c_str());
        return 1;
      }
      printf("%s  (%s)\n", path.c_str(), s.what);
      written++;
    }
  }
  printf("%d images\n", written);
  return 0;
}
