// Drives the UI without a display: taps by label, drags, and a record of what the UI asked
// for. Used by the simulator (sim.cpp) and the host tests (tests/ui_test.cpp).
#pragma once

#include "../components/hmi_ui/ui.h"

#include <cstdio>
#include <string>
#include <vector>

namespace sim {

using namespace hmiui;

// Writes down what the UI asked for, and answers as the device would
class Recorder : public Actions {
 public:
  std::string pin = "1234";
  std::vector<std::string> log;
  Model *model = nullptr;  // what the device would change at once is changed here too

  bool pin_ok(const char *typed) override {
    const bool ok = pin == typed;
    log.push_back(ok ? "pin ok" : "pin wrong");
    return ok;
  }
  void command(Command command) override { log.push_back(std::string("command ") + command_name(command)); }
  void write_setting(int index, float value) override {
    char line[64];
    snprintf(line, sizeof(line), "setting %d = %g", index, (double) value);
    log.push_back(line);
  }
  void set_settings_unlocked(bool unlocked) override {
    log.push_back(unlocked ? "unlock settings" : "lock settings");
    if (model != nullptr) model->settings_unlocked = unlocked;
  }
  void set_brightness(int percent) override {
    log.push_back("brightness " + std::to_string(percent));
    if (model != nullptr) model->brightness = (uint8_t) percent;
  }
  void set_log_enabled(bool on) override {
    log.push_back(on ? "log on" : "log off");
    if (model != nullptr) model->log_enabled = on;
  }
  void wifi_join(const char *ssid, const char *password) override {
    log.push_back(std::string("join ") + ssid + " / " + password);
  }

  bool did(const std::string &what) const {
    for (const auto &line : log) {
      if (line == what) return true;
    }
    return false;
  }
  int count(const std::string &start) const {
    int n = 0;
    for (const auto &line : log) n += line.compare(0, start.size(), start) == 0;
    return n;
  }
};

// A UI with a model, a frame to draw on and a finger
struct Driver {
  std::vector<uint16_t> pixels;
  Canvas canvas;
  Ui ui;
  Model model;
  Recorder actions;
  HitTrace trace;
  uint32_t now = 0;

  explicit Driver(const Model &m) : pixels((size_t) SCREEN_W * SCREEN_H), canvas(pixels.data(), SCREEN_W, SCREEN_H), model(m) {
    actions.model = &model;
    this->draw();
  }

  // One frame with the finger down at (x, y), or up
  void step(bool down, int x, int y) {
    Input input;
    input.down = down;
    input.x = x;
    input.y = y;
    now += 50;
    ui.frame(canvas, model, input, actions, now, &trace);
  }
  void draw() { this->step(false, 0, 0); }
  void tap_at(int x, int y) {
    this->step(true, x, y);
    this->step(false, x, y);
  }
  // Taps the n-th thing with that label on the frame as it was last drawn. False if there is none.
  bool tap(const char *label, int nth = 0) {
    const Hit *hit = trace.find(label, nth);
    if (hit == nullptr) return false;
    this->tap_at(hit->x + hit->w / 2, hit->y + hit->h / 2);
    return true;
  }
  // Types on a pad or the keyboard, one key per character
  bool type(const char *text) {
    for (const char *p = text; *p; p++) {
      const char key[2] = {*p, 0};
      if (!this->tap(key)) return false;
    }
    return true;
  }
  void drag(int x0, int y0, int x1, int y1) {
    this->step(true, x0, y0);
    for (int i = 1; i <= 8; i++) this->step(true, x0 + (x1 - x0) * i / 8, y0 + (y1 - y0) * i / 8);
    this->step(false, x1, y1);
  }
  bool shows(const char *label) const { return trace.find(label) != nullptr; }
};

}  // namespace sim
