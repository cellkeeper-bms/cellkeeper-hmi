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

  if (!bypass_lock && !this->settings_.is_unlocked()) {
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
    this->settings_.note_write();
  }
}

void HMIBMS::loop() {
  this->settings_.loop();

  // 1. Publish the sensor updates queued while parsing
  if (!this->publish_queue_.empty()) {
    std::vector<SensorUpdate> queue;
    queue.swap(this->publish_queue_);
    for (const auto &u : queue) {
      if (u.sensor != nullptr) u.sensor->publish_state(u.state);
    }
  }

  this->publish_log_();

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
            uint32_t now = millis();
            
            // Only assign/claim if we haven't done it recently, or if it's still at 0
            if ((address == 0 && now - this->last_assign_ms_ > 2000) || (address == 1 && !this->address_claimed_)) {
              this->last_assign_ms_ = now;
              this->address_claimed_ = (address == 1);
              
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
    // Decode as the table says when the sizes match; otherwise trust the wire type
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

void HMIBMS::dump_config() {
  ESP_LOGCONFIG(TAG, "HMI BMS:");
  this->check_uart_settings(this->baud_rate_);
}

}  // namespace hmi_bms
}  // namespace esphome
