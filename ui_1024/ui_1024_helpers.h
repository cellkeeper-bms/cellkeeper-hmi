// Helpers for the 1024x600 LVGL UI (ui_1024/scripts.yaml): event levels and texts,
// colours, and picking the current limit in force. No ESPHome ids in here.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace ui1024 {

// Event levels as the BMS sends them (hmi_bms EVENT_LEVELS order)
enum Level { LVL_INACTIVE = 0, LVL_INFO = 1, LVL_WARNING = 2, LVL_CRITICAL = 3, LVL_FATAL = 4 };

// Parses an event sensor state "WARNING (3)" / "INACTIVE (2)" / "INACTIVE" into level and count
inline void parse_event_state(const std::string &s, int &level, int &count) {
  level = LVL_INACTIVE;
  count = 0;
  if (s.rfind("FATAL", 0) == 0) level = LVL_FATAL;
  else if (s.rfind("CRITICAL", 0) == 0) level = LVL_CRITICAL;
  else if (s.rfind("WARNING", 0) == 0) level = LVL_WARNING;
  else if (s.rfind("INFO", 0) == 0) level = LVL_INFO;
  size_t p = s.find('(');
  if (p != std::string::npos) count = atoi(s.c_str() + p + 1);
}

// Severity group of an event type, from its sensor name "Event <Severity> <text>"
inline int name_group(const char *name) {
  if (strncmp(name, "Event Fatal", 11) == 0) return LVL_FATAL;
  if (strncmp(name, "Event Critical", 14) == 0) return LVL_CRITICAL;
  if (strncmp(name, "Event Warning", 13) == 0) return LVL_WARNING;
  if (strncmp(name, "Event Info", 10) == 0) return LVL_INFO;
  return LVL_WARNING;
}

// The sensor name without "Event <Severity> "
inline const char *short_name(const char *name) {
  if (strncmp(name, "Event ", 6) != 0) return name;
  const char *p = strchr(name + 6, ' ');
  return p ? p + 1 : name;
}

inline const char *level_name(int level) {
  switch (level) {
    case LVL_FATAL: return "FATAL";
    case LVL_CRITICAL: return "CRITICAL";
    case LVL_WARNING: return "WARNING";
    case LVL_INFO: return "INFO";
    default: return "INACTIVE";
  }
}

// HA dashboard colours: Fatal, Critical, Warning, Info; previously active is purple
inline uint32_t level_color(int level) {
  switch (level) {
    case LVL_FATAL: return 0xD50000;
    case LVL_CRITICAL: return 0xF44336;
    case LVL_WARNING: return 0xFFC107;
    case LVL_INFO: return 0x2196F3;
    default: return 0x26302B;
  }
}
static const uint32_t COLOR_PREVIOUS = 0x9C27B0;

// Text colour that reads on a level colour
inline uint32_t level_text_color(int level) { return (level == LVL_WARNING) ? 0x000000 : 0xFFFFFF; }

// One line on what each event means, by the BMS's name for it (HMIBMS::event_type_name), so the
// texts stay with their events whatever order the firmware lists them in. The host tests check
// that every event the firmware has is in here.
struct EventHelp {
  const char *name;
  const char *text;
};
static const EventHelp EVENT_HELP[] = {
    {"BOOT_NORMAL", "The BMS started normally."},
    {"BOOT_WATCHDOG", "The BMS restarted after a watchdog reset."},
    {"LOOP_OVERRUN", "The BMS main loop took longer than its time budget."},
    {"RESTARTING", "The BMS is restarting."},
    {"BOARD_MISMATCH", "The firmware's board setting does not match the board it runs on."},
    {"ESTOP_PRESSED", "The emergency stop input is active."},
    {"SUPPLY_VOLTAGE_STALE", "No fresh supply voltage readings arrived in time."},
    {"BATTERY_VOLTAGE_STALE", "No fresh battery voltage readings arrived in time."},
    {"CURRENT_STALE", "No fresh current readings arrived in time."},
    {"CELL_VOLTAGES_STALE", "No fresh cell voltage readings arrived in time."},
    {"BATTERY_TEMPERATURE_STALE", "No fresh battery temperature readings arrived in time."},
    {"SUPPLY_VOLTAGE_3V3_LOW", "The 3.3 V supply is below its limit."},
    {"SUPPLY_VOLTAGE_3V3_HIGH", "The 3.3 V supply is above its limit."},
    {"SUPPLY_VOLTAGE_5V_LOW", "The 5 V supply is below its limit."},
    {"SUPPLY_VOLTAGE_5V_HIGH", "The 5 V supply is above its limit."},
    {"SUPPLY_VOLTAGE_12V_LOW", "The 12 V supply is below its limit."},
    {"SUPPLY_VOLTAGE_12V_HIGH", "The 12 V supply is above its limit."},
    {"SUPPLY_VOLTAGE_CONTACTOR_VERY_LOW",
     "The contactor supply voltage is very low: the contactors may not hold."},
    {"SUPPLY_VOLTAGE_CONTACTOR_LOW", "The contactor supply voltage is low."},
    {"SUPPLY_VOLTAGE_CONTACTOR_HIGH", "The contactor supply voltage is high."},
    {"BATTERY_VOLTAGE_VERY_LOW", "Pack voltage is below its very-low limit."},
    {"BATTERY_VOLTAGE_HIGH", "Pack voltage is above its high limit."},
    {"BATTERY_VOLTAGE_VERY_HIGH", "Pack voltage is above its very-high limit."},
    {"VOLTAGE_MISMATCH", "The pack voltage and the sum of the module voltages disagree."},
    {"LINK_VOLTAGE_MISMATCH", "The link voltage check found a mismatch."},
    {"FUSE_VOLTAGE_DROP_HIGH", "The voltage drop across the fuse is higher than expected."},
    {"OVERCURRENT_CHARGING", "Charge current exceeded its limit."},
    {"OVERCURRENT_DISCHARGING", "Discharge current exceeded its limit."},
    {"CELL_VOLTAGE_VERY_LOW", "A cell voltage is below its very-low limit."},
    {"CELL_VOLTAGE_LOW", "A cell voltage is below its low limit."},
    {"CELL_VOLTAGE_HIGH", "A cell voltage is above its high limit."},
    {"CELL_VOLTAGE_VERY_HIGH", "A cell voltage is above its very-high limit."},
    {"SOFT_CHARGE_BUFFER_EXCEEDED", "Charging went past the soft charge buffer."},
    {"CELL_MISSING", "A cell the pack should have gives no reading."},
    {"ABSENT_CHANNEL_VOLTAGE",
     "A cell channel that should be empty reads a voltage: a cell connected there would go unchecked."},
    {"BATTERY_TEMPERATURE_VERY_LOW", "Battery temperature is below its very-low limit."},
    {"BATTERY_TEMPERATURE_LOW", "Battery temperature is below its low limit."},
    {"BATTERY_TEMPERATURE_HIGH", "Battery temperature is above its high limit."},
    {"BATTERY_TEMPERATURE_VERY_HIGH", "Battery temperature is above its very-high limit."},
    {"BMB_READ_ERROR", "A battery module board (BMB) did not answer."},
    {"BMB_CRC_MISMATCH", "Data from a BMB failed its checksum."},
    {"CELL_VOLTAGE_GLITCH", "A cell voltage reading jumped implausibly and was ignored."},
    {"MODULE_TEMPERATURE_GLITCH", "A module temperature reading jumped implausibly and was ignored."},
    {"BMB_PACK_MISMATCH", "The cells the BMBs report do not match the pack this firmware was built for."},
    {"ISOLATION_LOW", "Isolation between the pack and the chassis is low (measured with the contactors open)."},
    {"CONTACTOR_POS_STUCK_OPEN", "The positive contactor stayed open when it should have closed."},
    {"CONTACTOR_POS_STUCK_CLOSED",
     "The positive contactor stayed closed when it should have opened (possibly welded)."},
    {"CONTACTOR_NEG_STUCK_OPEN", "The negative contactor stayed open when it should have closed."},
    {"CONTACTOR_NEG_STUCK_CLOSED",
     "The negative contactor stayed closed when it should have opened (possibly welded)."},
    {"CONTACTOR_PRE_STUCK_OPEN", "The precharge contactor stayed open when it should have closed."},
    {"CONTACTOR_PRE_STUCK_CLOSED", "The precharge contactor stayed closed when it should have opened."},
    {"CONTACTOR_PRECHARGE_VOLTAGE_TOO_HIGH",
     "Precharge ended with too much voltage across the contactors: the load did not charge up to pack voltage."},
    {"CONTACTOR_PRECHARGE_CURRENT_TOO_HIGH", "Too much current flowed during precharge."},
    {"CONTACTOR_POS_UNEXPECTED_OPEN", "The positive contactor opened without being told to."},
    {"CONTACTOR_NEG_UNEXPECTED_OPEN", "The negative contactor opened without being told to."},
    {"CONTACTOR_CLOSING_FAILED", "The contactor closing sequence did not complete."},
    {"SUPERVISOR_NOT_ENABLED",
     "Closing is held off: the hardware supervisor is not enabling the contactor drivers."},
    {"SUPERVISOR_DISABLED_CONTACTORS",
     "The hardware supervisor cut the contactor drivers while the contactors were being driven."},
    {"INVERTER_DETECTED", "An inverter is communicating with the BMS."},
};
inline const char *event_help(const char *bms_name) {
  for (const auto &e : EVENT_HELP) {
    if (strcmp(e.name, bms_name) == 0) return e.text;
  }
  return "";
}

inline uint32_t lerp_color(uint32_t a, uint32_t b, float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
  int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
  int r = ar + (int) std::lround((br - ar) * t);
  int g = ag + (int) std::lround((bg - ag) * t);
  int bl = ab + (int) std::lround((bb - ab) * t);
  return ((uint32_t) r << 16) | ((uint32_t) g << 8) | (uint32_t) bl;
}

// Cell colour ramp, t = 0 (lowest cell) .. 1 (highest): blue, mint, amber
inline uint32_t cell_ramp(float t) {
  if (t <= 0.5f) return lerp_color(0x3A7BD5, 0x5EE3A8, t * 2.0f);
  return lerp_color(0x5EE3A8, 0xFFB74D, (t - 0.5f) * 2.0f);
}

// Module temperature colour
inline uint32_t temp_color(float c) {
  if (std::isnan(c)) return 0x26302B;
  if (c < 5.0f) return 0x3A7BD5;
  if (c < 35.0f) return 0x2E7D5B;
  if (c < 45.0f) return 0xFFB300;
  return 0xE53935;
}

// A current limit counts when it's a number below the BMS "no limit" value (6553.5 A)
inline bool limit_valid(float a) { return !std::isnan(a) && a >= 0.0f && a < 6500.0f; }

// Index of the lowest valid limit in vals[0..n), or -1
inline int lowest_limit(const float *vals, int n) {
  int best = -1;
  for (int i = 0; i < n; i++) {
    if (!limit_valid(vals[i])) continue;
    if (best < 0 || vals[i] < vals[best]) best = i;
  }
  return best;
}

// "1d 02:03:04" / "02:03:04" from seconds
inline std::string fmt_duration(float s) {
  if (std::isnan(s) || s < 0) return "--";
  unsigned long t = (unsigned long) s;
  unsigned long d = t / 86400, h = (t / 3600) % 24, m = (t / 60) % 60, sec = t % 60;
  char buf[32];
  if (d > 0) snprintf(buf, sizeof(buf), "%lud %02lu:%02lu", d, h, m);
  else snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, sec);
  return buf;
}

// Settings shown on the Settings page and edited with the numeric pad. reg is the HMI
// register (100+ for the HMI's own balance-charge numbers); value shown = entity * mult.
struct SettingDef {
  int reg;
  int card;  // 0 limits, 1 SoC, 2 balancing, 3 balance charge
  const char *title;
  const char *unit;
  const char *fmt;
  float mult;
};
static const SettingDef SETTINGS[] = {
    {43, 0, "Max charge current", "A", "%.1f", 1.0f},
    {44, 0, "Max discharge current", "A", "%.1f", 1.0f},
    {25, 0, "Cell working min", "V", "%.3f", 0.001f},
    {26, 0, "Cell working max", "V", "%.3f", 0.001f},
    {31, 0, "Cell soft min", "V", "%.3f", 0.001f},
    {32, 0, "Cell soft max", "V", "%.3f", 0.001f},
    {29, 0, "Voltage offset lower", "V", "%.1f", 0.1f},
    {30, 0, "Voltage offset upper", "V", "%.1f", 0.1f},
    {27, 1, "SoC scaling min", "%", "%.0f", 1.0f},
    {28, 1, "SoC scaling max", "%", "%.0f", 1.0f},
    {3, 1, "Set SoC now", "%", "%.0f", 1.0f},
    {42, 2, "Balancing start voltage", "V", "%.3f", 0.001f},
    {36, 2, "Min balancing voltage", "V", "%.3f", 0.001f},
    {35, 2, "Balance min offset", "mV", "%.0f", 1.0f},
    {33, 2, "Auto balance period", "ms", "%.0f", 1.0f},
    {34, 2, "Balancing periods per mV", "", "%.0f", 1.0f},
    {101, 3, "Interval", "days", "%.0f", 1.0f},
    {102, 3, "Start hour", "h", "%.0f", 1.0f},
    {103, 3, "Duration", "h", "%.0f", 1.0f},
    {104, 3, "Float offset upper", "V", "%.1f", 0.1f},
    {105, 3, "Balance charge offset upper", "V", "%.1f", 0.1f},
};
static const int SETTINGS_COUNT = sizeof(SETTINGS) / sizeof(SETTINGS[0]);

inline const SettingDef *setting_def(int reg) {
  for (int i = 0; i < SETTINGS_COUNT; i++)
    if (SETTINGS[i].reg == reg) return &SETTINGS[i];
  return nullptr;
}

// The number entity behind each register, filled at boot (void * keeps this header free of ESPHome types)
inline void *&setting_slot(int reg) {
  static void *slots[128] = {};
  return slots[reg & 127];
}

}  // namespace ui1024
