// Made-up packs for the simulator: one Model per situation worth looking at
#pragma once

#include "../components/hmi_ui/ui_model.h"
#include "../components/hmi_bms/hmi_map.h"

#include <cstdint>

namespace sim {

using hmiui::Level;
using hmiui::Model;

struct Scenario {
  const char *name;
  const char *what;
  Model model;
};

inline void add_event(Model &m, int type, Level level, int count) {
  m.events[m.event_count++] = {(uint8_t) type, level, (uint16_t) count, 0};
}

// A 96-cell NMC pack with plausible cell voltages around `mean` volts
inline void fill_cells(Model &m, int cells, float mean, float spread_mv) {
  uint32_t seed = 12345;
  float lo = 1e9f, hi = -1e9f, sum = 0;
  for (int i = 0; i < cells; i++) {
    seed = seed * 1664525u + 1013904223u;
    const float r = ((seed >> 8) & 0xFFFF) / 65535.0f - 0.5f;
    const float mv = mean * 1000.0f + r * spread_mv;
    m.cell_mv[i] = mv;
    lo = mv < lo ? mv : lo;
    hi = mv > hi ? mv : hi;
    sum += mv;
  }
  m.cell_count = cells;
  m.cell_min = lo / 1000.0f;
  m.cell_max = hi / 1000.0f;
  m.cell_avg = sum / cells / 1000.0f;
  m.pack_voltage = sum / 1000.0f;
  m.output_voltage = m.pack_voltage - 0.2f;
}

// A made-up day ending now at `soc_now`: load through the night, sun from 8 to 17 h
inline void fill_history(Model &m, float soc_now, float hour_now, float capacity_kwh) {
  const int n = hmiui::HISTORY_POINTS;
  float start = 50.0f;
  for (int attempt = 0; attempt < 12; attempt++) {
    float soc = start;
    for (int i = 0; i < n; i++) {
      float hour = hour_now - (n - 1 - i) * 0.25f;
      while (hour < 0) hour += 24.0f;
      const float sun = hour > 7.5f && hour < 17.5f ? 9.5f * std::sin((hour - 7.5f) / 10.0f * 3.14159f) : 0.0f;
      const float load = 1.1f + (hour > 17.5f && hour < 22.5f ? 2.4f : 0.0f) + (hour > 6.0f && hour < 8.0f ? 1.0f : 0.0f);
      float kw = sun - load;
      if (soc >= 99.9f && kw > 0) kw = 0.05f;  // full: only a trickle goes in
      if (soc <= 8.0f && kw < 0) kw = 0;       // empty: the inverter stops drawing
      soc += kw * 0.25f / capacity_kwh * 100.0f;
      soc = soc < 8.0f ? 8.0f : (soc > 100.0f ? 100.0f : soc);
      m.history_kw[i] = kw;
      m.history_soc[i] = soc;
    }
    if (std::fabs(soc - soc_now) < 0.3f) break;
    start += soc_now - soc;
    start = start < 8.0f ? 8.0f : (start > 100.0f ? 100.0f : start);
  }
  m.history_count = n;
}

inline void healthy_pack(Model &m) {
  using namespace esphome::hmi_bms;
  m.link = hmiui::Link::OK;
  m.system_state = hmiui::SystemState::OPERATING;
  m.contactors_closed = true;
  m.contactor_state = 8;
  m.soc = 72.4f;
  m.soc_change_1h = 3.2f;
  fill_cells(m, 96, 3.912f, 14.0f);
  m.module_count = 4;
  const float temps[4] = {24.6f, 25.1f, 25.8f, 24.9f};
  for (int i = 0; i < 4; i++) m.module_temp[i] = temps[i];
  m.temp_min = 24.6f;
  m.temp_max = 25.8f;
  m.charge_limit = 80.0f;
  m.charge_limited_by = "working limit";
  m.discharge_limit = 120.0f;
  m.discharge_limited_by = "hard limit";
  m.capacity_ah = 228.0f;
  m.nameplate_ah = 230.0f;
  m.learned_ah = 219.4f;
  m.today_in_kwh = 21.4f;
  m.today_out_kwh = 13.9f;
  m.inverter_soc = 72.0f;
  m.inverter_charge_v = 398.4f;
  m.supervisor_state = 1;
  m.supervisor_reasons = "none";
  m.balance_charge_status = "Next: Sunday 10:00";
  m.v3v3 = 3.28f;
  m.v5 = 5.22f;
  m.v12 = 13.18f;
  m.v_contactor = 13.37f;
  m.pack_name = "ck0";
  m.clock = "13:40";
  add_event(m, HMI_EVENT_BOOT_NORMAL, Level::INACTIVE, 1);
}

inline Scenario charging() {
  Scenario s{"charging", "healthy pack charging from solar at 8.4 kW", {}};
  healthy_pack(s.model);
  s.model.current = 22.4f;
  s.model.power = s.model.pack_voltage * s.model.current;
  fill_history(s.model, s.model.soc, 13.67f, 86.0f);
  return s;
}

inline Scenario discharging() {
  Scenario s{"discharging", "evening load, 3.1 kW out, one warning active", {}};
  healthy_pack(s.model);
  s.model.soc = 41.0f;
  s.model.soc_change_1h = -6.8f;
  s.model.current = -8.3f;
  s.model.power = s.model.pack_voltage * s.model.current;
  s.model.clock = "20:15";
  s.model.today_in_kwh = 38.2f;
  s.model.today_out_kwh = 24.7f;
  fill_history(s.model, s.model.soc, 20.25f, 86.0f);
  add_event(s.model, esphome::hmi_bms::HMI_EVENT_BATTERY_TEMPERATURE_HIGH, Level::WARNING, 2);
  return s;
}

inline Scenario idle() {
  Scenario s{"idle", "contactors closed, nothing flowing, no active events", {}};
  healthy_pack(s.model);
  s.model.soc = 100.0f;
  s.model.soc_change_1h = 0.0f;
  s.model.current = 0.02f;
  s.model.power = 7.0f;
  s.model.charge_limit = 0.0f;
  s.model.charge_limited_by = "cell voltage";
  s.model.clock = "15:05";
  fill_history(s.model, s.model.soc, 15.08f, 86.0f);
  return s;
}

// The desk board on 2026-11-10: nothing connected but power, so everything is stale
inline Scenario desk_fault() {
  using namespace esphome::hmi_bms;
  Scenario s{"desk-fault", "bare board on the desk: no cells, FAULT, five events", {}};
  Model &m = s.model;
  m.link = hmiui::Link::OK;
  m.system_state = hmiui::SystemState::FAULT;
  m.pack_voltage = 0.0f;
  m.current = 0.03f;
  m.power = 0.0f;
  m.soc_change_1h = 3.2f;
  m.charge_limit = 0.0f;
  m.charge_limited_by = "temperature";
  m.discharge_limit = 0.0f;
  m.discharge_limited_by = "working limit";
  m.today_in_kwh = 0.0f;
  m.today_out_kwh = 0.0f;
  m.balance_charge_status = "Off";
  m.pack_name = "ck0";
  m.clock = "13:40";
  m.capacity_ah = 1.0f;
  m.output_voltage = 0.0f;
  add_event(m, HMI_EVENT_BOOT_NORMAL, Level::INFO, 1);
  add_event(m, HMI_EVENT_CELL_VOLTAGES_STALE, Level::FATAL, 1);
  add_event(m, HMI_EVENT_BATTERY_TEMPERATURE_STALE, Level::FATAL, 1);
  add_event(m, HMI_EVENT_BATTERY_VOLTAGE_VERY_LOW, Level::FATAL, 1);
  add_event(m, HMI_EVENT_BMB_READ_ERROR, Level::WARNING, 37283);
  return s;
}

inline Scenario no_link() {
  Scenario s{"no-link", "the BMS does not answer", {}};
  s.model.pack_name = "ck0";
  s.model.clock = "13:40";
  s.model.link_silent_s = 42;
  return s;
}

}  // namespace sim
