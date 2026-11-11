// PC simulator of the LCD UI: draws the pages with the same code the display runs and writes
// them as image files.
//
//   sh sim/run.sh            builds this, runs it, and leaves PNG files in sim/out/
//
// Two kinds of picture: every page for every scenario ("home-charging.png"), and stories, where
// a finger taps its way through the UI and pictures are taken along the way
// ("story-stop-2-pin.png"). What the UI asked the device to do in each story is in stories.txt.
// It writes PPM files (the simplest image format there is); run.sh turns them into PNG.
#include "harness.h"
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

// One step of a story
struct Step {
  enum Kind { PAGE, SECTION, TAP, TYPE, SHOT } kind;
  const char *text;  // TAP: the label. TYPE: the characters. SHOT: the picture's name
  int number;        // PAGE: the page. SECTION: Setup's section. TAP: which one of that label
};
struct Story {
  const char *name;
  const char *scenario;
  std::vector<Step> steps;
};

static Step page(Page p) { return {Step::PAGE, "", (int) p}; }
static Step section(int n) { return {Step::SECTION, "", n}; }
static Step tap(const char *label, int nth = 0) { return {Step::TAP, label, nth}; }
static Step type(const char *text) { return {Step::TYPE, text, 0}; }
static Step shot(const char *name) { return {Step::SHOT, name, 0}; }

int main(int argc, char **argv) {
  const std::string out = argc > 1 ? argv[1] : "sim/out";
  const sim::Scenario scenarios[] = {sim::charging(),  sim::discharging(),      sim::idle(),
                                     sim::precharge(), sim::lfp(),              sim::fault_with_cells(),
                                     sim::desk_fault(), sim::wrong_protocol(), sim::no_link()};
  int written = 0;
  auto save = [&](const std::string &name, const Canvas &canvas) {
    if (!write_ppm(out + "/" + name + ".ppm", canvas)) {
      fprintf(stderr, "cannot write %s\n", name.c_str());
      exit(1);
    }
    written++;
  };

  // ---- every page, for every scenario ----
  struct View {
    Page page;
    const char *name;
    int section;
    int events_view;
  };
  static const View VIEWS[] = {
      {Page::HOME, "home", 0, 0},           {Page::CONTROL, "control", 0, 0},     {Page::POWER, "power", 0, 0},
      {Page::CELLS, "cells", 0, 0},         {Page::HEALTH, "health", 0, 0},       {Page::EVENTS, "events", 0, 0},
      {Page::EVENTS, "events-logic", 0, 2}, {Page::EVENTS, "events-all-types", 0, 1},
      {Page::SETUP, "setup-settings", 0, 0}, {Page::SETUP, "setup-display", 1, 0}, {Page::SETUP, "setup-log", 2, 0},
      {Page::SETUP, "setup-about", 3, 0},
  };
  for (const auto &s : scenarios) {
    for (const auto &v : VIEWS) {
      sim::Driver d(s.model);
      d.ui.state.page = v.page;
      d.ui.state.setup_section = v.section;
      d.ui.state.events_view = v.events_view;
      d.draw();
      save(std::string(v.name) + "-" + s.name, d.canvas);
    }
  }

  // ---- stories ----
  const std::vector<Story> stories = {
      {"stop", "charging",
       {page(Page::CONTROL), tap("Stop, open the contactors"), shot("1-confirm"), tap("Open the contactors"), type("12"),
        shot("2-pin"), type("99"), tap("OK"), shot("3-pin-wrong"), type("1234"), tap("OK"), shot("4-sent")}},
      {"setting", "charging",
       {page(Page::SETUP), tap("Unlock"), type("1234"), tap("OK"), shot("1-unlocked"), tap("Max charge current"), type("85"),
        shot("2-number"), tap("Set"), shot("3-sent")}},
      {"wifi", "charging",
       {page(Page::SETUP), section(1), tap("Set up a network"), shot("1-list"), tap("Hakalau Farm"), type("mauka"),
        shot("2-password"), tap("Show"), tap("#+="), type("-2"), shot("3-symbols"), tap("Join"), shot("4-sent")}},
      {"events", "desk-fault",
       {page(Page::EVENTS), tap("BMB Read Error"), shot("1-detail"), tap("Close"), tap("How a fault happens"), shot("2-logic"),
        tap("‹  Active"), tap("All event types"), shot("3-all-types")}},
      {"registers", "charging", {page(Page::SETUP), section(2), tap("Raw registers"), shot("1-registers")}},
      {"rail", "charging", {tap("Cells"), shot("1-cells"), tap("Home"), tap("All cells"), shot("2-cells-again")}},
  };
  FILE *report = fopen((out + "/stories.txt").c_str(), "w");
  int failed = 0;
  for (const auto &story : stories) {
    const sim::Scenario *scenario = nullptr;
    for (const auto &s : scenarios) {
      if (std::string(s.name) == story.scenario) scenario = &s;
    }
    if (scenario == nullptr) continue;
    sim::Driver d(scenario->model);
    fprintf(report, "story %s (%s)\n", story.name, story.scenario);
    for (const auto &step : story.steps) {
      switch (step.kind) {
        case Step::PAGE: d.ui.state.page = (Page) step.number; d.draw(); break;
        case Step::SECTION: d.ui.state.setup_section = step.number; d.draw(); break;
        case Step::TAP:
          if (!d.tap(step.text, step.number)) {
            fprintf(report, "  NOTHING TO TAP: %s\n", step.text);
            fprintf(stderr, "story %s: nothing to tap called \"%s\"\n", story.name, step.text);
            failed++;
          }
          break;
        case Step::TYPE:
          if (!d.type(step.text)) {
            fprintf(stderr, "story %s: cannot type \"%s\"\n", story.name, step.text);
            failed++;
          }
          break;
        case Step::SHOT:
          save(std::string("story-") + story.name + "-" + step.text, d.canvas);
          fprintf(report, "  picture %s\n", step.text);
          break;
      }
    }
    for (const auto &line : d.actions.log) fprintf(report, "  asked: %s\n", line.c_str());
    fprintf(report, "\n");
  }
  fclose(report);
  printf("%d images, %d story steps failed\n", written, failed);
  return failed ? 1 : 0;
}
