#include "hmi_bms.h"
#include "hmi_protocol.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"
#include <algorithm>
#include <ctime>
#include <vector>

#ifdef USE_ESP32_VARIANT_ESP32P4
#include "hal/usb_serial_jtag_ll.h"
#endif

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
#ifdef USE_ESP32_VARIANT_ESP32P4
  // The BMS link on the P4's USB pins (GPIO24/25): the USB PHY's pull-up on D+ is on at
  // reset and stays on when the pin is used as a UART input. It loads the line the BMS and
  // the supervisor answer on. The BMS drives it through 1k and still gets through; the
  // supervisor drives it through 2k and doesn't (seen on a rev 1.1 board, 2026-11-10).
  if (this->disable_usb_pullups_) {
    usb_serial_jtag_pull_override_vals_t pulls = {};  // every pull-up and pull-down off
    usb_serial_jtag_ll_phy_enable_pull_override(&pulls);
  }
#endif
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
  if (this->board_revision_text_sensor_) add(HMI_REG_BOARD_REVISION, HMI_TYPE_UINT16, POLL_SLOW);
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
  if (this->link_state_ != LINK_READY) {
    this->poll_link_();
    return;
  }

  // Slot 0 comes first after the link is up, so the slow registers arrive at once
  uint8_t slot = this->update_count_ % 10;
  this->update_count_++;

  // The supervisor's status (Tick 6). It is a separate chip that answers for itself on the
  // same line as the BMS, so nothing is asked of the BMS in this tick: two answers at once
  // would collide. After three requests without an answer it is taken as not there.
  if (slot == 6 && this->wants_supervisor_()) {
    if (this->supervisor_unanswered_ >= 3) {
      this->publish_text_(this->supervisor_state_text_sensor_, "No answer");
    } else {
      this->supervisor_unanswered_++;
    }
    this->send_packet_({HMI_MSG_READ_SUPERVISOR, this->address_});
    return;
  }

  // 1. Registers: the fast group every poll, plus the group whose slot this is. Only the
  // registers this BMS firmware has are asked for.
  int slot_group = -1;
  switch (slot) {
    case 0: slot_group = POLL_SLOW; break;
    case 3: slot_group = POLL_LIMITS; break;
    case 4: slot_group = POLL_SETTINGS; break;
    default: break;
  }
  std::vector<uint16_t> regs;
  for (const auto &e : this->registers_) {
    if (e.group != POLL_FAST && e.group != slot_group) continue;
    if (!regs.empty() && regs.back() == e.reg) continue;  // sorted: a register can feed several entries
    if (!this->bms_has_register_(e.reg)) continue;
    regs.push_back(e.reg);
  }
  this->send_register_reads_(regs);

  // 2. Arrays, each on its own slot
  if (slot == 0 && this->module_ids_text_sensor_ != nullptr) this->request_array_(HMI_ARRAY_MODULE_IDS);
  if (slot == 1 && !this->module_temperature_sensors_.empty()) this->request_array_(HMI_ARRAY_MODULE_TEMPS);
  if (slot == 2 && (!this->cell_voltage_sensors_.empty() || this->cell_count_sensor_ != nullptr)) {
    this->request_array_(HMI_ARRAY_CELL_VOLTAGES);
  }
  if (slot == 7 && (this->cells_balancing_text_sensor_ || this->cells_balancing_active_text_sensor_ ||
                    !this->balancing_time_sensors_.empty())) {
    this->request_array_(HMI_ARRAY_BALANCING_TIMES);
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
    sw.latest_timestamp = 0;
    this->request_events_(0);
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
      case HMI_MSG_READ_ARRAY_RESPONSE:
        this->handle_array_page_(payload + offset, length - offset);
        return;
      case HMI_MSG_LIST_REGISTERS_RESPONSE:
        this->handle_list_registers_response_(payload + offset, length - offset);
        return;
      case HMI_MSG_NACK:
        this->handle_nack_(payload + offset, length - offset);
        return;
      case HMI_MSG_READ_SUPERVISOR_RESPONSE:
        this->handle_supervisor_response_(payload + offset, length - offset);
        return;
      case HMI_MSG_READ_EVENTS_RESPONSE:
        this->handle_read_events_response_(payload + offset, length - offset);
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
          // Address 0 means the BMS has restarted: it may be other firmware now
          if (address == 0 && this->link_state_ != LINK_CHECK_VERSION) this->start_link_check_();
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
  if (this->link_state_ != LINK_READY) {
    // Nothing is published until the BMS is known to speak this protocol
    if (this->link_state_ != LINK_LIST_REGISTERS) this->handle_version_reply_(regs);
    return;
  }

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

// --- The link: protocol version, then the BMS's register list ---

void HMIBMS::start_link_check_() {
  this->link_state_ = LINK_CHECK_VERSION;
  this->link_polls_ = 0;
  this->bms_registers_.clear();
  this->array_read_.active = false;
  this->array_queue_.clear();
  this->event_sweep_.active = false;
}

void HMIBMS::publish_protocol_status_(const std::string &status) {
  if (this->protocol_text_sensor_ == nullptr) return;
  if (!this->protocol_text_sensor_->has_state() || this->protocol_text_sensor_->state != status) {
    this->protocol_text_sensor_->publish_state(status);
  }
}

// Called on every update until the link is ready. Requests are spaced out: every 5th update
// while checking, every 20th once the BMS has turned out to speak something else.
void HMIBMS::poll_link_() {
  const uint32_t n = this->link_polls_++;
  switch (this->link_state_) {
    case LINK_CHECK_VERSION:
    case LINK_INCOMPATIBLE:
      if (n % (this->link_state_ == LINK_INCOMPATIBLE ? 20 : 5) == 0) {
        this->send_register_reads_({HMI_REG_PROTOCOL_VERSION});
      }
      break;
    case LINK_LIST_REGISTERS:
      // Ask again from where the list has got to if a page was lost
      if (n % 5 == 0) {
        this->send_packet_(protocol::list_registers_request(this->address_, (uint16_t) this->bms_registers_.size()));
      }
      break;
    default:
      break;
  }
}

// The answer to the PROTOCOL_VERSION read. Firmware from before this protocol has no such
// register and answers without it.
void HMIBMS::handle_version_reply_(const std::vector<protocol::RegisterValue> &regs) {
  int version = -1;
  for (const auto &r : regs) {
    double v;
    if (r.id == HMI_REG_PROTOCOL_VERSION && protocol::decode_value(r.type, r.value, v)) version = (int) v;
  }
  this->bms_protocol_version_ = version;
  if (version == HMI_PROTOCOL_VERSION) {
    if (this->link_state_ == LINK_INCOMPATIBLE) ESP_LOGI(TAG, "BMS now speaks protocol %d", version);
    this->link_state_ = LINK_LIST_REGISTERS;
    this->link_polls_ = 0;
    this->bms_registers_.clear();
    this->poll_link_();
    return;
  }
  if (this->link_state_ != LINK_INCOMPATIBLE) {
    ESP_LOGE(TAG, "BMS speaks HMI protocol %s, this HMI needs %u: nothing will be read",
             version < 0 ? "1 or older" : str_sprintf("%d", version).c_str(), (unsigned) HMI_PROTOCOL_VERSION);
  }
  this->link_state_ = LINK_INCOMPATIBLE;
  this->publish_protocol_status_(version < 0
                                     ? str_sprintf("1 or older (this HMI needs %u)", (unsigned) HMI_PROTOCOL_VERSION)
                                     : str_sprintf("%d (this HMI needs %u)", version, (unsigned) HMI_PROTOCOL_VERSION));
}

void HMIBMS::handle_list_registers_response_(const uint8_t *data, size_t length) {
  if (this->link_state_ != LINK_LIST_REGISTERS) return;
  uint16_t start = 0, total = 0;
  std::vector<protocol::RegisterInfo> page;
  if (!protocol::parse_register_list(data, length, start, total, page)) return;
  if (start != this->bms_registers_.size()) return;  // a repeated or late page
  this->bms_registers_.insert(this->bms_registers_.end(), page.begin(), page.end());
  if (this->bms_registers_.size() < total && !page.empty()) {
    this->send_packet_(protocol::list_registers_request(this->address_, (uint16_t) this->bms_registers_.size()));
    return;
  }
  std::sort(this->bms_registers_.begin(), this->bms_registers_.end(),
            [](const protocol::RegisterInfo &a, const protocol::RegisterInfo &b) { return a.id < b.id; });
  size_t missing = 0;
  for (const auto &e : this->registers_) {
    if (e.group != POLL_NEVER && !this->bms_has_register_(e.reg)) {
      ESP_LOGW(TAG, "The BMS has no register 0x%04X: its sensor stays unknown", e.reg);
      missing++;
    }
  }
  ESP_LOGI(TAG, "BMS speaks protocol %d and has %zu registers (%zu wanted here are missing)",
           this->bms_protocol_version_, this->bms_registers_.size(), missing);
  this->link_state_ = LINK_READY;
  this->update_count_ = 0;
  this->publish_protocol_status_(str_sprintf("%d", this->bms_protocol_version_));
  this->update();
}

bool HMIBMS::bms_has_register_(uint16_t reg) const {
  auto it = std::lower_bound(this->bms_registers_.begin(), this->bms_registers_.end(), reg,
                             [](const protocol::RegisterInfo &e, uint16_t r) { return e.id < r; });
  return it != this->bms_registers_.end() && it->id == reg;
}

// The BMS refused a request (a message type it doesn't know, or one too short)
void HMIBMS::handle_nack_(const uint8_t *data, size_t length) {
  protocol::Nack nack;
  if (!protocol::parse_nack(data, length, nack)) return;
  ESP_LOGW(TAG, "The BMS refused message 0x%02X (reason %u)", nack.request, nack.reason);
  if (nack.request == HMI_MSG_READ_LOG) this->log_busy_ = false;
  if (nack.request == HMI_MSG_READ_ARRAY) this->finish_array_read_(false);
  if (nack.request == HMI_MSG_READ_EVENTS) this->event_sweep_.active = false;
}

void HMIBMS::publish_text_(text_sensor::TextSensor *s, const std::string &state) {
  if (s != nullptr && (!s->has_state() || s->state != state)) s->publish_state(state);
}

// The supervisor's status: whether it enables the contactor drivers, and if not, why
void HMIBMS::handle_supervisor_response_(const uint8_t *data, size_t length) {
  protocol::SupervisorStatus st;
  if (!protocol::parse_supervisor(data, length, st)) return;
  this->supervisor_unanswered_ = 0;

  static const char *const STATES[] = {"Disabled", "Enabled", "Latched off"};
  this->publish_text_(this->supervisor_state_text_sensor_, st.state < 3 ? STATES[st.state] : "Unknown");

  auto names = [](uint16_t bits) {
    std::string out;
    for (const auto &r : HMI_SUPERVISOR_REASONS) {
      if (!(bits & r.bit)) continue;
      if (!out.empty()) out += ", ";
      out += r.name;
      bits &= (uint16_t) ~r.bit;
    }
    if (bits != 0) out += str_sprintf("%s0x%04X", out.empty() ? "" : ", ", bits);
    return out;
  };
  std::string reasons = st.reasons == 0 ? "none" : names(st.reasons);
  if (st.latch_reasons != 0) reasons += " (latched by " + names(st.latch_reasons) + ")";
  this->publish_text_(this->supervisor_reasons_text_sensor_, reasons);

  // Its cell readings are its own, from the cell board traffic; -1 mV = none
  if (this->supervisor_cell_min_sensor_) {
    this->publish_queue_.push_back({this->supervisor_cell_min_sensor_, st.cell_min_mv < 0 ? NAN : st.cell_min_mv / 1000.0f});
  }
  if (this->supervisor_cell_max_sensor_) {
    this->publish_queue_.push_back({this->supervisor_cell_max_sensor_, st.cell_max_mv < 0 ? NAN : st.cell_max_mv / 1000.0f});
  }
  if (this->supervisor_cells_in_range_sensor_) {
    this->publish_queue_.push_back({this->supervisor_cells_in_range_sensor_, (float) st.cells_in_range});
  }
  if (this->supervisor_scans_sensor_) {
    this->publish_queue_.push_back({this->supervisor_scans_sensor_, (float) st.scans});
  }
  if (this->supervisor_scan_age_sensor_) {
    this->publish_queue_.push_back(
        {this->supervisor_scan_age_sensor_, st.ms_since_scan == 0xFFFFFFFFu ? NAN : st.ms_since_scan / 1000.0f});
  }
}

void HMIBMS::dump_config() {
  ESP_LOGCONFIG(TAG, "HMI BMS:");
  if (this->disable_usb_pullups_) ESP_LOGCONFIG(TAG, "  USB pin pull-ups: switched off");
  this->check_uart_settings(this->baud_rate_);
}

}  // namespace hmi_bms
}  // namespace esphome
