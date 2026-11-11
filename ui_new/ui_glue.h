// Helpers for ui_new_1024x600.yaml: turning what the BMS component's entities hold into what the
// LCD UI's model wants. Included in main.cpp before the entities exist, so everything here
// takes the entities it needs as arguments.
#pragma once

#include "esphome/components/hmi_ui/ui_model.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace ui_glue {

// ---- BMS settings: the numbers the settings page lists, in its order ----

struct BoundSetting {
  const char *group;
  const char *name;
  esphome::number::Number *number;
  float scale;       // from the number's unit to the one shown (mV to V: 0.001)
  uint8_t decimals;
  const char *unit;  // the one shown
};

inline std::vector<BoundSetting> &bound_settings() {
  static std::vector<BoundSetting> list;
  return list;
}

inline void fill_settings(hmiui::Model &m) {
  static std::vector<hmiui::Setting> shown;
  const auto &bound = bound_settings();
  shown.resize(bound.size());
  for (size_t i = 0; i < bound.size(); i++) {
    const BoundSetting &b = bound[i];
    const float lo = b.number->traits.get_min_value() * b.scale, hi = b.number->traits.get_max_value() * b.scale;
    shown[i] = {b.group, b.name, b.number->has_state() ? b.number->state * b.scale : NAN, b.decimals, b.unit, lo, hi};
  }
  m.settings = shown.data();
  m.setting_count = (int) shown.size();
}

inline void write_setting(int index, float value) {
  const auto &bound = bound_settings();
  if (index < 0 || index >= (int) bound.size()) return;
  auto call = bound[index].number->make_call();
  call.set_value(value / bound[index].scale);
  call.perform();
}

// ---- small conversions ----

// The BMS sends 6553.5 A for "this does not limit"
inline float limit(float amps) { return std::isnan(amps) ? NAN : (amps >= 6500.0f ? INFINITY : amps); }

// One part of a current limit, and whether it is the one in force
inline void add_part(hmiui::LimitPart *parts, int &count, const char *name, float amps, float in_force, const char *&by) {
  if (count >= hmiui::MAX_LIMIT_PARTS || std::isnan(amps)) return;
  parts[count++] = {name, limit(amps)};
  if (by[0] == 0 && !std::isnan(in_force) && std::fabs(amps - in_force) < 0.05f) by = name;
}

// An event sensor's state: "INACTIVE", or "<LEVEL> (<count>)" with " d=<hex>" after it when
// the event carries data. False for an event that never happened.
inline bool parse_event(const std::string &state, hmiui::Level &level, uint16_t &count, uint64_t &data) {
  static const char *const LEVELS[] = {"INACTIVE", "INFO", "WARNING", "CRITICAL", "FATAL"};
  const size_t open = state.find('(');
  if (open == std::string::npos) return false;
  level = hmiui::Level::INACTIVE;
  for (int i = 0; i < 5; i++) {
    if (state.compare(0, strlen(LEVELS[i]), LEVELS[i]) == 0) level = (hmiui::Level) i;
  }
  const long n = strtol(state.c_str() + open + 1, nullptr, 10);
  count = (uint16_t) (n > 65535 ? 65535 : (n < 0 ? 0 : n));
  const size_t d = state.find("d=");
  data = d == std::string::npos ? 0 : strtoull(state.c_str() + d + 2, nullptr, 16);
  return true;
}

// A list of cell numbers as the BMS component writes it, "1-3,7" or "None", into flags
inline void parse_cells(const std::string &list, bool *flags, int max) {
  const char *p = list.c_str();
  while (*p) {
    if (*p < '0' || *p > '9') {
      p++;
      continue;
    }
    char *end;
    long first = strtol(p, &end, 10), last = first;
    if (*end == '-') last = strtol(end + 1, &end, 10);
    for (long cell = first; cell <= last && cell <= max; cell++) {
      if (cell >= 1) flags[cell - 1] = true;
    }
    p = end;
  }
}

}  // namespace ui_glue
