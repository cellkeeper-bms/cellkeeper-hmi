#include "hmi_bms.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"
#include <ctime>
#include <vector>

namespace esphome {
namespace hmi_bms {

static const char *const TAG = "hmi_bms";

static const char* EVENT_LEVELS[] = {
    "INACTIVE",
    "INFO",
    "WARNING",
    "CRITICAL",
    "FATAL"
};

static const char* EVENT_TYPES[] = {
    "CONTACTOR_POS_STUCK_OPEN",
    "CONTACTOR_POS_STUCK_CLOSED",
    "CONTACTOR_NEG_STUCK_OPEN",
    "CONTACTOR_NEG_STUCK_CLOSED",
    "CONTACTOR_PRE_STUCK_OPEN",
    "CONTACTOR_PRE_STUCK_CLOSED",
    "CONTACTOR_PRECHARGE_VOLTAGE_TOO_HIGH",
    "CONTACTOR_PRECHARGE_CURRENT_TOO_HIGH",
    "CONTACTOR_POS_UNEXPECTED_OPEN",
    "CONTACTOR_NEG_UNEXPECTED_OPEN",
    "CONTACTOR_CLOSING_FAILED",
    "SUPPLY_VOLTAGE_STALE",
    "BATTERY_VOLTAGE_STALE",
    "BATTERY_TEMPERATURE_STALE",
    "CURRENT_STALE",
    "CELL_VOLTAGES_STALE",
    "SUPPLY_VOLTAGE_3V3_LOW",
    "SUPPLY_VOLTAGE_3V3_HIGH",
    "SUPPLY_VOLTAGE_5V_LOW",
    "SUPPLY_VOLTAGE_5V_HIGH",
    "SUPPLY_VOLTAGE_12V_LOW",
    "SUPPLY_VOLTAGE_12V_HIGH",
    "SUPPLY_VOLTAGE_CONTACTOR_LOW",
    "SUPPLY_VOLTAGE_CONTACTOR_VERY_LOW",
    "SUPPLY_VOLTAGE_CONTACTOR_HIGH",
    "BATTERY_VOLTAGE_HIGH",
    "BATTERY_VOLTAGE_VERY_HIGH",
    "BATTERY_VOLTAGE_LOW",
    "BATTERY_VOLTAGE_VERY_LOW",
    "CELL_VOLTAGE_HIGH",
    "CELL_VOLTAGE_VERY_HIGH",
    "CELL_VOLTAGE_LOW",
    "CELL_VOLTAGE_VERY_LOW",
    "SOFT_CHARGE_BUFFER_EXCEEDED",
    "OVERCURRENT_CHARGING",
    "OVERCURRENT_DISCHARGING",
    "BATTERY_TEMPERATURE_HIGH",
    "BATTERY_TEMPERATURE_VERY_HIGH",
    "BATTERY_TEMPERATURE_LOW",
    "BATTERY_TEMPERATURE_VERY_LOW",
    "VOLTAGE_MISMATCH",
    "BMB_READ_ERROR",
    "BMB_CRC_MISMATCH",
    "CELL_VOLTAGE_GLITCH",
    "MODULE_TEMPERATURE_GLITCH",
    "INVERTER_DETECTED",
    "ESTOP_PRESSED",
    "BOOT_NORMAL",
    "BOOT_WATCHDOG",
    "LOOP_OVERRUN",
    "RESTARTING",
    "LINK_VOLTAGE_MISMATCH",
    "FUSE_VOLTAGE_DROP_HIGH",
    "ISOLATION_ASYMMETRY",
    "CONTACTOR_PORT_ENERGIZED",
    "BOARD_MISMATCH",
    "SUPERVISOR_NOT_ENABLED",
    "SUPERVISOR_DISABLED_CONTACTORS",
    "BMB_MODULE_ID_CHECK_FAILED",
    "BMB_MODULE_ID_UNKNOWN",
    "BMB_MODULE_ID_CONFLICT"
};

const char *HMIBMS::event_type_name(int type) {
  if (type < 0 || type >= (int) (sizeof(EVENT_TYPES) / sizeof(EVENT_TYPES[0]))) return "UNKNOWN";
  return EVENT_TYPES[type];
}

#define MAX_EVENT_TYPE_ID 69

// "2026-10-22 00:39 UTC", or "" for 0
static std::string format_build_time(time_t t) {
  if (t <= 0) return "";
  struct tm tm_utc;
  gmtime_r(&t, &tm_utc);
  char buf[24];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M UTC", &tm_utc);
  return buf;
}

void HMIBMS::setup() {
  if (this->hmi_firmware_text_sensor_ != nullptr) {
    std::string built = format_build_time(App.get_build_time());
    this->hmi_firmware_text_sensor_->publish_state(built.empty() ? this->hmi_build_ : this->hmi_build_ + " · " + built);
  }
}

// "dab3751 · 2026-10-22 00:39 UTC", "dab3751-dirty · ...", or "unknown" when the BMS has no ID
void HMIBMS::publish_bms_firmware_() {
  if (this->bms_firmware_text_sensor_ == nullptr || !this->bms_build_hash_seen_) return;
  std::string text;
  if ((this->bms_build_hash_ & 0x0FFFFFFF) == 0) {
    text = "unknown";
  } else {
    char hash[16];
    snprintf(hash, sizeof(hash), "%07x%s", (unsigned) (this->bms_build_hash_ & 0x0FFFFFFF),
             (this->bms_build_hash_ & 0x80000000u) ? "-dirty" : "");
    text = hash;
  }
  std::string built = format_build_time(this->bms_build_time_);
  if (!built.empty()) text += " · " + built;
  if (!this->bms_firmware_text_sensor_->has_state() || this->bms_firmware_text_sensor_->state != text) {
    this->bms_firmware_text_sensor_->publish_state(text);
  }
}

void HMIBMS::update() {
  if (this->address_ == 0) {
    ESP_LOGD(TAG, "No device address assigned yet, skipping register read");
    return;
  }

  // Use slot 0 on first update so slow/detail registers are available
  // immediately after boot/reconnect (instead of after ~10 poll cycles).
  uint8_t slot = this->update_count_ % 10;
  this->update_count_++;

  // 1. Prepare Register Read Payload (Combines Slow and Fast polls)
  std::vector<uint8_t> payload;
  payload.push_back(HMI_MSG_READ_REGISTERS);
  payload.push_back(this->address_);
  
  auto add_reg = [&](uint16_t reg) {
    payload.push_back(reg & 0xFF); // Lo byte first (Little Endian)
    payload.push_back(reg >> 8);   // Hi byte
  };

  // Add Slow Poll Registers only on Tick 0
  if (slot == 0) {
    // Always poll these basic sensors if configured
    if (this->millis_sensor_) add_reg(HMI_REG_MILLIS);
    if (this->serial_text_sensor_) add_reg(HMI_REG_SERIAL);
    // PCB revision strap (registers 66/67; older firmware just doesn't answer)
    if (this->board_revision_text_sensor_) add_reg(HMI_REG_BOARD_REVISION);
    if (this->board_strap_counts_sensor_) add_reg(HMI_REG_BOARD_STRAP_COUNTS);
    // Board temperatures (70 is new; older firmware doesn't answer it)
    if (this->bms_chip_temperature_sensor_) add_reg(HMI_REG_MCU_TEMPERATURE);
    if (this->shunt_die_temperature_sensor_) add_reg(HMI_REG_SHUNT_DIE_TEMP);
    if (this->shunt_ntc_temperature_sensor_) add_reg(HMI_REG_SHUNT_NTC_TEMP);
    // Firmware build ID (registers 68/69; older firmware doesn't answer)
    if (this->bms_firmware_text_sensor_) {
      add_reg(HMI_REG_BUILD_HASH);
      add_reg(HMI_REG_BUILD_TIME);
    }
    if (this->system_request_sensor_) add_reg(HMI_REG_SYSTEM_REQUEST);
    if (this->soc_sensor_ || this->soc_number_) add_reg(HMI_REG_SOC);
    if (this->charge_raw_sensor_) add_reg(HMI_REG_CHARGE);
    if (this->temperature_min_dC_sensor_) add_reg(HMI_REG_TEMPERATURE_MIN);
    if (this->temperature_max_dC_sensor_) add_reg(HMI_REG_TEMPERATURE_MAX);
    if (this->cell_voltage_min_mV_sensor_) add_reg(HMI_REG_CELL_VOLTAGE_MIN);
    if (this->cell_voltage_max_mV_sensor_) add_reg(HMI_REG_CELL_VOLTAGE_MAX);
    if (this->system_sm_sensor_) add_reg(HMI_REG_SYSTEM_STATE);
    if (this->soc_voltage_based_sensor_) add_reg(HMI_REG_SOC_VOLTAGE_BASED);
    if (this->soc_basic_count_sensor_) add_reg(HMI_REG_SOC_BASIC_COUNT);
    if (this->capacity_mC_sensor_) add_reg(HMI_REG_CAPACITY);
    if (this->supply_voltage_3V3_mV_sensor_) add_reg(HMI_REG_SUPPLY_VOLTAGE_3V3);
    if (this->supply_voltage_5V_mV_sensor_) add_reg(HMI_REG_SUPPLY_VOLTAGE_5V);
    if (this->supply_voltage_12V_mV_sensor_) add_reg(HMI_REG_SUPPLY_VOLTAGE_12V);
    if (this->supply_voltage_contactor_mV_sensor_) add_reg(HMI_REG_SUPPLY_VOLTAGE_CTR);
    // Isolation monitor, all calculated on the BMS (registers 62-64; older firmware just doesn't answer)
    if (this->isolation_resistance_sensor_) add_reg(HMI_REG_ISOLATION_RESISTANCE);
    if (this->pack_pos_to_chassis_sensor_) add_reg(HMI_REG_PACK_POS_TO_CHASSIS);
    if (this->pack_neg_to_chassis_sensor_) add_reg(HMI_REG_PACK_NEG_TO_CHASSIS);
    if (this->isolation_check_age_sensor_) add_reg(HMI_REG_ISOLATION_CHECK_AGE);
    if (this->inverter_soc_sensor_) add_reg(HMI_REG_INVERTER_SOC);
    if (this->inverter_full_capacity_sensor_) add_reg(HMI_REG_INVERTER_FULL_CAPACITY_DAH);
    if (this->inverter_remaining_capacity_sensor_) add_reg(HMI_REG_INVERTER_REMAINING_CAPACITY_DAH);
    if (this->inverter_min_voltage_limit_sensor_) add_reg(HMI_REG_INVERTER_MIN_VOLTAGE_LIMIT_DV);
    if (this->inverter_max_voltage_limit_sensor_) add_reg(HMI_REG_INVERTER_MAX_VOLTAGE_LIMIT_DV);
    if (this->pos_contactor_voltage_mV_sensor_) add_reg(HMI_REG_POS_CONTACTOR_VOLTAGE);
    if (this->neg_contactor_voltage_mV_sensor_) add_reg(HMI_REG_NEG_CONTACTOR_VOLTAGE);
    // Settings numbers are read in their own request (Tick 4)

    // Add configured raw temperature sensors
    for (size_t i = 0; i < this->raw_temperature_sensors_.size(); i++) {
      if (this->raw_temperature_sensors_[i] != nullptr) {
        add_reg(HMI_REG_RAW_TEMPS_START + i);
      }
    }
  }

  // Add Fast Poll Registers (Every Tick)
  if (this->current_mA_sensor_) add_reg(HMI_REG_CURRENT);
  if (this->battery_voltage_mV_sensor_) add_reg(HMI_REG_BATTERY_VOLTAGE);
  if (this->output_voltage_mV_sensor_) add_reg(HMI_REG_OUTPUT_VOLTAGE);
  if (this->contactor_sm_sensor_) add_reg(HMI_REG_CONTACTORS_STATE);

  if (payload.size() > 2) {
    this->send_packet_(payload);
  }

  // 2. Read current-limit registers (Tick 3) in a dedicated request.
  // This keeps them on the slow cadence while avoiding truncation in
  // the larger slot-0 slow poll packet.
  if (slot == 3) {
    std::vector<uint8_t> limit_payload;
    limit_payload.push_back(HMI_MSG_READ_REGISTERS);
    limit_payload.push_back(this->address_);
    auto add_limit_reg = [&](uint16_t reg) {
      limit_payload.push_back(reg & 0xFF);
      limit_payload.push_back(reg >> 8);
    };

    if (this->max_charge_current_number_) add_limit_reg(HMI_REG_USER_CHARGE_CURRENT_LIMIT_DA);
    if (this->max_discharge_current_number_) add_limit_reg(HMI_REG_USER_DISCHARGE_CURRENT_LIMIT_DA);
    if (this->charge_current_limit_sensor_) add_limit_reg(HMI_REG_CHARGE_CURRENT_LIMIT_DA);
    if (this->discharge_current_limit_sensor_) add_limit_reg(HMI_REG_DISCHARGE_CURRENT_LIMIT_DA);
    if (this->temp_charge_current_limit_sensor_) add_limit_reg(HMI_REG_TEMP_CHARGE_CURRENT_LIMIT_DA);
    if (this->temp_discharge_current_limit_sensor_) add_limit_reg(HMI_REG_TEMP_DISCHARGE_CURRENT_LIMIT_DA);
    if (this->cell_voltage_charge_current_limit_sensor_) add_limit_reg(HMI_REG_CELL_VOLTAGE_CHARGE_CURRENT_LIMIT_DA);
    if (this->cell_voltage_discharge_current_limit_sensor_) add_limit_reg(HMI_REG_CELL_VOLTAGE_DISCHARGE_CURRENT_LIMIT_DA);
    if (this->working_charge_current_limit_sensor_) add_limit_reg(HMI_REG_WORKING_CHARGE_CURRENT_LIMIT_DA);
    if (this->working_discharge_current_limit_sensor_) add_limit_reg(HMI_REG_WORKING_DISCHARGE_CURRENT_LIMIT_DA);
    if (this->delta_charge_current_limit_sensor_) add_limit_reg(HMI_REG_DELTA_CHARGE_CURRENT_LIMIT_DA);
    if (this->hard_charge_current_limit_sensor_ || this->max_charge_current_number_) {
      add_limit_reg(HMI_REG_HARD_CHARGE_CURRENT_LIMIT_DA);
    }
    if (this->hard_discharge_current_limit_sensor_ || this->max_discharge_current_number_) {
      add_limit_reg(HMI_REG_HARD_DISCHARGE_CURRENT_LIMIT_DA);
    }

    if (limit_payload.size() > 2) {
      this->send_packet_(limit_payload);
    }
  }

  // Read the settings numbers (Tick 4) in a dedicated request. In the slot-0
  // slow poll the BMS's reply reached its 256-byte buffer when every register
  // was available, and it drops whatever doesn't fit (Balancing Start
  // Voltage never arrived on ck1).
  if (slot == 4) {
    std::vector<uint8_t> settings_payload;
    settings_payload.push_back(HMI_MSG_READ_REGISTERS);
    settings_payload.push_back(this->address_);
    auto add_settings_reg = [&](uint16_t reg) {
      settings_payload.push_back(reg & 0xFF);
      settings_payload.push_back(reg >> 8);
    };

    if (this->cell_voltage_working_min_number_) add_settings_reg(HMI_REG_CELL_VOLTAGE_WORKING_MIN);
    if (this->cell_voltage_working_max_number_) add_settings_reg(HMI_REG_CELL_VOLTAGE_WORKING_MAX);
    if (this->soc_scaling_min_number_) add_settings_reg(HMI_REG_SOC_SCALING_MIN);
    if (this->soc_scaling_max_number_) add_settings_reg(HMI_REG_SOC_SCALING_MAX);
    if (this->voltage_limit_offset_lower_number_) add_settings_reg(HMI_REG_VOLTAGE_LIMIT_OFFSET_LOWER);
    if (this->voltage_limit_offset_upper_number_) add_settings_reg(HMI_REG_VOLTAGE_LIMIT_OFFSET_UPPER);
    if (this->cell_voltage_soft_min_number_) add_settings_reg(HMI_REG_CELL_VOLTAGE_SOFT_MIN);
    if (this->cell_voltage_soft_max_number_) add_settings_reg(HMI_REG_CELL_VOLTAGE_SOFT_MAX);
    if (this->auto_balancing_period_ms_number_) add_settings_reg(HMI_REG_AUTO_BALANCING_PERIOD_MS);
    if (this->balancing_periods_per_mV_number_) add_settings_reg(HMI_REG_BALANCING_PERIODS_PER_MV);
    if (this->balance_min_offset_mV_number_) add_settings_reg(HMI_REG_BALANCE_MIN_OFFSET_MV);
    if (this->minimum_balancing_voltage_mV_number_) add_settings_reg(HMI_REG_MINIMUM_BALANCING_VOLTAGE_MV);
    if (this->balancing_start_voltage_mV_number_) add_settings_reg(HMI_REG_BALANCING_START_VOLTAGE_MV);

    if (settings_payload.size() > 2) {
      this->send_packet_(settings_payload);
    }
  }

  // 3. Read Module Temperatures (Tick 1) in a dedicated request.
  // Module temps are now float payloads, so keeping them separate avoids
  // response truncation when the slot-0 slow poll is large.
  if (slot == 1 && !this->module_temperature_sensors_.empty()) {
    std::vector<uint8_t> mt_payload;
    mt_payload.push_back(HMI_MSG_READ_REGISTERS);
    mt_payload.push_back(this->address_);
    for (size_t i = 0; i < this->module_temperature_sensors_.size(); i++) {
      if (this->module_temperature_sensors_[i] != nullptr) {
        uint16_t reg = HMI_REG_MODULE_TEMPS_START + i;
        mt_payload.push_back(reg & 0xFF);
        mt_payload.push_back(reg >> 8);
      }
    }
    if (mt_payload.size() > 2) {
      this->send_packet_(mt_payload);
    }
  }

  // 4. Read Cell Voltages (Tick 2)
  if (slot == 2 && !this->cell_voltage_sensors_.empty()) {
    ESP_LOGV(TAG, "Polling cell voltages (slot 2). Sensors: %zu", this->cell_voltage_sensors_.size());
    std::vector<uint8_t> cv_payload;
    cv_payload.push_back(HMI_MSG_READ_CELL_VOLTAGES);
    cv_payload.push_back(this->address_);
    this->send_packet_(cv_payload);
  }

  // 5. Read Events (Tick 5): start a new sweep of the event list from index 0. A sweep
  // that lost a page is simply restarted here.
  if (slot == 5) {
    auto &sw = this->event_sweep_;
    sw.active = true;
    sw.pages = 0;
    sw.seen.assign(this->error_sensors_.size(), false);
    for (auto &l : sw.by_level) l.clear();
    sw.highest_level = 0;
    sw.latest_type = -1;
    sw.latest_level = 0;
    sw.latest_count = 0;
    this->request_events_(0);
  }

  // 6. Read Balancing Times (Tick 7)
  if (slot == 7 && (this->cells_balancing_text_sensor_ || this->cells_balancing_active_text_sensor_ ||
                    !this->balancing_time_sensors_.empty())) {
    std::vector<uint8_t> bal_payload;
    bal_payload.push_back(HMI_MSG_READ_BALANCING_TIMES);
    bal_payload.push_back(this->address_);
    this->send_packet_(bal_payload);
  }

  // 7. Read Log (Tick 9) - Poll for new log messages
  //    Send 2 requests back-to-back to double throughput (each response carries up to 256 bytes,
  //    so 2 requests can drain up to 512 bytes per round-trip instead of 256).
  //    Flow control uses log_busy_ which clears when a non-full response arrives (<240 bytes),
  //    so this also works if the BMS sends multiple responses per single request.
  if (slot == 9 && this->log_enabled_ && (this->log_text_sensor_ || !this->log_line_sensors_.empty())) {
    if (this->log_busy_) {
      if (millis() - this->last_log_request_time_ > 2000) {
        ESP_LOGW(TAG, "BMS Log Request Timed Out");
        this->log_busy_ = false;
      }
    }

    if (!this->log_busy_) {
      ESP_LOGI(TAG, "Sending 2x BMS Log Requests");
      std::vector<uint8_t> log_payload;
      log_payload.push_back(HMI_MSG_READ_LOG);
      log_payload.push_back(this->address_);
      this->send_packet_(log_payload);
      this->send_packet_(log_payload);
      this->log_busy_ = true;
      this->last_log_request_time_ = millis();
    }
  }
}

void HMIBMS::send_packet_(const std::vector<uint8_t> &payload) {
  if (payload.empty()) return;
  uint8_t len_minus_1 = payload.size() - 1;
  std::vector<uint8_t> frame;
  frame.push_back(0xFF); // Sync byte included in CRC
  frame.push_back(len_minus_1);
  frame.insert(frame.end(), payload.begin(), payload.end());
  
  // Outgoing CRC includes the 0xFF sync byte and length byte
  uint16_t crc = this->crc16_modbus_(frame.data(), frame.size());
  
  frame.push_back(crc & 0xFF);
  frame.push_back(crc >> 8);

  ESP_LOGD(TAG, "Sending packet: %s (payload_len=%zu)", 
           format_hex_pretty(frame).c_str(), payload.size());

  this->write_array(frame);
}

void HMIBMS::send_system_request(uint8_t request) {
  if (this->address_ == 0) {
    ESP_LOGW(TAG, "Cannot send system request: No device address assigned");
    return;
  }
  ESP_LOGI(TAG, "Sending system request: %u", request);
  std::vector<uint8_t> payload;
  payload.push_back(HMI_MSG_WRITE_REGISTERS);
  payload.push_back(this->address_);
  payload.push_back(HMI_REG_SYSTEM_REQUEST & 0xFF); // Reg Lo
  payload.push_back(HMI_REG_SYSTEM_REQUEST >> 8);   // Reg Hi
  payload.push_back(HMI_TYPE_UINT8);
  payload.push_back(request);
  this->send_packet_(payload);
  
  if (this->system_request_sensor_) {
    this->publish_queue_.push_back({this->system_request_sensor_, (float)request});
  }
}

void HMIBMS::write_register(uint16_t reg_id, uint8_t type, float value, float scale, bool bypass_lock) {
  if (this->address_ == 0) {
    ESP_LOGW(TAG, "Cannot write register: No device address assigned");
    return;
  }

  if (!bypass_lock && !this->settings_unlocked_) {
    ESP_LOGW(TAG, "Cannot write register %u: Settings are LOCKED. Enable 'Settings Unlock' first.", reg_id);
    return;
  }
  
  // scale is what the raw register value is multiplied by to give the entity value
  // (e.g. 0.01 for SoC in 0.01 %), so raw = value / scale. Rounded, not truncated:
  // in float maths 98 / 0.01 can come out as 9799.9999.
  const double scaled = (scale != 0.0f) ? (double) value / (double) scale : (double) value;
  int64_t raw_val = llround(scaled);

  // Keep the value inside what the register type can hold
  int64_t lo = 0, hi = 0;
  switch (type) {
    case HMI_TYPE_UINT8: lo = 0; hi = UINT8_MAX; break;
    case HMI_TYPE_INT8: lo = INT8_MIN; hi = INT8_MAX; break;
    case HMI_TYPE_UINT16: lo = 0; hi = UINT16_MAX; break;
    case HMI_TYPE_INT16: lo = INT16_MIN; hi = INT16_MAX; break;
    case HMI_TYPE_UINT32: lo = 0; hi = UINT32_MAX; break;
    case HMI_TYPE_INT32: lo = INT32_MIN; hi = INT32_MAX; break;
    default:
      ESP_LOGW(TAG, "Cannot write register %u: unsupported type 0x%02X", reg_id, type);
      return;
  }
  if (raw_val < lo || raw_val > hi) {
    ESP_LOGW(TAG, "Register %u: raw value %lld outside its type's range, clamped", reg_id, (long long) raw_val);
    raw_val = raw_val < lo ? lo : hi;
  }

  ESP_LOGI(TAG, "Writing register %u: %f (raw: %lld)", reg_id, value, (long long) raw_val);

  std::vector<uint8_t> payload;
  payload.push_back(HMI_MSG_WRITE_REGISTERS);
  payload.push_back(this->address_);
  payload.push_back(reg_id & 0xFF);
  payload.push_back(reg_id >> 8);
  payload.push_back(type);

  if (type == HMI_TYPE_UINT8 || type == HMI_TYPE_INT8) {
    payload.push_back((uint8_t)raw_val);
  } else if (type == HMI_TYPE_UINT16 || type == HMI_TYPE_INT16) {
    payload.push_back((uint8_t)(raw_val & 0xFF));
    payload.push_back((uint8_t)((raw_val >> 8) & 0xFF));
  } else if (type == HMI_TYPE_UINT32 || type == HMI_TYPE_INT32) {
    payload.push_back((uint8_t)(raw_val & 0xFF));
    payload.push_back((uint8_t)((raw_val >> 8) & 0xFF));
    payload.push_back((uint8_t)((raw_val >> 16) & 0xFF));
    payload.push_back((uint8_t)((raw_val >> 24) & 0xFF));
  }
  
  this->send_packet_(payload);
  if (!bypass_lock) {
    // A user write keeps the unlock window open; an automation's doesn't
    this->last_settings_write_ms_ = millis();
  }
}

void HMIBMS::set_settings_unlocked(bool unlocked) {
  this->settings_unlocked_ = unlocked;
  this->last_settings_countdown_s_ = 0xFFFFFFFFUL;
  if (unlocked) {
    // Start/restart inactivity timeout window when unlock mode is enabled.
    this->last_settings_write_ms_ = millis();
    if (this->settings_status_text_sensor_ != nullptr) {
      this->settings_status_text_sensor_->publish_state("Settings Unlocked (30s)");
    }
    return;
  }
  this->publish_settings_status(unlocked ? "Settings unlocked" : "Settings locked");
}

void HMIBMS::publish_settings_status(const char *status) {
  this->settings_status_restore_pending_ = false;
  if (this->settings_status_text_sensor_ != nullptr) {
    this->settings_status_text_sensor_->publish_state(status);
  }
}

void HMIBMS::publish_settings_status_temporary(const char *status, uint32_t duration_ms) {
  if (this->settings_status_text_sensor_ != nullptr) {
    this->settings_status_text_sensor_->publish_state(status);
  }
  this->settings_status_restore_pending_ = true;
  this->settings_status_restore_at_ = millis() + duration_ms;
}

void HMIBMSNumber::write_from_automation(float value) {
  this->parent_->write_register(this->reg_id_, this->type_, value, this->scale_, true);
}

void HMIBMSNumber::control(float value) {
  if (!this->parent_->is_settings_unlocked()) {
    // Revert HA display back to last value confirmed from HMI/BMS bus.
    this->revert_to_bus_state();
    this->parent_->publish_settings_status_temporary("Settings locked (unlock to apply)", 10000);
    // Also trigger an immediate poll so HA gets authoritative readback quickly.
    this->parent_->update();
    return;
  }

  // Enforce user current limits against hard limits when available.
  float out_value = value;
  bool clamped_to_hard_limit = false;
  if (this->reg_id_ == HMI_REG_USER_CHARGE_CURRENT_LIMIT_DA &&
      this->parent_->has_hard_charge_current_limit()) {
    const float hard_limit = this->parent_->get_hard_charge_current_limit_a();
    if (!std::isnan(hard_limit) && out_value > hard_limit) {
      out_value = hard_limit;
      clamped_to_hard_limit = true;
    }
  } else if (this->reg_id_ == HMI_REG_USER_DISCHARGE_CURRENT_LIMIT_DA &&
             this->parent_->has_hard_discharge_current_limit()) {
    const float hard_limit = this->parent_->get_hard_discharge_current_limit_a();
    if (!std::isnan(hard_limit) && out_value > hard_limit) {
      out_value = hard_limit;
      clamped_to_hard_limit = true;
    }
  }

  // SOC setpoint is intended as a command-style input.
  // Keep the entered value visible here; live SOC remains on the SOC sensor.
  if (this->reg_id_ == HMI_REG_SOC) {
    this->publish_state(out_value);
  } else if (clamped_to_hard_limit) {
    // Reflect the clamped number immediately in HA controls.
    this->publish_state(out_value);
  }
  this->parent_->write_register(this->reg_id_, this->type_, out_value, this->scale_);
  if (clamped_to_hard_limit) {
    this->parent_->publish_settings_status_temporary("Clamped to hard limit", 5000);
  } else {
    this->parent_->publish_settings_status_temporary("Settings applied", 3000);
  }
}

void HMIBMSLogSwitch::write_state(bool state) {
  this->publish_state(state);
  if (this->parent_) {
    this->parent_->set_log_enabled(state);
  }
}

void HMIBMS::loop() {
  if (this->settings_unlocked_ &&
      (int32_t) (millis() - this->last_settings_write_ms_) >= (int32_t) SETTINGS_UNLOCK_TIMEOUT_MS) {
    this->set_settings_unlocked(false);
    this->publish_settings_status_temporary("Settings auto-locked (30s idle)", 10000);
  }

  if (this->settings_status_restore_pending_ && (int32_t)(millis() - this->settings_status_restore_at_) >= 0) {
    this->settings_status_restore_pending_ = false;
    if (this->settings_unlocked_) {
      this->last_settings_countdown_s_ = 0xFFFFFFFFUL;
    } else {
      this->publish_settings_status("Settings locked");
    }
  }

  if (this->settings_unlocked_ && !this->settings_status_restore_pending_) {
    const uint32_t elapsed_ms = millis() - this->last_settings_write_ms_;
    const uint32_t remaining_ms = (elapsed_ms >= SETTINGS_UNLOCK_TIMEOUT_MS) ? 0 : (SETTINGS_UNLOCK_TIMEOUT_MS - elapsed_ms);
    const uint32_t remaining_s = (remaining_ms + 999) / 1000;
    if (remaining_s != this->last_settings_countdown_s_) {
      this->last_settings_countdown_s_ = remaining_s;
      if (this->settings_status_text_sensor_ != nullptr) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Settings Unlocked (%lus)", (unsigned long) remaining_s);
        this->settings_status_text_sensor_->publish_state(buf);
      }
    }
  }

  // 1. Process all sensor updates from the queue
  while (!this->publish_queue_.empty()) {
    auto update = this->publish_queue_.front();
    this->publish_queue_.erase(this->publish_queue_.begin());
    if (update.sensor != nullptr) {
      update.sensor->publish_state(update.state);
    }
  }

  // 1b. Process one log message from the queue every 50ms to pace updates for Home Assistant
  if (this->log_enabled_ && this->log_text_sensor_ && !this->log_publish_queue_.empty()) {
    uint32_t now = millis();
    if (now - this->last_log_publish_time_ >= 50) {
      std::string line = this->log_publish_queue_.front();
      this->log_publish_queue_.pop_front();
      this->log_text_sensor_->publish_state(line);
      this->last_log_publish_time_ = now;
    }
  }

  // 1c. Publish buffered log line snapshot every 2 seconds, only changed lines
  if (this->log_enabled_ && this->log_line_snapshot_dirty_ && !this->log_line_sensors_.empty()) {
    uint32_t now = millis();
    if (now - this->last_log_line_publish_time_ >= this->log_update_interval_ms_) {
      for (size_t i = 0; i < this->log_line_sensors_.size() && i < this->log_line_snapshot_.size(); i++) {
        if (this->log_line_snapshot_[i] != this->log_line_sensors_[i]->state) {
          this->log_line_sensors_[i]->publish_state(this->log_line_snapshot_[i]);
        }
      }
      this->log_line_snapshot_dirty_ = false;
      this->last_log_line_publish_time_ = now;
    }
  }

  static std::vector<uint8_t> dump_buffer;
  uint32_t bytes_read = 0;
  uint8_t packets_handled = 0;
  
  // 2. Read UART data - high limit to keep up with 460800 baud log stream
  while (this->available() && bytes_read < 2048 && packets_handled < 10) {
    uint8_t byte;
    this->read_byte(&byte);
    bytes_read++;

    if (this->dump_raw_) {
      dump_buffer.push_back(byte);
      if (dump_buffer.size() >= 32) {
        ESP_LOGD(TAG, "Raw Rx: %s", format_hex_pretty(dump_buffer).c_str());
        dump_buffer.clear();
      }
    }

    if (this->rx_buffer_.empty()) {
      if (byte == 0xFF) {
        this->rx_buffer_.push_back(byte);
      }
    } else if (this->rx_buffer_.size() == 1) {
      // Length byte
      this->rx_buffer_.push_back(byte);
    } else if (this->rx_buffer_.size() == 2) {
      // Message Type byte - check if it's a known BMS message type
      // We'll be more permissive here to catch error responses (e.g. 0x80 + type)
      this->rx_buffer_.push_back(byte);
    } else {
      this->rx_buffer_.push_back(byte);
      uint8_t payload_len_minus_1 = this->rx_buffer_[1];
      size_t payload_len = (size_t)payload_len_minus_1 + 1;
      
      // Packet is: 0xFF, len-1, payload[payload_len], crc[2]
      if (this->rx_buffer_.size() == payload_len + 4) {
        // Complete packet
        uint16_t received_crc = (this->rx_buffer_[payload_len + 3] << 8) | this->rx_buffer_[payload_len + 2];
        // Calculate Modbus CRC on the entire frame [FF, Len-1, Payload...]
        uint16_t computed_crc = this->crc16_modbus_(this->rx_buffer_.data(), payload_len + 2);
        
        if (received_crc == computed_crc || this->bypass_crc_) {
          this->last_rx_ms_ = millis();
          if (received_crc != computed_crc) {
            ESP_LOGV(TAG, "CRC mismatch (bypassed): 0x%04X != 0x%04X (len=%d)", received_crc, computed_crc, payload_len);
          } else {
            ESP_LOGV(TAG, "CRC match: 0x%04X", computed_crc);
          }
          this->handle_packet_(this->rx_buffer_.data() + 2, payload_len);
          this->rx_buffer_.clear();
          packets_handled++;
        } else {
          // The BMS sends the CRC low byte first (duart_send_packet); anything else is corrupt
          ESP_LOGW(TAG, "CRC mismatch: Type=0x%02X LenField=0x%02X Calc=0x%04X Msg=0x%04X",
                   this->rx_buffer_[2], payload_len_minus_1, computed_crc, received_crc);
          ESP_LOGD(TAG, "Raw packet: %s", format_hex_pretty(this->rx_buffer_).c_str());
          // Resync: drop the first byte and look for the next 0xFF
          this->rx_buffer_.erase(this->rx_buffer_.begin());
          while (!this->rx_buffer_.empty() && this->rx_buffer_[0] != 0xFF) {
            this->rx_buffer_.erase(this->rx_buffer_.begin());
          }
        }
      }
    }
    
    if (this->rx_buffer_.size() > 512) {
      ESP_LOGV(TAG, "Buffer overflow, clearing...");
      this->rx_buffer_.clear();
    }
  }
}

void HMIBMS::handle_packet_(const uint8_t *payload, size_t length) {
  size_t offset = 0;
  while (offset < length) {
    uint8_t msg_type = payload[offset];
    size_t msg_len = 0;

    // Determine message length based on type
    switch (msg_type) {
      case HMI_MSG_READ_REGISTERS_RESPONSE:
        // This message type has variable length based on the registers included
        this->handle_read_registers_response_(payload + offset, length - offset);
        return; // Assume variable length takes rest of packet for now
      case HMI_MSG_READ_CELL_VOLTAGES_RESPONSE:
        ESP_LOGD(TAG, "Received Cell Voltages Response (msg_type=0x85)");
        this->handle_read_cell_voltages_response_(payload + offset, length - offset);
        return;
      case HMI_MSG_READ_EVENTS_RESPONSE:
        this->handle_read_events_response_(payload + offset, length - offset);
        return;
      case HMI_MSG_READ_BALANCING_TIMES_RESPONSE:
        this->handle_read_balancing_times_response_(payload + offset, length - offset);
        return;
      case HMI_MSG_READ_LOG_RESPONSE:
        this->handle_read_log_response_(payload + offset, length - offset);
        return;
      case HMI_MSG_ANNOUNCE_DEVICE:
        if (offset + 11 <= length) {
          uint8_t device_type = payload[offset + 1];
          uint8_t address = payload[offset + 2];
          // Serial number is the 8 bytes starting at payload[offset + 3]
          uint64_t serial = 0;
          for (int i = 0; i < 8; i++) {
            serial |= (uint64_t)payload[offset + 3 + i] << (i * 8);
          }
          ESP_LOGI(TAG, "Device announced: Type=0x%02X, Addr=0x%02X, Serial=%llu", device_type, address, serial);
          
          this->serial_ = serial;
          if (address == 0 || address == 1) {
            static uint32_t last_assign_time = 0;
            static bool address_claimed = false;
            uint32_t now = millis();
            
            // Only assign/claim if we haven't done it recently, or if it's still at 0
            if ((address == 0 && now - last_assign_time > 2000) || (address == 1 && !address_claimed)) {
              last_assign_time = now;
              address_claimed = (address == 1);
              
              ESP_LOGI(TAG, "Claiming/Assigning address 0x01 for device %llu", serial);
              std::vector<uint8_t> set_addr_payload;
              set_addr_payload.push_back(HMI_MSG_SET_DEVICE_ADDRESS);
              set_addr_payload.push_back(address); // Current address (0 or 1)
              set_addr_payload.push_back(0x01);    // Target address
              // Send serial in the same order it was received
              for (int i = 0; i < 8; i++) {
                set_addr_payload.push_back(payload[offset + 3 + i]);
              }
              this->send_packet_(set_addr_payload);
            }
          }
          
          if (address != 0) {
            if (this->address_ == 0) {
              ESP_LOGI(TAG, "BMS Address confirmed at 0x%02X. Triggering initial poll...", address);
              this->address_ = address;
              this->update(); // Poll immediately now that we have an address
            }
            this->address_ = address;
          }
          msg_len = 11;
        } else {
          msg_len = length - offset;
        }
        break;
      default:
        ESP_LOGD(TAG, "Unknown message type: 0x%02X at offset %zu of %zu", msg_type, offset, length);
        msg_len = length - offset;
        break;
    }
    offset += msg_len;
  }
}

void HMIBMS::handle_read_registers_response_(const uint8_t *data, size_t length) {
  if (length < 2) return;
  uint8_t address = data[1];
  ESP_LOGD(TAG, "Read registers response from 0x%02X, len=%zu", address, length);
  size_t offset = 2;
  
  float t_min = NAN;
  float t_max = NAN;
  float v_min = NAN;
  float v_max = NAN;
  auto decode_current_limit_a = [](uint16_t raw) -> float {
    return (float) raw * 0.1f;
  };

  while (offset + 3 <= length) {
    uint16_t reg_id = data[offset] | (data[offset + 1] << 8);
    uint8_t reg_type = data[offset + 2];
    offset += 3;
    size_t val_size = reg_type & 0x0F;
    if (val_size == 0 || offset + val_size > length) return;
    const uint8_t *v = data + offset;
    
    if (reg_id == HMI_REG_MILLIS && this->millis_sensor_ != nullptr) {
      uint64_t val = (uint64_t)v[0] | ((uint64_t)v[1] << 8) | ((uint64_t)v[2] << 16) | ((uint64_t)v[3] << 24) |
                     ((uint64_t)v[4] << 32) | ((uint64_t)v[5] << 40) | ((uint64_t)v[6] << 48) | ((uint64_t)v[7] << 56);
      this->publish_queue_.push_back({this->millis_sensor_, (float)val});
    } else if (reg_id == HMI_REG_SERIAL && this->serial_text_sensor_ != nullptr) {
      uint64_t val = (uint64_t)v[0] | ((uint64_t)v[1] << 8) | ((uint64_t)v[2] << 16) | ((uint64_t)v[3] << 24) |
                     ((uint64_t)v[4] << 32) | ((uint64_t)v[5] << 40) | ((uint64_t)v[6] << 48) | ((uint64_t)v[7] << 56);
      char buf[32];
      snprintf(buf, sizeof(buf), "%llu", val);
      this->serial_text_sensor_->publish_state(buf);
    } else if (reg_id == HMI_REG_BOARD_REVISION && this->board_revision_text_sensor_ != nullptr) {
      uint16_t code = v[0] | (v[1] << 8);
      char buf[24];
      if (code == 0xFFFF) {
        snprintf(buf, sizeof(buf), "Unknown");
      } else if (code < 10) {
        snprintf(buf, sizeof(buf), "Rev %u", code);
      } else {
        snprintf(buf, sizeof(buf), "Rev %u.%u", code / 10, code % 10);
      }
      this->board_revision_text_sensor_->publish_state(buf);
    } else if ((reg_id == HMI_REG_MCU_TEMPERATURE || reg_id == HMI_REG_SHUNT_DIE_TEMP || reg_id == HMI_REG_SHUNT_NTC_TEMP) &&
               reg_type == HMI_TYPE_FLOAT) {
      sensor::Sensor *s = reg_id == HMI_REG_MCU_TEMPERATURE  ? this->bms_chip_temperature_sensor_
                          : reg_id == HMI_REG_SHUNT_DIE_TEMP ? this->shunt_die_temperature_sensor_
                                                             : this->shunt_ntc_temperature_sensor_;
      if (s != nullptr) {
        uint32_t raw = (uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24);
        union {
          uint32_t u;
          float f;
        } conv{raw};
        this->publish_queue_.push_back({s, conv.f});
      }
    } else if ((reg_id == HMI_REG_BUILD_HASH || reg_id == HMI_REG_BUILD_TIME) && reg_type == HMI_TYPE_UINT32) {
      uint32_t val = (uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24);
      if (reg_id == HMI_REG_BUILD_HASH) {
        this->bms_build_hash_ = val;
        this->bms_build_hash_seen_ = true;
      } else {
        this->bms_build_time_ = val;
      }
      this->publish_bms_firmware_();
    } else if (reg_id == HMI_REG_BOARD_STRAP_COUNTS && this->board_strap_counts_sensor_ != nullptr &&
               reg_type == HMI_TYPE_FLOAT) {
      uint32_t raw = (uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24);
      union {
        uint32_t u;
        float f;
      } conv{raw};
      this->publish_queue_.push_back({this->board_strap_counts_sensor_, conv.f});
    } else if (reg_id == HMI_REG_SOC) {
      uint32_t val;
      if (reg_type == HMI_TYPE_UINT16 || reg_type == HMI_TYPE_INT16) {
        val = (uint16_t) (v[0] | (v[1] << 8));
      } else {
        val = (uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24);
      }
      if (this->soc_sensor_ != nullptr) {
        this->publish_queue_.push_back({this->soc_sensor_, (float)val / 100.0f});
      }
      if (this->soc_number_ != nullptr) {
        this->soc_number_->publish_from_bus((float)val * 0.01f);
      }
    } else if (reg_id == HMI_REG_CURRENT && this->current_mA_sensor_ != nullptr) {
      int32_t val = (int32_t)((uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24));
      this->publish_queue_.push_back({this->current_mA_sensor_, (float)val});
    } else if (reg_id == HMI_REG_CHARGE && this->charge_raw_sensor_ != nullptr) {
      int64_t val;
      if (reg_type == HMI_TYPE_INT64) {
        val = (int64_t)v[0] | ((int64_t)v[1] << 8) | ((int64_t)v[2] << 16) | ((int64_t)v[3] << 24) |
              ((int64_t)v[4] << 32) | ((int64_t)v[5] << 40) | ((int64_t)v[6] << 48) | ((int64_t)v[7] << 56);
      } else {
        val = (int32_t)((uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24));
      }
      this->publish_queue_.push_back({this->charge_raw_sensor_, (float)(val / 3600000.0f)}); // Convert mC to Ah
    } else if (reg_id == HMI_REG_BATTERY_VOLTAGE && this->battery_voltage_mV_sensor_ != nullptr) {
      int32_t val = (int32_t)((uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24));
      this->publish_queue_.push_back({this->battery_voltage_mV_sensor_, (float)val});
    } else if (reg_id == HMI_REG_OUTPUT_VOLTAGE && this->output_voltage_mV_sensor_ != nullptr) {
      int32_t val = (int32_t)((uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24));
      this->publish_queue_.push_back({this->output_voltage_mV_sensor_, (float)val});
    } else if (reg_id == HMI_REG_POS_CONTACTOR_VOLTAGE && this->pos_contactor_voltage_mV_sensor_ != nullptr) {
      int32_t val = (int32_t)((uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24));
      this->publish_queue_.push_back({this->pos_contactor_voltage_mV_sensor_, (float)val});
    } else if (reg_id == HMI_REG_NEG_CONTACTOR_VOLTAGE && this->neg_contactor_voltage_mV_sensor_ != nullptr) {
      int32_t val = (int32_t)((uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24));
      this->publish_queue_.push_back({this->neg_contactor_voltage_mV_sensor_, (float)val});
    } else if (reg_id == HMI_REG_TEMPERATURE_MIN) {
      uint32_t raw = (uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24);
      union {
        uint32_t u;
        float f;
      } conv{raw};
      t_min = conv.f;
      if (this->temperature_min_dC_sensor_) this->publish_queue_.push_back({this->temperature_min_dC_sensor_, t_min});
    } else if (reg_id == HMI_REG_TEMPERATURE_MAX) {
      uint32_t raw = (uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24);
      union {
        uint32_t u;
        float f;
      } conv{raw};
      t_max = conv.f;
      if (this->temperature_max_dC_sensor_) this->publish_queue_.push_back({this->temperature_max_dC_sensor_, t_max});
    } else if (reg_id == HMI_REG_CELL_VOLTAGE_MIN) {
      int16_t val = (int16_t)(v[0] | (v[1] << 8));
      v_min = (float)val;
      if (this->cell_voltage_min_mV_sensor_) this->publish_queue_.push_back({this->cell_voltage_min_mV_sensor_, v_min});
    } else if (reg_id == HMI_REG_CELL_VOLTAGE_MAX) {
      int16_t val = (int16_t)(v[0] | (v[1] << 8));
      v_max = (float)val;
      if (this->cell_voltage_max_mV_sensor_) this->publish_queue_.push_back({this->cell_voltage_max_mV_sensor_, v_max});
    } else if (reg_id == HMI_REG_SYSTEM_REQUEST && this->system_request_sensor_ != nullptr) {
      this->publish_queue_.push_back({this->system_request_sensor_, (float)v[0]});
    } else if (reg_id == HMI_REG_SYSTEM_STATE && this->system_sm_sensor_ != nullptr) {
      this->publish_queue_.push_back({this->system_sm_sensor_, (float)v[0]});
    } else if (reg_id == HMI_REG_CONTACTORS_STATE && this->contactor_sm_sensor_ != nullptr) {
      this->publish_queue_.push_back({this->contactor_sm_sensor_, (float)v[0]});
    } else if (reg_id == HMI_REG_SOC_VOLTAGE_BASED && this->soc_voltage_based_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->soc_voltage_based_sensor_, (float)val / 100.0f});
    } else if (reg_id == HMI_REG_SOC_BASIC_COUNT && this->soc_basic_count_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->soc_basic_count_sensor_, (float)val / 100.0f});
    } else if (reg_id == HMI_REG_CAPACITY && this->capacity_mC_sensor_ != nullptr) {
      uint32_t val = (uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24);
      this->publish_queue_.push_back({this->capacity_mC_sensor_, (float)val / 3600000.0f}); // Convert mC to Ah
    } else if (reg_id == HMI_REG_SUPPLY_VOLTAGE_3V3 && this->supply_voltage_3V3_mV_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->supply_voltage_3V3_mV_sensor_, (float)val});
    } else if (reg_id == HMI_REG_SUPPLY_VOLTAGE_5V && this->supply_voltage_5V_mV_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->supply_voltage_5V_mV_sensor_, (float)val});
    } else if (reg_id == HMI_REG_SUPPLY_VOLTAGE_12V && this->supply_voltage_12V_mV_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->supply_voltage_12V_mV_sensor_, (float)val});
    } else if (reg_id == HMI_REG_SUPPLY_VOLTAGE_CTR && this->supply_voltage_contactor_mV_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->supply_voltage_contactor_mV_sensor_, (float)val});
    } else if ((reg_id == HMI_REG_ISOLATION_RESISTANCE || reg_id == HMI_REG_PACK_POS_TO_CHASSIS ||
                reg_id == HMI_REG_PACK_NEG_TO_CHASSIS || reg_id == HMI_REG_ISOLATION_CHECK_AGE) &&
               reg_type == HMI_TYPE_FLOAT) {
      uint32_t raw = (uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24);
      union {
        uint32_t u;
        float f;
      } conv{raw};
      sensor::Sensor *s = reg_id == HMI_REG_PACK_POS_TO_CHASSIS   ? this->pack_pos_to_chassis_sensor_
                          : reg_id == HMI_REG_PACK_NEG_TO_CHASSIS ? this->pack_neg_to_chassis_sensor_
                          : reg_id == HMI_REG_ISOLATION_CHECK_AGE ? this->isolation_check_age_sensor_
                                                                  : this->isolation_resistance_sensor_;
      if (s != nullptr) this->publish_queue_.push_back({s, conv.f});
    } else if (reg_id == HMI_REG_INVERTER_SOC && this->inverter_soc_sensor_ != nullptr) {
      int16_t val = (int16_t)(v[0] | (v[1] << 8));
      this->publish_queue_.push_back({this->inverter_soc_sensor_, (float)val * 0.01f});
    } else if (reg_id == HMI_REG_INVERTER_FULL_CAPACITY_DAH && this->inverter_full_capacity_sensor_ != nullptr) {
      int16_t val = (int16_t)(v[0] | (v[1] << 8));
      this->publish_queue_.push_back({this->inverter_full_capacity_sensor_, (float)val * 0.1f});
    } else if (reg_id == HMI_REG_INVERTER_REMAINING_CAPACITY_DAH && this->inverter_remaining_capacity_sensor_ != nullptr) {
      int16_t val = (int16_t)(v[0] | (v[1] << 8));
      this->publish_queue_.push_back({this->inverter_remaining_capacity_sensor_, (float)val * 0.1f});
    } else if (reg_id == HMI_REG_INVERTER_MIN_VOLTAGE_LIMIT_DV && this->inverter_min_voltage_limit_sensor_ != nullptr) {
      int16_t val = (int16_t)(v[0] | (v[1] << 8));
      this->publish_queue_.push_back({this->inverter_min_voltage_limit_sensor_, (float)val * 0.1f});
    } else if (reg_id == HMI_REG_INVERTER_MAX_VOLTAGE_LIMIT_DV && this->inverter_max_voltage_limit_sensor_ != nullptr) {
      int16_t val = (int16_t)(v[0] | (v[1] << 8));
      this->publish_queue_.push_back({this->inverter_max_voltage_limit_sensor_, (float)val * 0.1f});
    } else if (reg_id == HMI_REG_CELL_VOLTAGE_WORKING_MIN && this->cell_voltage_working_min_number_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->cell_voltage_working_min_number_->publish_from_bus((float)val);
    } else if (reg_id == HMI_REG_CELL_VOLTAGE_WORKING_MAX && this->cell_voltage_working_max_number_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->cell_voltage_working_max_number_->publish_from_bus((float)val);
    } else if (reg_id == HMI_REG_SOC_SCALING_MIN && this->soc_scaling_min_number_ != nullptr) {
      int16_t val = (int16_t)(v[0] | (v[1] << 8));
      this->soc_scaling_min_number_->publish_from_bus((float)val * 0.01f);
    } else if (reg_id == HMI_REG_SOC_SCALING_MAX && this->soc_scaling_max_number_ != nullptr) {
      int16_t val = (int16_t)(v[0] | (v[1] << 8));
      this->soc_scaling_max_number_->publish_from_bus((float)val * 0.01f);
    } else if (reg_id == HMI_REG_VOLTAGE_LIMIT_OFFSET_LOWER && this->voltage_limit_offset_lower_number_ != nullptr) {
      int16_t val = (int16_t)(v[0] | (v[1] << 8));
      this->voltage_limit_offset_lower_number_->publish_from_bus((float)val);
    } else if (reg_id == HMI_REG_VOLTAGE_LIMIT_OFFSET_UPPER && this->voltage_limit_offset_upper_number_ != nullptr) {
      int16_t val = (int16_t)(v[0] | (v[1] << 8));
      this->voltage_limit_offset_upper_number_->publish_from_bus((float)val);
    } else if (reg_id == HMI_REG_CELL_VOLTAGE_SOFT_MIN && this->cell_voltage_soft_min_number_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->cell_voltage_soft_min_number_->publish_from_bus((float)val);
    } else if (reg_id == HMI_REG_CELL_VOLTAGE_SOFT_MAX && this->cell_voltage_soft_max_number_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->cell_voltage_soft_max_number_->publish_from_bus((float)val);
    } else if (reg_id == HMI_REG_AUTO_BALANCING_PERIOD_MS && this->auto_balancing_period_ms_number_ != nullptr) {
      uint32_t val = (uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24);
      this->auto_balancing_period_ms_number_->publish_from_bus((float)val);
    } else if (reg_id == HMI_REG_BALANCING_PERIODS_PER_MV && this->balancing_periods_per_mV_number_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->balancing_periods_per_mV_number_->publish_from_bus((float)val);
    } else if (reg_id == HMI_REG_BALANCE_MIN_OFFSET_MV && this->balance_min_offset_mV_number_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->balance_min_offset_mV_number_->publish_from_bus((float)val);
    } else if (reg_id == HMI_REG_MINIMUM_BALANCING_VOLTAGE_MV && this->minimum_balancing_voltage_mV_number_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->minimum_balancing_voltage_mV_number_->publish_from_bus((float)val);
    } else if (reg_id == HMI_REG_BALANCING_START_VOLTAGE_MV && this->balancing_start_voltage_mV_number_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->balancing_start_voltage_mV_number_->publish_from_bus((float)val);
    } else if (reg_id == HMI_REG_USER_CHARGE_CURRENT_LIMIT_DA && this->max_charge_current_number_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      float out = decode_current_limit_a(val);
      // Keep all readbacks numeric. For user "no limit" sentinel, prefer hard limit if known.
      if (val == 0xFFFF && this->has_last_hard_charge_current_limit_) {
        out = this->last_hard_charge_current_limit_a_;
      }
      this->max_charge_current_number_->publish_from_bus(out);
    } else if (reg_id == HMI_REG_USER_DISCHARGE_CURRENT_LIMIT_DA && this->max_discharge_current_number_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      float out = decode_current_limit_a(val);
      if (val == 0xFFFF && this->has_last_hard_discharge_current_limit_) {
        out = this->last_hard_discharge_current_limit_a_;
      }
      this->max_discharge_current_number_->publish_from_bus(out);
    } else if (reg_id == HMI_REG_CHARGE_CURRENT_LIMIT_DA && this->charge_current_limit_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->charge_current_limit_sensor_, decode_current_limit_a(val)});
    } else if (reg_id == HMI_REG_DISCHARGE_CURRENT_LIMIT_DA && this->discharge_current_limit_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->discharge_current_limit_sensor_, decode_current_limit_a(val)});
    } else if (reg_id == HMI_REG_TEMP_CHARGE_CURRENT_LIMIT_DA && this->temp_charge_current_limit_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->temp_charge_current_limit_sensor_, decode_current_limit_a(val)});
    } else if (reg_id == HMI_REG_TEMP_DISCHARGE_CURRENT_LIMIT_DA && this->temp_discharge_current_limit_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->temp_discharge_current_limit_sensor_, decode_current_limit_a(val)});
    } else if (reg_id == HMI_REG_CELL_VOLTAGE_CHARGE_CURRENT_LIMIT_DA && this->cell_voltage_charge_current_limit_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->cell_voltage_charge_current_limit_sensor_, decode_current_limit_a(val)});
    } else if (reg_id == HMI_REG_CELL_VOLTAGE_DISCHARGE_CURRENT_LIMIT_DA && this->cell_voltage_discharge_current_limit_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->cell_voltage_discharge_current_limit_sensor_, decode_current_limit_a(val)});
    } else if (reg_id == HMI_REG_WORKING_CHARGE_CURRENT_LIMIT_DA && this->working_charge_current_limit_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->working_charge_current_limit_sensor_, decode_current_limit_a(val)});
    } else if (reg_id == HMI_REG_WORKING_DISCHARGE_CURRENT_LIMIT_DA && this->working_discharge_current_limit_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->working_discharge_current_limit_sensor_, decode_current_limit_a(val)});
    } else if (reg_id == HMI_REG_DELTA_CHARGE_CURRENT_LIMIT_DA && this->delta_charge_current_limit_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      this->publish_queue_.push_back({this->delta_charge_current_limit_sensor_, decode_current_limit_a(val)});
    } else if (reg_id == HMI_REG_HARD_CHARGE_CURRENT_LIMIT_DA && this->hard_charge_current_limit_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      float out = decode_current_limit_a(val);
      this->last_hard_charge_current_limit_a_ = out;
      this->has_last_hard_charge_current_limit_ = true;
      if (this->max_charge_current_number_ != nullptr) {
        this->max_charge_current_number_->traits.set_max_value(out);
      }
      this->publish_queue_.push_back({this->hard_charge_current_limit_sensor_, out});
    } else if (reg_id == HMI_REG_HARD_DISCHARGE_CURRENT_LIMIT_DA && this->hard_discharge_current_limit_sensor_ != nullptr) {
      uint16_t val = v[0] | (v[1] << 8);
      float out = decode_current_limit_a(val);
      this->last_hard_discharge_current_limit_a_ = out;
      this->has_last_hard_discharge_current_limit_ = true;
      if (this->max_discharge_current_number_ != nullptr) {
        this->max_discharge_current_number_->traits.set_max_value(out);
      }
      this->publish_queue_.push_back({this->hard_discharge_current_limit_sensor_, out});
    } else if (reg_id == HMI_REG_HARD_CHARGE_CURRENT_LIMIT_DA) {
      uint16_t val = v[0] | (v[1] << 8);
      float out = decode_current_limit_a(val);
      this->last_hard_charge_current_limit_a_ = out;
      this->has_last_hard_charge_current_limit_ = true;
      if (this->max_charge_current_number_ != nullptr) {
        this->max_charge_current_number_->traits.set_max_value(out);
      }
    } else if (reg_id == HMI_REG_HARD_DISCHARGE_CURRENT_LIMIT_DA) {
      uint16_t val = v[0] | (v[1] << 8);
      float out = decode_current_limit_a(val);
      this->last_hard_discharge_current_limit_a_ = out;
      this->has_last_hard_discharge_current_limit_ = true;
      if (this->max_discharge_current_number_ != nullptr) {
        this->max_discharge_current_number_->traits.set_max_value(out);
      }
    } else if (reg_id >= HMI_REG_MODULE_TEMPS_START && reg_id <= HMI_REG_MODULE_TEMPS_END) {
      uint16_t idx = reg_id - HMI_REG_MODULE_TEMPS_START;
      if (idx < this->module_temperature_sensors_.size() && this->module_temperature_sensors_[idx] != nullptr) {
        uint32_t raw = (uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24);
        union {
          uint32_t u;
          float f;
        } conv{raw};
        this->publish_queue_.push_back({this->module_temperature_sensors_[idx], conv.f});
      }
    } else if (reg_id >= HMI_REG_RAW_TEMPS_START && reg_id <= HMI_REG_RAW_TEMPS_END) {
      uint16_t idx = reg_id - HMI_REG_RAW_TEMPS_START;
      if (idx < this->raw_temperature_sensors_.size() && this->raw_temperature_sensors_[idx] != nullptr) {
        int16_t val = (int16_t)(v[0] | (v[1] << 8));
        this->publish_queue_.push_back({this->raw_temperature_sensors_[idx], (float)val / 10.0f});
      }
    }
    offset += val_size;
  }

  // Deltas from the last raw values off the bus (degC, mV), independent of any YAML filters
  if (!std::isnan(t_min)) this->last_temperature_min_c_ = t_min;
  if (!std::isnan(t_max)) this->last_temperature_max_c_ = t_max;
  if (!std::isnan(v_min)) this->last_cell_voltage_min_mv_ = v_min;
  if (!std::isnan(v_max)) this->last_cell_voltage_max_mv_ = v_max;
  const bool temps_new = !std::isnan(t_min) || !std::isnan(t_max);
  const bool cells_new = !std::isnan(v_min) || !std::isnan(v_max);
  if (this->temperature_delta_sensor_ && temps_new && !std::isnan(this->last_temperature_min_c_) &&
      !std::isnan(this->last_temperature_max_c_)) {
    this->publish_queue_.push_back(
        {this->temperature_delta_sensor_, this->last_temperature_max_c_ - this->last_temperature_min_c_});
  }
  if (this->cell_voltage_delta_sensor_ && cells_new && !std::isnan(this->last_cell_voltage_min_mv_) &&
      !std::isnan(this->last_cell_voltage_max_mv_)) {
    this->publish_queue_.push_back(
        {this->cell_voltage_delta_sensor_, this->last_cell_voltage_max_mv_ - this->last_cell_voltage_min_mv_});
  }
}

void HMIBMS::handle_read_cell_voltages_response_(const uint8_t *data, size_t length) {
  if (length < 3) {
    ESP_LOGW(TAG, "Cell voltages response too short: %zu", length);
    return;
  }
  uint8_t address = data[1];
  uint8_t num_cells = data[2];
  
  ESP_LOGD(TAG, "Parsing cell voltages: Addr=0x%02X NumCells=%u Len=%zu", address, num_cells, length);
  if (length > 3) {
     // Log first few bytes of payload data for debugging
     char hex_buf[64];
     size_t dump_len = std::min(length - 3, (size_t)16);
     for(size_t i=0; i<dump_len; i++) snprintf(hex_buf + i*3, 4, "%02X ", data[3+i]);
     ESP_LOGD(TAG, "CV Payload start: %s", hex_buf);
  }

  size_t offset = 3;
  int16_t last_cell_voltage = 0;
  uint16_t cells_found = 0;
  uint16_t nonzero_cells = 0;
  
  for (int i = 0; i < num_cells && offset < length; i++) {
    int16_t cell_voltage;
    if (data[offset] & 0x80) {
      // Absolute voltage (2 bytes, big-endian)
      if (offset + 1 >= length) break;
      uint16_t val = (((uint16_t)data[offset] & 0x7F) << 8) | data[offset + 1];
      // Sign extend 15-bit to 16-bit
      if (val & 0x4000) {
        cell_voltage = (int16_t)(val | 0x8000);
      } else {
        cell_voltage = (int16_t)val;
      }
      offset += 2;
    } else {
      // Delta voltage (1 byte, signed 7-bit)
      uint8_t val = data[offset] & 0x7F;
      int8_t delta;
      if (val & 0x40) {
        delta = (int8_t)(val | 0x80);
      } else {
        delta = (int8_t)val;
      }
      cell_voltage = last_cell_voltage + delta;
      offset += 1;
    }
    
    if (i < this->cell_voltage_sensors_.size() && this->cell_voltage_sensors_[i] != nullptr) {
      if (cell_voltage == -1) {
        this->publish_queue_.push_back({this->cell_voltage_sensors_[i], NAN});
      } else {
        this->publish_queue_.push_back({this->cell_voltage_sensors_[i], cell_voltage / 1000.0f});
        cells_found++;
      }
    }
    if (cell_voltage > 0) {
      nonzero_cells++;
    }
    last_cell_voltage = cell_voltage;
  }

  // Publish only the number of parsed cells with non-zero voltage.
  // No fallbacks to header or configured count.
  if (this->cell_count_sensor_ != nullptr) {
    this->publish_queue_.push_back({this->cell_count_sensor_, (float)nonzero_cells});
    ESP_LOGD(TAG, "Cell count nonzero=%u (header=%u valid=%u)",
             nonzero_cells, num_cells, cells_found);
  }
  ESP_LOGD(TAG, "Read %u cell voltages from 0x%02X (queued)", cells_found, address);
}

void HMIBMS::handle_read_balancing_times_response_(const uint8_t *data, size_t length) {
  if (length < 3) return;
  uint8_t address = data[1];
  uint8_t num_cells = data[2];
  
  size_t offset = 3;
  int32_t last_time = 0;
  std::string cells_balancing = "";
  std::string cells_balancing_active = "";
  const uint32_t now_ms = millis();
  const uint32_t active_window_ms = 5000;
  if (this->balancing_time_prev_.size() < (size_t) num_cells) {
    this->balancing_time_prev_.resize(num_cells, 0);
  }
  if (this->balancing_active_until_ms_.size() < (size_t) num_cells) {
    this->balancing_active_until_ms_.resize(num_cells, 0);
  }

  int balancing_run_start = -1;
  int balancing_run_end = -1;
  auto flush_balancing_run = [&]() {
    if (balancing_run_start < 0) return;
    if (!cells_balancing.empty()) {
      cells_balancing += ",";
    }
    if (balancing_run_start == balancing_run_end) {
      cells_balancing += std::to_string(balancing_run_start + 1);
    } else {
      cells_balancing += std::to_string(balancing_run_start + 1);
      cells_balancing += "-";
      cells_balancing += std::to_string(balancing_run_end + 1);
    }
    balancing_run_start = -1;
    balancing_run_end = -1;
  };

  int active_run_start = -1;
  int active_run_end = -1;
  auto flush_active_run = [&]() {
    if (active_run_start < 0) return;
    if (!cells_balancing_active.empty()) {
      cells_balancing_active += ",";
    }
    if (active_run_start == active_run_end) {
      cells_balancing_active += std::to_string(active_run_start + 1);
    } else {
      cells_balancing_active += std::to_string(active_run_start + 1);
      cells_balancing_active += "-";
      cells_balancing_active += std::to_string(active_run_end + 1);
    }
    active_run_start = -1;
    active_run_end = -1;
  };
  
  for (int i = 0; i < num_cells && offset < length; i++) {
    int32_t time_val;
    if (data[offset] & 0x80) {
      // Absolute value (2 bytes, big-endian), encoded as 15-bit unsigned.
      if (offset + 1 >= length) break;
      uint16_t val = (((uint16_t)data[offset] & 0x7F) << 8) | data[offset + 1];
      time_val = (int32_t) val;
      offset += 2;
    } else {
      // Delta value (1 byte, signed 7-bit)
      uint8_t val = data[offset] & 0x7F;
      int8_t delta;
      if (val & 0x40) {
        delta = (int8_t)(val | 0x80);
      } else {
        delta = (int8_t)val;
      }
      time_val = last_time + delta;
      offset += 1;
    }

    // Balancing-time counters are non-negative.
    if (time_val < 0) {
      time_val = 0;
    }
    
    last_time = time_val;

    if (i < this->balancing_time_sensors_.size() && this->balancing_time_sensors_[i] != nullptr) {
      this->publish_queue_.push_back({this->balancing_time_sensors_[i], (float) time_val});
    }

    int32_t prev_time_val = 0;
    if (i < (int) this->balancing_time_prev_.size()) {
      prev_time_val = this->balancing_time_prev_[i];
      this->balancing_time_prev_[i] = time_val;
    }

    if (time_val <= 0) {
      if (i < (int) this->balancing_active_until_ms_.size()) {
        this->balancing_active_until_ms_[i] = 0;
      }
    } else if (prev_time_val > time_val) {
      if (i < (int) this->balancing_active_until_ms_.size()) {
        this->balancing_active_until_ms_[i] = now_ms + active_window_ms;
      }
    }
    const bool is_active_now = i < (int) this->balancing_active_until_ms_.size() &&
                               this->balancing_active_until_ms_[i] > now_ms;

    if (time_val > 0) {
      if (balancing_run_start < 0) {
        balancing_run_start = i;
        balancing_run_end = i;
      } else if (i == balancing_run_end + 1) {
        balancing_run_end = i;
      } else {
        flush_balancing_run();
        balancing_run_start = i;
        balancing_run_end = i;
      }
    } else if (balancing_run_start >= 0) {
      flush_balancing_run();
    }

    if (is_active_now) {
      if (active_run_start < 0) {
        active_run_start = i;
        active_run_end = i;
      } else if (i == active_run_end + 1) {
        active_run_end = i;
      } else {
        flush_active_run();
        active_run_start = i;
        active_run_end = i;
      }
    } else if (active_run_start >= 0) {
      flush_active_run();
    }
  }
  flush_balancing_run();
  flush_active_run();

  if (cells_balancing.empty()) {
    cells_balancing = "None";
  }
  if (cells_balancing_active.empty()) {
    cells_balancing_active = "None";
  }

  ESP_LOGD(TAG, "Balancing cells (time > 0): %s", cells_balancing.c_str());
  ESP_LOGD(TAG, "Balancing active cells (decrementing): %s", cells_balancing_active.c_str());

  if (this->cells_balancing_text_sensor_) {
    this->cells_balancing_text_sensor_->publish_state(cells_balancing);
  }
  if (this->cells_balancing_active_text_sensor_) {
    this->cells_balancing_active_text_sensor_->publish_state(cells_balancing_active);
  }
}

void HMIBMS::handle_read_log_response_(const uint8_t *data, size_t length) {
  if (length < 2) {
    ESP_LOGW(TAG, "Log response too short: %zu", length);
    this->log_busy_ = false;
    return;
  }
  
  // data[0] is Msg Type, data[1] is Address
  size_t log_len = length - 2;
  ESP_LOGD(TAG, "Received log chunk len=%zu", log_len);
  
  if (log_len == 0) {
    // Empty response means BMS has no more data
    this->log_busy_ = false;
    return;
  }
  
  // Append received chunk to our buffer
  this->log_buffer_.append((const char*)(data + 2), log_len);
  ESP_LOGV(TAG, "Log buffer size: %zu", this->log_buffer_.size());
  
  // Process complete lines (terminated by \n)
  size_t pos;
  while ((pos = this->log_buffer_.find('\n')) != std::string::npos) {
    std::string line = this->log_buffer_.substr(0, pos);
    
    // Trim trailing \r
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    // Strip ANSI escape sequences (ESC[...letter) sent by BMS terminal output
    {
      std::string clean;
      clean.reserve(line.size());
      for (size_t i = 0; i < line.size(); i++) {
        if (line[i] == '\x1b' && i + 1 < line.size() && line[i + 1] == '[') {
          i += 2;
          while (i < line.size() && !isalpha((unsigned char)line[i])) i++;
        } else if ((unsigned char)line[i] >= 0x20) {
          clean += line[i];
        }
      }
      line = clean;
    }
    
    if (!line.empty()) {
      ESP_LOGI(TAG, "[BMS LOG] %s", line.c_str());
      if (this->log_text_sensor_) {
        this->log_publish_queue_.push_back(line);
        if (this->log_publish_queue_.size() > 100) {
           this->log_publish_queue_.pop_front();
        }
      }
      // Sync log line sensors: when we see a line starting with the sync prefix, reset to line 0
      if (!this->log_sync_prefix_.empty() &&
          line.size() >= this->log_sync_prefix_.size() &&
          line.compare(0, this->log_sync_prefix_.size(), this->log_sync_prefix_) == 0) {
        // New cycle detected: snapshot whatever we have (short 15-cell or long 96-cell logs both valid)
        if (!this->log_line_pending_.empty() && this->log_line_index_ > 0) {
          this->log_line_snapshot_ = this->log_line_pending_;
          this->log_line_snapshot_dirty_ = true;
        }
        this->log_line_index_ = 0;
        this->log_line_pending_.clear();
        this->log_line_pending_.resize(this->log_line_sensors_.size());
      }
      // Buffer the line for next snapshot
      if (this->log_line_index_ < this->log_line_pending_.size()) {
        this->log_line_pending_[this->log_line_index_] = line;
      }
      this->log_line_index_++;
    }
    
    // Remove processed line from buffer (including the \n at pos)
    this->log_buffer_.erase(0, pos + 1);
  }

  // Safety cap on buffer size to prevent memory leak if no newline ever comes
  if (this->log_buffer_.size() > 1024) {
    ESP_LOGW(TAG, "Log buffer overflow, clearing...");
    this->log_buffer_.clear();
  }

  // Flow control: a non-full response (<240 bytes) means the BMS log buffer is drained.
  // A full response means more data is likely waiting, so send 2 more requests.
  // This works whether the BMS sends 1 response per request or multiple.
  if (log_len >= 240) {
    ESP_LOGV(TAG, "Log response full (%zu bytes), sending 2 more requests", log_len);
    std::vector<uint8_t> log_payload;
    log_payload.push_back(HMI_MSG_READ_LOG);
    log_payload.push_back(this->address_);
    this->send_packet_(log_payload);
    this->send_packet_(log_payload);
    this->last_log_request_time_ = millis();
  } else {
    // Non-full response: BMS is done sending, allow next tick to start a new cycle
    this->log_busy_ = false;
  }
}

void HMIBMS::request_events_(uint16_t start_index) {
  std::vector<uint8_t> payload;
  payload.push_back(HMI_MSG_READ_EVENTS);
  payload.push_back(this->address_);
  payload.push_back(start_index & 0xFF);
  payload.push_back(start_index >> 8);
  this->send_packet_(payload);
}

// The BMS lists every event type that has happened (count > 0), active or not, starting
// at the requested index. A full reply (11 events) means there may be more from next_index.
void HMIBMS::handle_read_events_response_(const uint8_t *data, size_t length) {
  static const size_t EVENT_RECORD_SIZE = 22;
  static const uint16_t EVENTS_PER_FULL_REPLY = 11;  // (250 - 6) / 22 in hmi_handle_read_events
  static const uint8_t MAX_EVENT_PAGES = 8;
  if (length < 6) return;
  auto &sw = this->event_sweep_;
  if (!sw.active) return;  // a late page from a sweep that was already finished
  uint8_t address = data[1];
  uint16_t next_index = data[2] | (data[3] << 8);
  uint16_t count = data[4] | (data[5] << 8);
  ESP_LOGD(TAG, "Read events response from 0x%02X, page %u, count=%u, next=%u", address, sw.pages, count, next_index);

  size_t offset = 6;
  uint16_t parsed = 0;
  for (uint16_t i = 0; i < count && offset + EVENT_RECORD_SIZE <= length; i++, offset += EVENT_RECORD_SIZE) {
    uint16_t type = data[offset] | (data[offset + 1] << 8);
    uint16_t level = data[offset + 2] | (data[offset + 3] << 8);
    uint16_t event_count = data[offset + 4] | (data[offset + 5] << 8);
    parsed++;

    // Override: Inverter Detected (Type 45) is always Info level
    if (type == 45) {
      level = 1;  // INFO
    }

    const char *type_str = event_type_name(type);
    const char *level_str = (level < sizeof(EVENT_LEVELS) / sizeof(EVENT_LEVELS[0])) ? EVENT_LEVELS[level] : "??";

    char state_buf[32];
    snprintf(state_buf, sizeof(state_buf), "%s (%u)", level_str, event_count);
    if (type < this->error_sensors_.size() && this->error_sensors_[type] != nullptr) {
      auto *error_sensor = this->error_sensors_[type];
      if (!error_sensor->has_state() || error_sensor->state != state_buf) {
        error_sensor->publish_state(state_buf);
      }
      if (type < sw.seen.size()) sw.seen[type] = true;
    }

    // The first event of the sweep is the latest
    if (sw.latest_type < 0) {
      sw.latest_type = type;
      sw.latest_level = level;
      sw.latest_count = event_count;
    }
    if (level > sw.highest_level) sw.highest_level = level;
    if (level < 5) {
      char event_str[64];
      snprintf(event_str, sizeof(event_str), "%s (%u)", type_str, event_count);
      sw.by_level[level].push_back(event_str);
    }
  }
  sw.pages++;

  if (parsed >= EVENTS_PER_FULL_REPLY && sw.pages < MAX_EVENT_PAGES) {
    this->request_events_(next_index);
    return;
  }
  this->finish_event_sweep_();
}

void HMIBMS::finish_event_sweep_() {
  auto &sw = this->event_sweep_;
  sw.active = false;

  // Types the BMS didn't list on any page have never happened
  for (size_t i = 0; i < this->error_sensors_.size(); i++) {
    if (this->error_sensors_[i] == nullptr || (i < sw.seen.size() && sw.seen[i])) continue;
    auto *error_sensor = this->error_sensors_[i];
    if (!error_sensor->has_state() || error_sensor->state != "INACTIVE") {
      error_sensor->publish_state("INACTIVE");
    }
  }

  // "Historic: ...\nInfo: ...\nWarning: none\n..." (level 0 = happened before, not active now)
  static const char *CAT_NAMES[] = {"Historic", "Info", "Warning", "Critical", "Fatal"};
  std::string history;
  for (int lvl = 0; lvl < 5; lvl++) {
    history += CAT_NAMES[lvl];
    history += ": ";
    if (sw.by_level[lvl].empty()) {
      history += "none";
    } else {
      for (size_t k = 0; k < sw.by_level[lvl].size(); k++) {
        if (k > 0) history += ", ";
        history += sw.by_level[lvl][k];
      }
    }
    history += "\n";
  }

  if (this->highest_event_level_sensor_) {
    this->publish_queue_.push_back({this->highest_event_level_sensor_, (float) sw.highest_level});
  }
  if (this->last_event_sensor_ && sw.latest_type >= 0) {
    this->publish_queue_.push_back({this->last_event_sensor_, (float) sw.latest_type});
  }
  this->last_event_code_ = sw.latest_type;
  this->last_event_level_ = sw.latest_level;
  this->last_event_count_ = sw.latest_type < 0 ? 0 : sw.latest_count;
  this->last_event_type_str_ = sw.latest_type < 0 ? "UNKNOWN" : event_type_name(sw.latest_type);

  if (this->event_history_text_sensor_) {
    if (!this->event_history_text_sensor_->has_state() || this->event_history_text_sensor_->state != history) {
      this->event_history_text_sensor_->publish_state(history);
    }
  }
}

uint16_t HMIBMS::crc16_modbus_(const uint8_t *data, size_t len) {
  // Modbus CRC16 (poly 0x8005 -> reflected 0xA001), init 0xFFFF
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

void HMIBMS::dump_config() {
  ESP_LOGCONFIG(TAG, "HMI BMS:");
  this->check_uart_settings(this->baud_rate_);
}

}  // namespace hmi_bms
}  // namespace esphome
