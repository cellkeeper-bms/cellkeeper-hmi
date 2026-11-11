// Everything the LCD UI shows, as plain data. On the device it is filled from the BMS
// component's sensors; in the simulator from scenarios (sim/scenarios.h). A float that isn't
// known is NAN, and the UI shows a dash for it.
#pragma once

#include <cmath>
#include <cstdint>

namespace hmiui {

static const int MAX_CELLS = 120;
static const int MAX_MODULES = 8;
static const int MAX_EVENTS = 64;

// What the link to the BMS is doing
enum class Link : uint8_t {
  NO_DATA,   // nothing heard from the BMS
  WRONG_PROTOCOL,  // a BMS answers, in a protocol this HMI doesn't speak
  CONNECTING,      // reading what the BMS has
  OK,
};

// BMS system state (the firmware's enum system_states)
enum class SystemState : uint8_t { UNINITIALIZED, INITIALIZING, CALIBRATING, INACTIVE, OPERATING, FAULT, UNKNOWN };

// Event levels as the BMS sends them
enum class Level : uint8_t { INACTIVE, INFO, WARNING, CRITICAL, FATAL };

struct Event {
  uint8_t type;      // index into the firmware's event list
  Level level;       // now
  uint16_t count;    // times it has happened
  uint64_t data;     // of its latest occurrence
};

struct Model {
  Link link = Link::NO_DATA;
  SystemState system_state = SystemState::UNKNOWN;
  uint8_t contactor_state = 0;      // the firmware's contactors state number
  bool contactors_closed = false;

  // Pack
  float soc = NAN;                  // %
  float pack_voltage = NAN;         // V, battery side
  float output_voltage = NAN;       // V, inverter side
  float current = NAN;              // A, + = charging
  float power = NAN;                // W, + = charging
  float soc_change_1h = NAN;        // % over the last hour

  // Cells
  int cell_count = 0;               // cells with a reading
  float cell_mv[MAX_CELLS];         // mV, NAN = no cell in this slot
  uint16_t balance_s[MAX_CELLS];    // balancing time left, seconds
  bool balancing_now[MAX_CELLS];
  float cell_min = NAN, cell_max = NAN, cell_avg = NAN;  // V
  int module_count = 0;
  float module_temp[MAX_MODULES];   // degrees C
  float temp_min = NAN, temp_max = NAN;

  // Limits (A); NAN = not known, INFINITY = no limit
  float charge_limit = NAN, discharge_limit = NAN;
  const char *charge_limited_by = "";     // "temperature", "cell voltage", "working limit"...
  const char *discharge_limited_by = "";

  // Capacity and energy
  float capacity_ah = NAN, nameplate_ah = NAN, learned_ah = NAN;
  float today_in_kwh = NAN, today_out_kwh = NAN;
  float lifetime_in_kwh = NAN, lifetime_out_kwh = NAN;

  // What the inverter is told
  float inverter_soc = NAN, inverter_charge_v = NAN, inverter_discharge_v = NAN;

  // Events
  int event_count = 0;
  Event events[MAX_EVENTS];

  // Supervisor
  int supervisor_state = -1;        // -1 no answer, 0 disabled, 1 enabled, 2 latched off
  const char *supervisor_reasons = "";

  // Supplies and temperatures of the electronics
  float v3v3 = NAN, v5 = NAN, v12 = NAN, v_contactor = NAN;
  float bms_chip_temp = NAN, shunt_temp = NAN, hmi_chip_temp = NAN;

  // Identity
  const char *pack_name = "";
  const char *bms_firmware = "";
  const char *hmi_firmware = "";
  const char *board_revision = "";
  const char *wifi_ssid = "";
  const char *ip = "";
  int wifi_rssi = 0;
  uint32_t bms_uptime_s = 0, hmi_uptime_s = 0;

  const char *balance_charge_status = "";

  Model() {
    for (auto &v : cell_mv) v = NAN;
    for (auto &v : balance_s) v = 0;
    for (auto &v : balancing_now) v = false;
    for (auto &v : module_temp) v = NAN;
  }

  // The most severe active event's level
  Level worst_level() const {
    Level worst = Level::INACTIVE;
    for (int i = 0; i < event_count; i++) {
      if (events[i].level > worst) worst = events[i].level;
    }
    return worst;
  }
  int active_events() const {
    int n = 0;
    for (int i = 0; i < event_count; i++) n += events[i].level != Level::INACTIVE;
    return n;
  }
};

}  // namespace hmiui
