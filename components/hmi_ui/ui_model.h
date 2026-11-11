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
static const int HISTORY_POINTS = 96;  // 24 hours, one point per 15 minutes

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

// One of the things that limit the current, and what it allows
struct LimitPart {
  const char *name;  // "hard limit", "temperature", "cell voltage"...
  float amps;        // INFINITY = not limiting
};
static const int MAX_LIMIT_PARTS = 6;

// A BMS setting as the settings page lists it
struct Setting {
  const char *group;
  const char *name;
  float value;       // as the BMS confirmed it
  uint8_t decimals;
  const char *unit;
};

struct Model {
  Link link = Link::NO_DATA;
  uint32_t link_silent_s = 0;       // how long nothing has been heard from the BMS
  SystemState system_state = SystemState::UNKNOWN;
  uint8_t contactor_state = 0;      // the firmware's contactors state number
  bool contactors_closed = false;
  const char *contactor_state_text = "";  // the firmware's name for the contactor state
  float link_voltage = NAN;         // V, between the contactors
  float contactor_pos_v = NAN, contactor_neg_v = NAN;  // V across each contactor

  // Pack
  float soc = NAN;                  // %
  float pack_voltage = NAN;         // V, battery side
  float output_voltage = NAN;       // V, inverter side
  float current = NAN;              // A, + = charging
  float power = NAN;                // W, + = charging
  float soc_change_1h = NAN;        // % over the last hour

  // The last 24 hours as the display saw them, oldest first (kept in RAM: empty after a reboot)
  int history_count = 0;
  float history_soc[HISTORY_POINTS];  // %
  float history_kw[HISTORY_POINTS];   // kW, + = charging

  // Cells
  int cell_count = 0;               // cells with a reading
  float cell_mv[MAX_CELLS];         // mV, NAN = no cell in this slot
  uint16_t balance_s[MAX_CELLS];    // balancing time left, seconds
  bool balancing_now[MAX_CELLS];
  uint32_t balance_cycle_left_s = 0;  // until the balancing cycle ends
  float cell_min = NAN, cell_max = NAN, cell_avg = NAN;  // V
  int module_count = 0;
  float module_temp[MAX_MODULES];   // degrees C
  float temp_min = NAN, temp_max = NAN;

  // Limits (A); NAN = not known, INFINITY = no limit
  float charge_limit = NAN, discharge_limit = NAN;
  const char *charge_limited_by = "";     // "temperature", "cell voltage", "working limit"...
  const char *discharge_limited_by = "";
  int charge_part_count = 0, discharge_part_count = 0;
  LimitPart charge_parts[MAX_LIMIT_PARTS], discharge_parts[MAX_LIMIT_PARTS];

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
  float bms_chip_temp = NAN, shunt_temp = NAN, shunt_ntc_temp = NAN, hmi_chip_temp = NAN;

  // Isolation between the pack and the chassis, measured with the contactors open
  float isolation_kohm = NAN;
  float isolation_pos_v = NAN, isolation_neg_v = NAN;  // pack + and pack - to chassis
  uint32_t isolation_age_s = 0;     // since the last open-contactor measurement

  // The link between this display and the BMS
  uint32_t link_crc_errors = 0, link_timeouts = 0;

  // Settings (as the BMS confirmed them) and the BMS log
  int setting_count = 0;
  const Setting *settings = nullptr;
  bool settings_unlocked = false;
  int log_count = 0;
  const char *const *log_lines = nullptr;
  bool log_enabled = false;
  uint8_t brightness = 68;          // %

  // Identity
  const char *pack_name = "";
  const char *bms_firmware = "";
  const char *hmi_firmware = "";
  const char *board_revision = "";
  const char *wifi_ssid = "";
  const char *ip = "";
  int wifi_rssi = 0;
  const char *clock = "";           // "14:32"; "" until the time is known
  uint32_t bms_uptime_s = 0, hmi_uptime_s = 0;

  const char *balance_charge_status = "";
  const char *bms_serial = "";
  const char *wifi_ap = "";

  Model() {
    for (auto &v : cell_mv) v = NAN;
    for (auto &v : balance_s) v = 0;
    for (auto &v : balancing_now) v = false;
    for (auto &v : module_temp) v = NAN;
    for (auto &v : history_soc) v = NAN;
    for (auto &v : history_kw) v = NAN;
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
