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
  static const char *const PAGE_NAMES[] = {"main"};
  int written = 0;
  for (const auto &s : scenarios) {
    Ui ui;
    for (int p = 0; p < (int) (sizeof(PAGE_NAMES) / sizeof(PAGE_NAMES[0])); p++) {
      ui.page = (Page) p;
      ui.render(canvas, s.model, 0);
      const std::string path = out + "/" + s.name + "-" + PAGE_NAMES[p] + ".ppm";
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
