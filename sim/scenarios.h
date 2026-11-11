// Made-up packs for the simulator: one Model per situation worth looking at
#pragma once

#include "../components/hmi_ui/ui_model.h"
#include "../components/hmi_bms/hmi_map.h"

#include <cstdint>
#include <initializer_list>

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

inline void set_limits(Model &m, std::initializer_list<hmiui::LimitPart> charge,
                       std::initializer_list<hmiui::LimitPart> discharge) {
  m.charge_part_count = m.discharge_part_count = 0;
  m.charge_limit = m.discharge_limit = INFINITY;
  for (const auto &p : charge) {
    m.charge_parts[m.charge_part_count++] = p;
    if (p.amps < m.charge_limit) {
      m.charge_limit = p.amps;
      m.charge_limited_by = p.name;
    }
  }
  for (const auto &p : discharge) {
    m.discharge_parts[m.discharge_part_count++] = p;
    if (p.amps < m.discharge_limit) {
      m.discharge_limit = p.amps;
      m.discharge_limited_by = p.name;
    }
  }
}

// The BMS settings, as the settings page lists them
static const hmiui::Setting SETTINGS[] = {
    {"Current", "Max charge current", 100.0f, 1, "A"},
    {"Current", "Max discharge current", 120.0f, 1, "A"},
    {"Cell voltage", "Working range, bottom", 3.300f, 3, "V"},
    {"Cell voltage", "Working range, top", 4.050f, 3, "V"},
    {"Cell voltage", "Soft limit, low", 3.000f, 3, "V"},
    {"Cell voltage", "Soft limit, high", 4.200f, 3, "V"},
    {"Sent to the inverter", "Voltage offset, lower", 0.0f, 1, "V"},
    {"Sent to the inverter", "Voltage offset, upper", 0.0f, 1, "V"},
    {"State of charge", "SoC scaling, bottom", 5.0f, 0, "%"},
    {"State of charge", "SoC scaling, top", 98.0f, 0, "%"},
    {"State of charge", "Set SoC now", 72.0f, 0, "%"},
    {"State of charge", "Capacity", 228.0f, 0, "Ah"},
    {"Balancing", "Balance above", 3.900f, 3, "V"},
    {"Balancing", "Balance to within", 4.0f, 0, "mV"},
    {"Balance charge", "Every", 7.0f, 0, "days"},
    {"Balance charge", "Upper offset", 2.0f, 1, "V"},
};

static const char *const LOG_LINES[] = {
    "[  6812.340] bmb: 96 cells, min 3.905 max 3.919, 4 modules ok",
    "[  6812.341] limits: chg 80.0 A (working) dis 120.0 A (setting)",
    "[  6813.340] soc: ekf 72.4 % count 72.1 % ocv 72.9 %",
    "[  6813.842] inverter: can rx ok, cvl 398.4 V ccl 80.0 A dcl 120.0 A",
    "[  6814.340] bmb: 96 cells, min 3.905 max 3.919, 4 modules ok",
    "[  6814.341] balancing: 19 cells, cycle 35 s left",
    "[  6815.340] contactors: CLOSED, supply 13.37 V",
    "[  6815.341] isolation: last open check 76860 s ago, >= 100 Mohm",
    "[  6816.340] bmb: 96 cells, min 3.905 max 3.920, 4 modules ok",
    "[  6816.341] current: +22.41 A, shunt 30.6 C",
};

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
  set_limits(m,
             {{"hard limit", 150.0f}, {"temperature", 150.0f}, {"cell voltage", 150.0f}, {"top of working range", 80.0f},
              {"cell spread", INFINITY}, {"max charge setting", 100.0f}},
             {{"hard limit", 150.0f}, {"temperature", 150.0f}, {"cell voltage", 150.0f},
              {"bottom of working range", 150.0f}, {"max discharge setting", 120.0f}});
  m.contactor_state_text = "CLOSED";
  m.link_voltage = 375.5f;
  m.contactor_pos_v = 0.1f;
  m.contactor_neg_v = 0.1f;
  m.isolation_kohm = 100000.0f;
  m.isolation_pos_v = 187.4f;
  m.isolation_neg_v = -196.3f;
  m.isolation_age_s = 76860;
  m.bms_chip_temp = 32.8f;
  m.shunt_temp = 30.6f;
  m.shunt_ntc_temp = 28.4f;
  m.hmi_chip_temp = 33.8f;
  m.lifetime_in_kwh = 4312.0f;
  m.lifetime_out_kwh = 4087.0f;
  m.settings = SETTINGS;
  m.setting_count = (int) (sizeof(SETTINGS) / sizeof(SETTINGS[0]));
  m.log_lines = LOG_LINES;
  m.log_count = (int) (sizeof(LOG_LINES) / sizeof(LOG_LINES[0]));
  m.log_enabled = true;
  m.bms_serial = "17631295745500622579";
  m.bms_firmware = "9787788 · 2026-11-10 23:37 UTC";
  m.hmi_firmware = "e572d88 · 2026-11-11 01:27 UTC";
  m.board_revision = "Rev 1.1";
  m.wifi_ssid = "LANZ";
  m.ip = "192.168.111.222";
  m.wifi_rssi = -49;
  m.wifi_ap = "AE:9C:6C:C8:28:E7";
  m.bms_uptime_s = 6816;
  m.hmi_uptime_s = 174;
  m.capacity_ah = 228.0f;
  m.nameplate_ah = 230.0f;
  m.learned_ah = 219.4f;
  m.today_in_kwh = 21.4f;
  m.today_out_kwh = 13.9f;
  m.inverter_soc = 72.0f;
  m.inverter_charge_v = 398.4f;
  m.inverter_discharge_v = 288.0f;
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
  add_event(m, HMI_EVENT_INVERTER_DETECTED, Level::INFO, 1);
  add_event(m, HMI_EVENT_OVERCURRENT_CHARGING, Level::INACTIVE, 2);
  add_event(m, HMI_EVENT_CELL_VOLTAGE_GLITCH, Level::INACTIVE, 14);
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
  s.model.inverter_soc = 41.0f;
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
  s.model.inverter_soc = 100.0f;
  s.model.soc_change_1h = 0.0f;
  s.model.current = 0.02f;
  s.model.power = 7.0f;
  s.model.charge_parts[2].amps = 0.0f;  // cell voltage
  s.model.charge_limit = 0.0f;
  s.model.charge_limited_by = "cell voltage";
  // Balancing at the top: the higher cells bleed, some more are waiting their turn
  int bleeding = 0;
  for (int i = 0; i < 96; i++) {
    if (s.model.cell_mv[i] > 3916.0f && bleeding < 19) {
      s.model.balancing_now[i] = true;
      s.model.balance_s[i] = 35;
      bleeding++;
    } else if (s.model.cell_mv[i] > 3914.5f) {
      s.model.balance_s[i] = 120;
    }
  }
  s.model.balance_cycle_left_s = 35;
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
  set_limits(m,
             {{"hard limit", 30.0f}, {"temperature", 0.0f}, {"cell voltage", 0.0f}, {"top of working range", 30.0f},
              {"cell spread", INFINITY}, {"max charge setting", 30.0f}},
             {{"hard limit", 30.0f}, {"temperature", 0.0f}, {"cell voltage", 0.0f}, {"bottom of working range", 0.0f},
              {"max discharge setting", 30.0f}});
  m.contactor_state_text = "OPEN";
  m.link_voltage = 0.0f;
  m.contactor_pos_v = 0.0f;
  m.contactor_neg_v = 0.4f;
  m.v3v3 = 3.28f;
  m.v5 = 5.22f;
  m.v12 = 13.18f;
  m.v_contactor = 13.37f;
  m.bms_chip_temp = 32.8f;
  m.shunt_temp = 30.6f;
  m.shunt_ntc_temp = -55.2f;
  m.hmi_chip_temp = 33.8f;
  m.supervisor_state = 0;
  m.supervisor_reasons = "NO_SCAN, BMS_DISAGREE, SETTLING";
  m.nameplate_ah = 1.0f;
  m.settings = SETTINGS;
  m.setting_count = (int) (sizeof(SETTINGS) / sizeof(SETTINGS[0]));
  m.bms_serial = "17631295745500622579";
  m.bms_firmware = "9787788-dirty · 2026-11-10 23:37 UTC";
  m.hmi_firmware = "e572d88 · 2026-11-11 01:27 UTC";
  m.board_revision = "Rev 1.1";
  m.wifi_ssid = "LANZ";
  m.ip = "10.60.7.94";
  m.wifi_rssi = -49;
  m.wifi_ap = "AE:9C:6C:C8:28:E7";
  m.bms_uptime_s = 6820;
  m.hmi_uptime_s = 174;
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
  s.model.hmi_firmware = "e572d88 · 2026-11-11 01:27 UTC";
  s.model.wifi_ssid = "LANZ";
  s.model.ip = "192.168.111.222";
  s.model.wifi_rssi = -49;
  s.model.wifi_ap = "AE:9C:6C:C8:28:E7";
  s.model.hmi_uptime_s = 174;
  s.model.hmi_chip_temp = 33.8f;
  return s;
}

}  // namespace sim
