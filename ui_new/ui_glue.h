// Helpers for ui_new_1024x600.yaml: turning what the BMS component's entities hold into what the
// LCD UI's model wants. Included in main.cpp before the entities exist, so everything here
// takes the entities it needs as arguments.
#pragma once

#include "esphome/components/hmi_ui/ui_model.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

// ---- the last 24 hours ----

// Kept in RAM, so it starts empty after a restart. One point per quarter of an hour: the state
// of charge at its end and the average power over it. The quarter in progress is the newest
// point, so the chart is not a quarter of an hour behind.
inline void fill_history(hmiui::Model &m, float soc, float power_w, uint32_t now_ms) {
  static const uint32_t QUARTER_MS = 15 * 60 * 1000;
  static float socs[hmiui::HISTORY_POINTS], kws[hmiui::HISTORY_POINTS];
  static int count = 0, samples = 0;
  static uint32_t quarter_started_ms = 0, last_sample_ms = 0;
  static double kw_sum = 0;
  if (!std::isnan(power_w) && !std::isnan(soc) && now_ms - last_sample_ms >= 1000) {
    if (samples == 0) quarter_started_ms = now_ms;
    kw_sum += power_w / 1000.0;
    samples++;
    last_sample_ms = now_ms;
  }
  if (samples > 0 && now_ms - quarter_started_ms >= QUARTER_MS) {
    if (count == hmiui::HISTORY_POINTS) {
      memmove(socs, socs + 1, sizeof(float) * (count - 1));
      memmove(kws, kws + 1, sizeof(float) * (count - 1));
      count--;
    }
    socs[count] = soc;
    kws[count] = (float) (kw_sum / samples);
    count++;
    kw_sum = 0;
    samples = 0;
  }
  int n = count;
  memcpy(m.history_soc, socs, sizeof(float) * n);
  memcpy(m.history_kw, kws, sizeof(float) * n);
  if (samples > 0) {
    if (n == hmiui::HISTORY_POINTS) {
      memmove(m.history_soc, m.history_soc + 1, sizeof(float) * (n - 1));
      memmove(m.history_kw, m.history_kw + 1, sizeof(float) * (n - 1));
      n--;
    }
    m.history_soc[n] = soc;
    // To a tenth of a kilowatt: the average so far moves with every sample
    m.history_kw[n] = std::round(kw_sum / samples * 10.0) / 10.0f;
    n++;
  }
  m.history_count = n;
}

// ---- the Wi-Fi networks in range ----

// From the Wi-Fi component's last scan: each name once, the strongest first. Read again every
// few seconds, not every frame.
template<typename Results> inline void fill_networks(hmiui::Model &m, const Results &results, uint32_t now_ms) {
  static std::vector<std::string> names;
  static std::vector<hmiui::WifiNetwork> networks;
  static uint32_t read_ms = 0;
  static bool read = false;
  if (!read || now_ms - read_ms >= 5000) {
    read = true;
    read_ms = now_ms;
    struct Found {
      std::string ssid;
      int rssi;
      bool secured;
    };
    std::vector<Found> found;
    for (const auto &r : results) {
      const std::string ssid = r.get_ssid().str();
      if (ssid.empty()) continue;
      bool seen = false;
      for (auto &f : found) {
        if (f.ssid == ssid) {
          seen = true;
          if (r.get_rssi() > f.rssi) f.rssi = r.get_rssi();
        }
      }
      if (!seen) found.push_back({ssid, (int) r.get_rssi(), r.get_with_auth()});
    }
    std::sort(found.begin(), found.end(), [](const Found &a, const Found &b) { return a.rssi > b.rssi; });
    names.clear();
    networks.clear();
    for (const auto &f : found) names.push_back(f.ssid);
    for (size_t i = 0; i < found.size(); i++) networks.push_back({names[i].c_str(), found[i].rssi, found[i].secured});
  }
  m.wifi_networks = networks.data();
  m.wifi_network_count = (int) networks.size();
}

// ---- the raw register view ----

static const int MAX_REGISTERS = 64;
inline hmiui::RegisterRow *register_rows() {
  static hmiui::RegisterRow rows[MAX_REGISTERS];
  return rows;
}
inline char (*register_values())[28] {
  static char values[MAX_REGISTERS][28];
  return values;
}
// One row: the register's number, its name in the firmware, and what its entity holds now
inline void add_register(hmiui::Model &m, uint16_t id, const char *name, float value, const char *format) {
  if (m.register_count >= MAX_REGISTERS) return;
  char *text = register_values()[m.register_count];
  if (std::isnan(value)) {
    snprintf(text, 28, "--");
  } else {
    snprintf(text, 28, format, (double) value);
  }
  register_rows()[m.register_count] = {id, name, text};
  m.registers = register_rows();
  m.register_count++;
}
inline void add_register(hmiui::Model &m, uint16_t id, const char *name, const std::string &value) {
  if (m.register_count >= MAX_REGISTERS) return;
  char *text = register_values()[m.register_count];
  snprintf(text, 28, "%s", value.c_str());
  register_rows()[m.register_count] = {id, name, text};
  m.registers = register_rows();
  m.register_count++;
}

// ---- the BMS log ----

// The log line entities, in their order, without the empty ones
inline void fill_log(hmiui::Model &m, esphome::text_sensor::TextSensor *const *lines, int count) {
  static const char *shown[64];
  int n = 0;
  for (int i = 0; i < count && n < 64; i++) {
    if (!lines[i]->state.empty()) shown[n++] = lines[i]->state.c_str();
  }
  m.log_lines = shown;
  m.log_count = n;
}

}  // namespace ui_glue
