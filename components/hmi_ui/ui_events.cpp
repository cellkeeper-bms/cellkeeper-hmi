// The events' names as words, and one line on what each means. Keyed by the BMS's name for the
// event, so the texts stay with their events whatever order the firmware lists them in.
#include "ui_draw.h"

#include "../hmi_bms/hmi_map.h"

#include <cstring>

namespace hmiui {

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

const char *event_title(uint8_t type) {
  if (type >= esphome::hmi_bms::HMI_EVENT_COUNT) return "";
  // "CELL_VOLTAGES_STALE" -> "Cell Voltages Stale"
  static char buf[4][64];
  static int next = 0;
  char *out = buf[next];
  next = (next + 1) % 4;
  const char *name = esphome::hmi_bms::HMI_EVENT_NAMES[type];
  size_t i = 0;
  bool start = true, keep = false;
  for (; name[i] && i < sizeof(buf[0]) - 1; i++) {
    const char ch = name[i];
    if (ch == '_') {
      out[i] = ' ';
      start = true;
      continue;
    }
    if (start) {
      // Abbreviations and words with a digit in them ("3V3", "12V") stay in capitals
      size_t len = 0;
      bool digit = false;
      while (name[i + len] && name[i + len] != '_') {
        digit = digit || (name[i + len] >= '0' && name[i + len] <= '9');
        len++;
      }
      keep = digit || (len == 3 && (strncmp(name + i, "BMB", 3) == 0 || strncmp(name + i, "CRC", 3) == 0));
    }
    out[i] = (start || keep) ? ch : (char) (ch | 0x20);
    start = false;
  }
  out[i] = 0;
  return out;
}

const char *event_name(uint8_t type) {
  return type < esphome::hmi_bms::HMI_EVENT_COUNT ? esphome::hmi_bms::HMI_EVENT_NAMES[type] : "";
}

int event_type_count() { return esphome::hmi_bms::HMI_EVENT_COUNT; }

const char *event_help(uint8_t type) {
  if (type >= esphome::hmi_bms::HMI_EVENT_COUNT) return "";
  const char *name = esphome::hmi_bms::HMI_EVENT_NAMES[type];
  for (const auto &e : EVENT_HELP) {
    if (strcmp(e.name, name) == 0) return e.text;
  }
  return "";
}

}  // namespace hmiui
