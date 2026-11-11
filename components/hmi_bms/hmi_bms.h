#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include <deque>
#include "hmi_protocol.h"
#include "hmi_map.h"

namespace esphome {
namespace hmi_bms {

class HMIBMS;

class HMIBMSNumber : public number::Number {
 public:
  HMIBMSNumber(HMIBMS *parent, uint16_t reg_id, uint8_t type, float scale = 1.0f)
      : parent_(parent), reg_id_(reg_id), type_(type), scale_(scale) {}
  void control(float value) override;
  void publish_from_bus(float value) {
    this->last_bus_state_ = value;
    this->has_bus_state_ = true;
    this->publish_state(value);
  }
  void revert_to_bus_state() {
    if (this->has_bus_state_) {
      this->publish_state(this->last_bus_state_);
    }
  }
  // For on-device automations (the weekly balance charge): write without the
  // user's settings unlock. The BMS readback then updates the state.
  void write_from_automation(float value);
  bool has_bus_state() const { return this->has_bus_state_; }
  float bus_state() const { return this->last_bus_state_; }
  uint16_t reg_id() const { return this->reg_id_; }
  uint8_t type() const { return this->type_; }
  float scale() const { return this->scale_; }

 protected:
  HMIBMS *parent_;
  uint16_t reg_id_;
  uint8_t type_;
  float scale_;
  float last_bus_state_{0.0f};
  bool has_bus_state_{false};
};

// The settings lock: writes from HA or the LCD need it unlocked, and it locks itself after
// 30 s without a write. It drives the "Settings Write Status" text sensor.
class SettingsLock {
 public:
  void set_status_sensor(text_sensor::TextSensor *s) { status_ = s; }
  void set_unlocked(bool unlocked);
  bool is_unlocked() const { return unlocked_; }
  // A user write keeps it unlocked for another 30 s
  void note_write() { last_write_ms_ = millis(); }
  void publish_status(const char *status);
  // Shows status for duration_ms, then goes back to the countdown or "Settings locked"
  void publish_status_temporary(const char *status, uint32_t duration_ms);
  void loop();

 protected:
  static const uint32_t UNLOCK_TIMEOUT_MS = 30000;
  text_sensor::TextSensor *status_{nullptr};
  bool unlocked_{false};
  uint32_t last_write_ms_{0};
  uint32_t last_countdown_s_{UINT32_MAX};
  bool restore_pending_{false};
  uint32_t restore_at_ms_{0};
};

class HMIBMSLogSwitch : public switch_::Switch {
 public:
  void set_parent(HMIBMS *parent) { parent_ = parent; }

 protected:
  void write_state(bool state) override;
  HMIBMS *parent_{nullptr};
};

enum HMIType {
  HMI_TYPE_UINT8 = 0x11,
  HMI_TYPE_INT8 = 0x21,
  HMI_TYPE_UINT16 = 0x12,
  HMI_TYPE_INT16 = 0x22,
  HMI_TYPE_UINT32 = 0x14,
  HMI_TYPE_INT32 = 0x24,
  HMI_TYPE_UINT64 = 0x18,
  HMI_TYPE_INT64 = 0x28,
  HMI_TYPE_FLOAT = 0x34,
  HMI_TYPE_DOUBLE = 0x38,
};

// Message types, register IDs, array IDs and event types are in hmi_map.h, generated from the
// BMS firmware's headers.

// When a register is read: FAST on every poll, the others on their own slot of the 10-poll cycle
enum PollGroup : uint8_t {
  POLL_FAST = 0,
  POLL_SLOW = 1,      // slot 0
  POLL_LIMITS = 3,    // slot 3
  POLL_SETTINGS = 4,  // slot 4
  POLL_NEVER = 5,     // a command: written, never read back
};

class HMIBMS : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  // Register table: a sensor or number fed from one BMS register. type is how the value is
  // decoded (used when its size matches the wire type), entity value = raw * scale.
  void add_register_sensor(uint16_t reg, uint8_t type, float scale, uint8_t group, sensor::Sensor *s) {
    this->registers_.push_back({reg, type, group, scale, s, nullptr});
  }
  void add_register_number(uint8_t group, HMIBMSNumber *n) {
    this->registers_.push_back({n->reg_id(), n->type(), group, n->scale(), nullptr, n});
  }

  void set_serial_text_sensor(text_sensor::TextSensor *s) { serial_text_sensor_ = s; }
  void set_board_revision_text_sensor(text_sensor::TextSensor *s) { board_revision_text_sensor_ = s; }
  // Which firmware the BMS and this HMI run (registers 68/69; the HMI's own commit from codegen)
  void set_bms_firmware_text_sensor(text_sensor::TextSensor *s) { bms_firmware_text_sensor_ = s; }
  void set_hmi_firmware_text_sensor(text_sensor::TextSensor *s) { hmi_firmware_text_sensor_ = s; }
  void set_hmi_build(const std::string &build) { hmi_build_ = build; }
  // The BMBs' module IDs, in chain order (array HMI_ARRAY_MODULE_IDS)
  void set_module_ids_text_sensor(text_sensor::TextSensor *s) { module_ids_text_sensor_ = s; }
  // The HMI protocol version the BMS speaks, or why this HMI can't talk to it
  void set_protocol_text_sensor(text_sensor::TextSensor *s) { protocol_text_sensor_ = s; }
  // True once the BMS has answered with the protocol version this HMI speaks and its register list
  bool is_link_ready() const { return this->link_state_ == LINK_READY; }
  // True when a BMS answers but speaks another protocol version
  bool is_incompatible() const { return this->link_state_ == LINK_INCOMPATIBLE; }
  // Calculated here rather than read
  void set_temperature_delta_sensor(sensor::Sensor *s) { temperature_delta_sensor_ = s; }
  void set_cell_voltage_delta_sensor(sensor::Sensor *s) { cell_voltage_delta_sensor_ = s; }
  void set_cell_count_sensor(sensor::Sensor *s) { cell_count_sensor_ = s; }
  // The user current limits also clamp to the hard limits and show "no limit" as the hard limit
  void set_max_charge_current_number(HMIBMSNumber *n) { max_charge_current_number_ = n; }
  void set_max_discharge_current_number(HMIBMSNumber *n) { max_discharge_current_number_ = n; }

  void set_last_event_sensor(sensor::Sensor *s) { last_event_sensor_ = s; }
  void set_highest_event_level_sensor(sensor::Sensor *s) { highest_event_level_sensor_ = s; }
  void set_event_history_text_sensor(text_sensor::TextSensor *s) { event_history_text_sensor_ = s; }
  void set_cells_balancing_text_sensor(text_sensor::TextSensor *s) { cells_balancing_text_sensor_ = s; }
  void set_cells_balancing_active_text_sensor(text_sensor::TextSensor *s) { cells_balancing_active_text_sensor_ = s; }
  void set_log_text_sensor(text_sensor::TextSensor *s) { log_text_sensor_ = s; }
  void add_log_line_sensor(text_sensor::TextSensor *s) { log_line_sensors_.push_back(s); }
  text_sensor::TextSensor *get_event_history_text_sensor() const { return event_history_text_sensor_; }
  void add_cell_voltage_sensor(sensor::Sensor *s) { cell_voltage_sensors_.push_back(s); }
  void add_balancing_time_sensor(sensor::Sensor *s) { balancing_time_sensors_.push_back(s); }
  void add_module_temperature_sensor(sensor::Sensor *s) { module_temperature_sensors_.push_back(s); }
  const std::vector<sensor::Sensor *> &get_cell_voltage_sensors() const { return cell_voltage_sensors_; }
  const std::vector<sensor::Sensor *> &get_balancing_time_sensors() const { return balancing_time_sensors_; }
  // One text sensor per BMS event type (index = type), state "LEVEL (count)" or "INACTIVE"; for the LCD
  const std::vector<text_sensor::TextSensor *> &get_event_sensors() const { return error_sensors_; }
  // BMS name of an event type, e.g. "CELL_VOLTAGE_HIGH"; "UNKNOWN" outside the table
  static const char *event_type_name(int type);
  // ms since the last valid frame from the BMS; UINT32_MAX before the first one
  uint32_t ms_since_rx() const { return this->last_rx_ms_ == 0 ? UINT32_MAX : millis() - this->last_rx_ms_; }
  void set_bypass_crc(bool bypass) { rx_.set_bypass_crc(bypass); }
  void set_baud_rate(uint32_t baud) { baud_rate_ = baud; }
  void set_dump_raw(bool dump) { dump_raw_ = dump; }
  void set_address(uint8_t address) { address_ = address; }
  void set_log_sync_prefix(const std::string &prefix) { log_sync_prefix_ = prefix; }
  void set_log_enable_switch(HMIBMSLogSwitch *s) { log_enable_switch_ = s; }
  bool is_log_enabled() const { return log_enabled_; }
  void set_log_enabled(bool enabled) { log_enabled_ = enabled; }
  uint32_t get_log_update_interval() const { return log_update_interval_ms_; }
  void set_log_update_interval(uint32_t ms) { log_update_interval_ms_ = ms; }
  void set_settings_unlocked(bool unlocked) { settings_.set_unlocked(unlocked); }
  bool is_settings_unlocked() const { return settings_.is_unlocked(); }
  void set_settings_status_text_sensor(text_sensor::TextSensor *s) { settings_.set_status_sensor(s); }
  void publish_settings_status(const char *status) { settings_.publish_status(status); }
  void publish_settings_status_temporary(const char *status, uint32_t duration_ms) {
    settings_.publish_status_temporary(status, duration_ms);
  }
  void send_system_request(uint8_t request);
  // bypass_lock is only for HMIBMSNumber::write_from_automation()
  void write_register(uint16_t reg_id, uint8_t type, float value, float scale, bool bypass_lock = false);

  void set_error_sensor(int type_id, text_sensor::TextSensor *s) {
    if (type_id < 0) return;
    if (this->error_sensors_.size() <= (size_t)type_id) {
      this->error_sensors_.resize(type_id + 1, nullptr);
    }
    this->error_sensors_[type_id] = s;
  }

  int get_last_event_code() const { return last_event_code_; }
  int get_last_event_level() const { return last_event_level_; }
  int get_last_event_count() const { return last_event_count_; }
  const char* get_last_event_type_str() const { return last_event_type_str_.c_str(); }
  bool has_hard_charge_current_limit() const { return has_last_hard_charge_current_limit_; }
  bool has_hard_discharge_current_limit() const { return has_last_hard_discharge_current_limit_; }
  float get_hard_charge_current_limit_a() const { return last_hard_charge_current_limit_a_; }
  float get_hard_discharge_current_limit_a() const { return last_hard_discharge_current_limit_a_; }

 protected:
  void handle_packet_(const uint8_t *payload, size_t length);
  void handle_message_(const uint8_t *msg, size_t length);
  void handle_read_registers_response_(const uint8_t *data, size_t length);
  void handle_list_registers_response_(const uint8_t *data, size_t length);
  void handle_array_page_(const uint8_t *data, size_t length);
  void handle_nack_(const uint8_t *data, size_t length);
  void handle_read_log_response_(const uint8_t *data, size_t length);
  void publish_log_();
  void handle_read_events_response_(const uint8_t *data, size_t length);
  
  void send_packet_(const std::vector<uint8_t> &payload);
  

  uint8_t address_{0x00};
  uint64_t serial_{0};
  // Address assignment: last SET_DEVICE_ADDRESS sent, and whether address 1 was claimed
  uint32_t last_assign_ms_{0};
  bool address_claimed_{false};
  uint32_t update_count_{0};
  protocol::FrameParser rx_;
  uint32_t last_crc_errors_{0};

  // What the BMS speaks, settled before anything else is polled: its protocol version
  // (register PROTOCOL_VERSION must read HMI_PROTOCOL_VERSION), then the registers it has
  // (LIST_REGISTERS), so only those are asked for. Checked again when the BMS restarts.
  enum LinkState : uint8_t { LINK_CHECK_VERSION, LINK_LIST_REGISTERS, LINK_READY, LINK_INCOMPATIBLE };
  LinkState link_state_{LINK_CHECK_VERSION};
  uint32_t link_polls_{0};
  int bms_protocol_version_{-1};  // -1: the BMS answered without a version (older than this protocol)
  std::vector<protocol::RegisterInfo> bms_registers_;  // in increasing ID order, as the BMS lists them
  text_sensor::TextSensor *protocol_text_sensor_{nullptr};
  void start_link_check_();
  void poll_link_();
  void handle_version_reply_(const std::vector<protocol::RegisterValue> &regs);
  void publish_protocol_status_(const std::string &status);
  bool bms_has_register_(uint16_t reg) const;

  // Arrays (cell voltages, balancing times, module temperatures, module IDs) are read a page
  // at a time, one array at a time; the others wait their turn.
  struct ArrayRead {
    bool active{false};
    uint8_t id{0};
    uint8_t pages{0};
    uint32_t started_ms{0};
    std::vector<double> values;
  } array_read_;
  std::vector<uint8_t> array_queue_;
  void request_array_(uint8_t array_id);
  void send_array_request_(uint8_t array_id, uint16_t start);
  void finish_array_read_(bool complete);
  void publish_cell_voltages_(const std::vector<double> &values);
  void publish_balancing_times_(const std::vector<double> &values);
  void publish_module_temperatures_(const std::vector<double> &values);
  void publish_module_ids_(const std::vector<double> &values);
  
  struct RegisterEntry {
    uint16_t reg;
    uint8_t type;
    uint8_t group;
    float scale;
    sensor::Sensor *sensor;
    HMIBMSNumber *number;
  };
  std::vector<RegisterEntry> registers_;  // sorted by reg in setup()
  void add_internal_registers_();
  void send_register_reads_(const std::vector<uint16_t> &regs);
  bool handle_special_register_(const protocol::RegisterValue &r);
  void publish_register_(uint16_t reg, const uint8_t *value, uint8_t wire_type);

  text_sensor::TextSensor *serial_text_sensor_{nullptr};
  text_sensor::TextSensor *board_revision_text_sensor_{nullptr};
  text_sensor::TextSensor *bms_firmware_text_sensor_{nullptr};
  text_sensor::TextSensor *module_ids_text_sensor_{nullptr};
  text_sensor::TextSensor *hmi_firmware_text_sensor_{nullptr};
  std::string hmi_build_{"unknown"};
  uint32_t bms_build_hash_{0};
  uint32_t bms_build_time_{0};
  bool bms_build_hash_seen_{false};
  void publish_bms_firmware_();
  sensor::Sensor *temperature_delta_sensor_{nullptr};
  sensor::Sensor *cell_voltage_delta_sensor_{nullptr};
  sensor::Sensor *cell_count_sensor_{nullptr};
  HMIBMSNumber *max_charge_current_number_{nullptr};
  HMIBMSNumber *max_discharge_current_number_{nullptr};
  // Last raw min/max off the bus, for the deltas
  float last_temperature_min_c_{NAN};
  float last_temperature_max_c_{NAN};
  float last_cell_voltage_min_mv_{NAN};
  float last_cell_voltage_max_mv_{NAN};
  float last_hard_charge_current_limit_a_{0.0f};
  float last_hard_discharge_current_limit_a_{0.0f};
  bool has_last_hard_charge_current_limit_{false};
  bool has_last_hard_discharge_current_limit_{false};
  sensor::Sensor *last_event_sensor_{nullptr};
  sensor::Sensor *highest_event_level_sensor_{nullptr};
  text_sensor::TextSensor *event_history_text_sensor_{nullptr};
  text_sensor::TextSensor *cells_balancing_text_sensor_{nullptr};
  text_sensor::TextSensor *cells_balancing_active_text_sensor_{nullptr};
  std::vector<int32_t> balancing_time_prev_;
  std::vector<uint32_t> balancing_active_until_ms_;
  text_sensor::TextSensor *log_text_sensor_{nullptr};
  std::vector<text_sensor::TextSensor *> log_line_sensors_;
  uint16_t log_line_index_{0};
  std::vector<std::string> log_line_pending_;
  std::vector<std::string> log_line_snapshot_;
  bool log_line_snapshot_dirty_{false};
  uint32_t last_log_line_publish_time_{0};
  std::vector<sensor::Sensor *> cell_voltage_sensors_;
  std::vector<sensor::Sensor *> balancing_time_sensors_;
  std::vector<sensor::Sensor *> module_temperature_sensors_;
  uint32_t baud_rate_{460800};
  bool dump_raw_{false};
  SettingsLock settings_;
  uint32_t last_rx_ms_{0};
  bool log_enabled_{false};
  uint32_t log_update_interval_ms_{2000};
  HMIBMSLogSwitch *log_enable_switch_{nullptr};
  int last_event_code_{-1};
  int last_event_level_{0};
  int last_event_count_{0};
  std::string last_event_type_str_{"UNKNOWN"};
  std::string log_buffer_;
  std::string log_sync_prefix_;
  uint32_t last_log_request_time_{0};
  uint32_t last_log_publish_time_{0};
  bool log_busy_{false};
  std::deque<std::string> log_publish_queue_;
  std::vector<text_sensor::TextSensor *> error_sensors_;

  // One read of the BMS event list: a reply holds at most 11 events, so it can take
  // several pages (next_index). Types are marked inactive only after the last page.
  void request_events_(uint16_t start_index);
  void finish_event_sweep_();
  struct EventSweep {
    bool active{false};
    uint8_t pages{0};
    std::vector<bool> seen;
    std::vector<std::string> by_level[5];
    uint16_t highest_level{0};
    int latest_type{-1};
    int latest_level{0};
    int latest_count{0};
  } event_sweep_;

  struct SensorUpdate {
    sensor::Sensor *sensor;
    float state;
  };
  std::vector<SensorUpdate> publish_queue_;
};

}  // namespace hmi_bms
}  // namespace esphome
