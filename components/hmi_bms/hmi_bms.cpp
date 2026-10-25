#include "hmi_bms.h"
#include "hmi_protocol.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"
#include <algorithm>
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
  this->add_internal_registers_();
  std::stable_sort(this->registers_.begin(), this->registers_.end(),
                   [](const RegisterEntry &a, const RegisterEntry &b) { return a.reg < b.reg; });
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

// Registers read for the text sensors and limits that aren't table entries themselves
void HMIBMS::add_internal_registers_() {
  auto add = [this](uint16_t reg, uint8_t type, uint8_t group) {
    this->registers_.push_back({reg, type, group, 1.0f, nullptr, nullptr});
  };
  if (this->serial_text_sensor_) add(HMI_REG_SERIAL, HMI_TYPE_UINT64, POLL_SLOW);
  // PCB revision strap (register 66; older firmware just doesn't answer)
  if (this->board_revision_text_sensor_) add(HMI_REG_BOARD_REVISION, HMI_TYPE_UINT16, POLL_SLOW);
  // Firmware build ID (registers 68/69; older firmware doesn't answer)
  if (this->bms_firmware_text_sensor_) {
    add(HMI_REG_BUILD_HASH, HMI_TYPE_UINT32, POLL_SLOW);
    add(HMI_REG_BUILD_TIME, HMI_TYPE_UINT32, POLL_SLOW);
  }
  // The user current limits are clamped to the hard limits, and show "no limit" as them
  if (this->max_charge_current_number_) add(HMI_REG_HARD_CHARGE_CURRENT_LIMIT_DA, HMI_TYPE_UINT16, POLL_LIMITS);
  if (this->max_discharge_current_number_) add(HMI_REG_HARD_DISCHARGE_CURRENT_LIMIT_DA, HMI_TYPE_UINT16, POLL_LIMITS);
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

  // 1. Registers: the fast group every poll, plus the group whose slot this is
  int slot_group = -1;
  switch (slot) {
    case 0: slot_group = POLL_SLOW; break;
    case 1: slot_group = POLL_MODULE_TEMPS; break;
    case 3: slot_group = POLL_LIMITS; break;
    case 4: slot_group = POLL_SETTINGS; break;
    default: break;
  }
  std::vector<uint16_t> regs;
  for (const auto &e : this->registers_) {
    if (e.group != POLL_FAST && e.group != slot_group) continue;
    if (!regs.empty() && regs.back() == e.reg) continue;  // sorted: a register can feed several entries
    regs.push_back(e.reg);
  }
  this->send_register_reads_(regs);

  // 2. Read Cell Voltages (Tick 2)
  if (slot == 2 && !this->cell_voltage_sensors_.empty()) {
    ESP_LOGV(TAG, "Polling cell voltages (slot 2). Sensors: %zu", this->cell_voltage_sensors_.size());
    std::vector<uint8_t> cv_payload;
    cv_payload.push_back(HMI_MSG_READ_CELL_VOLTAGES);
    cv_payload.push_back(this->address_);
    this->send_packet_(cv_payload);
  }

  // 3. Read Events (Tick 5): start a new sweep of the event list from index 0. A sweep
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

  // 4. Read Balancing Times (Tick 7)
  if (slot == 7 && (this->cells_balancing_text_sensor_ || this->cells_balancing_active_text_sensor_ ||
                    !this->balancing_time_sensors_.empty())) {
    std::vector<uint8_t> bal_payload;
    bal_payload.push_back(HMI_MSG_READ_BALANCING_TIMES);
    bal_payload.push_back(this->address_);
    this->send_packet_(bal_payload);
  }

  // 5. Read Log (Tick 9) - Poll for new log messages
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

// READ_REGISTERS requests for these registers, split so that no reply passes what the BMS
// sends: it stops adding registers once tx_idx + 11 > 256 (hmi_handle_read_registers).
void HMIBMS::send_register_reads_(const std::vector<uint16_t> &regs) {
  static const size_t REPLY_BUDGET = 245 - 2;  // after the type and address bytes
  std::vector<uint8_t> payload;
  size_t reply = 0;
  auto flush = [&]() {
    if (payload.size() > 2) this->send_packet_(payload);
    payload.clear();
    reply = 0;
  };
  for (uint16_t reg : regs) {
    // Expected reply size of this register (id, type, value), from its table type
    size_t size = 8;
    auto it = std::lower_bound(this->registers_.begin(), this->registers_.end(), reg,
                               [](const RegisterEntry &e, uint16_t r) { return e.reg < r; });
    if (it != this->registers_.end() && it->reg == reg) size = protocol::type_size(it->type);
    if (payload.size() + 2 > protocol::MAX_PAYLOAD || reply + 3 + size > REPLY_BUDGET) flush();
    if (payload.empty()) {
      payload.push_back(HMI_MSG_READ_REGISTERS);
      payload.push_back(this->address_);
    }
    payload.push_back(reg & 0xFF);  // Lo byte first (Little Endian)
    payload.push_back(reg >> 8);
    reply += 3 + size;
  }
  flush();
}

void HMIBMS::send_packet_(const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> frame = protocol::build_frame(payload);
  if (frame.empty()) {
    ESP_LOGW(TAG, "Not sending a payload of %zu bytes (1..%zu fit in a frame)", payload.size(), protocol::MAX_PAYLOAD);
    return;
  }
  ESP_LOGD(TAG, "Sending packet: %s (payload_len=%zu)", format_hex_pretty(frame).c_str(), payload.size());
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
  
  this->publish_register_(HMI_REG_SYSTEM_REQUEST, &request, HMI_TYPE_UINT8);
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

  // 2. Read UART data - a high limit keeps up with the 460800 baud log stream
  uint8_t chunk[256];
  size_t bytes_read = 0;
  while (bytes_read < 2048) {
    size_t avail = this->available();
    if (avail == 0) break;
    size_t n = std::min(avail, sizeof(chunk));
    if (!this->read_array(chunk, n)) break;
    bytes_read += n;
    if (this->dump_raw_) {
      ESP_LOGD(TAG, "Raw Rx: %s", format_hex_pretty(chunk, n).c_str());
    }
    this->rx_.push(chunk, n);
  }

  // 3. Handle complete frames, at most 10 per loop; the rest wait in the parser
  const uint8_t *payload;
  size_t payload_len;
  for (uint8_t handled = 0; handled < 10 && this->rx_.next_frame(payload, payload_len); handled++) {
    this->last_rx_ms_ = millis();
    this->handle_packet_(payload, payload_len);
  }
  if (this->rx_.crc_errors() != this->last_crc_errors_) {
    ESP_LOGW(TAG, "CRC errors: %u so far (frames dropped and resynced)", (unsigned) this->rx_.crc_errors());
    this->last_crc_errors_ = this->rx_.crc_errors();
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
  std::vector<protocol::RegisterValue> regs;
  protocol::parse_registers(data, length, regs);
  ESP_LOGD(TAG, "Read registers response from 0x%02X, len=%zu, registers=%zu", data[1], length, regs.size());

  float t_min = NAN, t_max = NAN, v_min = NAN, v_max = NAN;
  for (const auto &r : regs) {
    // Min/max for the deltas, whether or not their own sensors exist
    double v;
    if ((r.id == HMI_REG_TEMPERATURE_MIN || r.id == HMI_REG_TEMPERATURE_MAX) && protocol::decode_value(r.type, r.value, v)) {
      (r.id == HMI_REG_TEMPERATURE_MIN ? t_min : t_max) = (float) v;
    } else if ((r.id == HMI_REG_CELL_VOLTAGE_MIN || r.id == HMI_REG_CELL_VOLTAGE_MAX) &&
               protocol::type_size(r.type) == 2) {
      (r.id == HMI_REG_CELL_VOLTAGE_MIN ? v_min : v_max) = (float) protocol::get_i16(r.value);
    }
    if (this->handle_special_register_(r)) continue;
    this->publish_register_(r.id, r.value, r.type);
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

// Publishes a register value to every table entry for it: sensors through the publish queue,
// numbers as their confirmed state
void HMIBMS::publish_register_(uint16_t reg, const uint8_t *value, uint8_t wire_type) {
  auto it = std::lower_bound(this->registers_.begin(), this->registers_.end(), reg,
                             [](const RegisterEntry &e, uint16_t r) { return e.reg < r; });
  for (; it != this->registers_.end() && it->reg == reg; ++it) {
    if (it->sensor == nullptr && it->number == nullptr) continue;
    // Decode as the table says when the sizes match (e.g. raw temperatures are sent as
    // uint16 but are signed); otherwise trust the wire type
    const uint8_t type = protocol::type_size(it->type) == protocol::type_size(wire_type) ? it->type : wire_type;
    double raw;
    if (!protocol::decode_value(type, value, raw)) continue;
    const float out = (float) (raw * it->scale);
    if (it->sensor != nullptr) this->publish_queue_.push_back({it->sensor, out});
    if (it->number != nullptr) it->number->publish_from_bus(out);
  }
}

// Registers that need more than value * scale. True when the register is fully handled.
bool HMIBMS::handle_special_register_(const protocol::RegisterValue &r) {
  const uint8_t *v = r.value;
  const size_t size = protocol::type_size(r.type);
  switch (r.id) {
    case HMI_REG_SERIAL:
      if (this->serial_text_sensor_ != nullptr && size == 8) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long) protocol::get_u64(v));
        this->serial_text_sensor_->publish_state(buf);
      }
      return true;
    case HMI_REG_BOARD_REVISION:
      if (this->board_revision_text_sensor_ != nullptr && size == 2) {
        uint16_t code = protocol::get_u16(v);
        char buf[24];
        if (code == 0xFFFF) {
          snprintf(buf, sizeof(buf), "Unknown");
        } else if (code < 10) {
          snprintf(buf, sizeof(buf), "Rev %u", code);
        } else {
          snprintf(buf, sizeof(buf), "Rev %u.%u", code / 10, code % 10);
        }
        this->board_revision_text_sensor_->publish_state(buf);
      }
      return true;
    case HMI_REG_BUILD_HASH:
    case HMI_REG_BUILD_TIME:
      if (r.type == HMI_TYPE_UINT32) {
        uint32_t val = protocol::get_u32(v);
        if (r.id == HMI_REG_BUILD_HASH) {
          this->bms_build_hash_ = val;
          this->bms_build_hash_seen_ = true;
        } else {
          this->bms_build_time_ = val;
        }
        this->publish_bms_firmware_();
      }
      return true;
    case HMI_REG_USER_CHARGE_CURRENT_LIMIT_DA:
    case HMI_REG_USER_DISCHARGE_CURRENT_LIMIT_DA: {
      const bool chg = r.id == HMI_REG_USER_CHARGE_CURRENT_LIMIT_DA;
      HMIBMSNumber *n = chg ? this->max_charge_current_number_ : this->max_discharge_current_number_;
      if (n != nullptr && size == 2) {
        uint16_t val = protocol::get_u16(v);
        float out = (float) val * 0.1f;
        // Keep all readbacks numeric: for the "no limit" value show the hard limit if known
        const bool has_hard = chg ? this->has_last_hard_charge_current_limit_ : this->has_last_hard_discharge_current_limit_;
        if (val == 0xFFFF && has_hard) {
          out = chg ? this->last_hard_charge_current_limit_a_ : this->last_hard_discharge_current_limit_a_;
        }
        n->publish_from_bus(out);
      }
      return true;
    }
    case HMI_REG_HARD_CHARGE_CURRENT_LIMIT_DA:
    case HMI_REG_HARD_DISCHARGE_CURRENT_LIMIT_DA: {
      if (size != 2) return true;
      const bool chg = r.id == HMI_REG_HARD_CHARGE_CURRENT_LIMIT_DA;
      const float out = (float) protocol::get_u16(v) * 0.1f;
      (chg ? this->last_hard_charge_current_limit_a_ : this->last_hard_discharge_current_limit_a_) = out;
      (chg ? this->has_last_hard_charge_current_limit_ : this->has_last_hard_discharge_current_limit_) = true;
      HMIBMSNumber *n = chg ? this->max_charge_current_number_ : this->max_discharge_current_number_;
      if (n != nullptr) n->traits.set_max_value(out);
      return false;  // its sensor, if any, is a normal table entry
    }
    default:
      return false;
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

  std::vector<int32_t> values;
  protocol::decode_delta_array(data + 3, length - 3, num_cells, true, values);
  uint16_t cells_found = 0;
  uint16_t nonzero_cells = 0;
  for (size_t i = 0; i < values.size(); i++) {
    const int32_t cell_voltage = values[i];
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
  
  std::vector<int32_t> times;
  protocol::decode_delta_array(data + 3, length - 3, num_cells, false, times);
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
  
  for (int i = 0; i < (int) times.size(); i++) {
    // Balancing-time counters are non-negative.
    const int32_t time_val = times[i] < 0 ? 0 : times[i];

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
  static const uint8_t MAX_EVENT_PAGES = 8;
  auto &sw = this->event_sweep_;
  if (!sw.active) return;  // a late page from a sweep that was already finished
  uint16_t next_index = 0;
  std::vector<protocol::EventRecord> records;
  if (!protocol::parse_events(data, length, next_index, records)) return;
  ESP_LOGD(TAG, "Read events response from 0x%02X, page %u, events=%zu, next=%u", data[1], sw.pages,
           records.size(), next_index);

  for (const auto &rec : records) {
    const uint16_t type = rec.type;
    // Override: Inverter Detected (Type 45) is always Info level
    const uint16_t level = (type == 45) ? 1 : rec.level;
    const char *type_str = event_type_name(type);
    const char *level_str = (level < sizeof(EVENT_LEVELS) / sizeof(EVENT_LEVELS[0])) ? EVENT_LEVELS[level] : "??";

    char state_buf[32];
    snprintf(state_buf, sizeof(state_buf), "%s (%u)", level_str, rec.count);
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
      sw.latest_count = rec.count;
    }
    if (level > sw.highest_level) sw.highest_level = level;
    if (level < 5) {
      char event_str[64];
      snprintf(event_str, sizeof(event_str), "%s (%u)", type_str, rec.count);
      sw.by_level[level].push_back(event_str);
    }
  }
  sw.pages++;

  if (records.size() >= protocol::EVENTS_PER_FULL_REPLY && sw.pages < MAX_EVENT_PAGES) {
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

void HMIBMS::dump_config() {
  ESP_LOGCONFIG(TAG, "HMI BMS:");
  this->check_uart_settings(this->baud_rate_);
}

}  // namespace hmi_bms
}  // namespace esphome
