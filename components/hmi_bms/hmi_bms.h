#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include <deque>

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

 protected:
  HMIBMS *parent_;
  uint16_t reg_id_;
  uint8_t type_;
  float scale_;
  float last_bus_state_{0.0f};
  bool has_bus_state_{false};
};

class HMIBMSLogSwitch : public switch_::Switch {
 public:
  void set_parent(HMIBMS *parent) { parent_ = parent; }

 protected:
  void write_state(bool state) override;
  HMIBMS *parent_{nullptr};
};

enum HMIMsgType {
  HMI_MSG_ANNOUNCE_DEVICE = 0x81,
  HMI_MSG_SET_DEVICE_ADDRESS = 0x01,
  HMI_MSG_WRITE_REGISTERS = 0x03,
  HMI_MSG_READ_REGISTERS = 0x04,
  HMI_MSG_READ_REGISTERS_RESPONSE = 0x84,
  HMI_MSG_READ_CELL_VOLTAGES = 0x05,
  HMI_MSG_READ_CELL_VOLTAGES_RESPONSE = 0x85,
  HMI_MSG_READ_EVENTS = 0x06,
  HMI_MSG_READ_EVENTS_RESPONSE = 0x86,
  HMI_MSG_READ_BALANCING_TIMES = 0x07,
  HMI_MSG_READ_BALANCING_TIMES_RESPONSE = 0x87,
  HMI_MSG_READ_LOG = 0x08,
  HMI_MSG_READ_LOG_RESPONSE = 0x88,
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

enum HMIRegister {
  HMI_REG_SERIAL = 1,
  HMI_REG_MILLIS = 2,
  HMI_REG_SOC = 3,
  HMI_REG_CURRENT = 4,
  HMI_REG_CHARGE = 5,
  HMI_REG_BATTERY_VOLTAGE = 6,
  HMI_REG_OUTPUT_VOLTAGE = 7,
  HMI_REG_POS_CONTACTOR_VOLTAGE = 8,
  HMI_REG_NEG_CONTACTOR_VOLTAGE = 9,
  HMI_REG_TEMPERATURE_MIN = 10,
  HMI_REG_TEMPERATURE_MAX = 11,
  HMI_REG_CELL_VOLTAGE_MIN = 12,
  HMI_REG_CELL_VOLTAGE_MAX = 13,
  HMI_REG_SYSTEM_REQUEST = 14,
  HMI_REG_SYSTEM_STATE = 15,
  HMI_REG_CONTACTORS_STATE = 16,
  HMI_REG_SOC_VOLTAGE_BASED = 17,
  HMI_REG_SOC_BASIC_COUNT = 18,
  HMI_REG_CAPACITY = 20,
  HMI_REG_SUPPLY_VOLTAGE_3V3 = 21,
  HMI_REG_SUPPLY_VOLTAGE_5V = 22,
  HMI_REG_SUPPLY_VOLTAGE_12V = 23,
  HMI_REG_SUPPLY_VOLTAGE_CTR = 24,
  HMI_REG_CELL_VOLTAGE_WORKING_MIN = 25,
  HMI_REG_CELL_VOLTAGE_WORKING_MAX = 26,
  HMI_REG_SOC_SCALING_MIN = 27,
  HMI_REG_SOC_SCALING_MAX = 28,
  HMI_REG_VOLTAGE_LIMIT_OFFSET_LOWER = 29,
  HMI_REG_VOLTAGE_LIMIT_OFFSET_UPPER = 30,
  HMI_REG_CELL_VOLTAGE_SOFT_MIN = 31,
  HMI_REG_CELL_VOLTAGE_SOFT_MAX = 32,
  HMI_REG_AUTO_BALANCING_PERIOD_MS = 33,
  HMI_REG_BALANCING_PERIODS_PER_MV = 34,
  HMI_REG_BALANCE_MIN_OFFSET_MV = 35,
  HMI_REG_MINIMUM_BALANCING_VOLTAGE_MV = 36,
  HMI_REG_INVERTER_SOC = 37,               // int16 (0.01%)
  HMI_REG_INVERTER_FULL_CAPACITY_DAH = 38, // int16 (0.1Ah)
  HMI_REG_INVERTER_REMAINING_CAPACITY_DAH = 39, // int16 (0.1Ah)
  HMI_REG_INVERTER_MIN_VOLTAGE_LIMIT_DV = 40,   // int16 (0.1V)
  HMI_REG_INVERTER_MAX_VOLTAGE_LIMIT_DV = 41,   // int16 (0.1V)
  HMI_REG_BALANCING_START_VOLTAGE_MV = 42,      // uint16 (mV)
  HMI_REG_USER_CHARGE_CURRENT_LIMIT_DA = 43,    // uint16 (0.1A)
  HMI_REG_USER_DISCHARGE_CURRENT_LIMIT_DA = 44, // uint16 (0.1A)
  HMI_REG_CHARGE_CURRENT_LIMIT_DA = 45,         // uint16 (0.1A)
  HMI_REG_DISCHARGE_CURRENT_LIMIT_DA = 46,      // uint16 (0.1A)
  HMI_REG_TEMP_CHARGE_CURRENT_LIMIT_DA = 47,    // uint16 (0.1A)
  HMI_REG_TEMP_DISCHARGE_CURRENT_LIMIT_DA = 48, // uint16 (0.1A)
  HMI_REG_CELL_VOLTAGE_CHARGE_CURRENT_LIMIT_DA = 49,    // uint16 (0.1A)
  HMI_REG_CELL_VOLTAGE_DISCHARGE_CURRENT_LIMIT_DA = 50, // uint16 (0.1A)
  HMI_REG_WORKING_CHARGE_CURRENT_LIMIT_DA = 51,         // uint16 (0.1A)
  HMI_REG_WORKING_DISCHARGE_CURRENT_LIMIT_DA = 52,      // uint16 (0.1A)
  HMI_REG_DELTA_CHARGE_CURRENT_LIMIT_DA = 53,           // uint16 (0.1A)
  HMI_REG_HARD_CHARGE_CURRENT_LIMIT_DA = 54,            // uint16 (0.1A)
  HMI_REG_HARD_DISCHARGE_CURRENT_LIMIT_DA = 55,         // uint16 (0.1A)
  // 60/61 (raw isolation ADC counts) exist on the BMS but are not shown here
  HMI_REG_ISOLATION_RESISTANCE = 62,                    // float (ohms), NAN unless contactors open
  HMI_REG_PACK_POS_TO_CHASSIS = 63,                     // float (V)
  HMI_REG_PACK_NEG_TO_CHASSIS = 64,                     // float (V)
  HMI_REG_ISOLATION_CHECK_AGE = 65,                     // float (s); 0 = live, NAN = not measured yet
  HMI_REG_BOARD_REVISION = 66,                          // uint16: major*10+minor (Rev1.3 = 13), 0 = Rev0, 0xFFFF = unknown
  HMI_REG_BOARD_STRAP_COUNTS = 67,                      // float, revision strap ADC counts
  HMI_REG_CELL_VOLTAGES_START = 0x100,
  HMI_REG_CELL_VOLTAGES_END = 0x1FF,
  HMI_REG_MODULE_TEMPS_START = 0x200,
  HMI_REG_MODULE_TEMPS_END = 0x207,
  HMI_REG_RAW_TEMPS_START = 0x208,
  HMI_REG_RAW_TEMPS_END = 0x238,
};

class HMIBMS : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_millis_sensor(sensor::Sensor *s) { millis_sensor_ = s; }
  void set_serial_text_sensor(text_sensor::TextSensor *s) { serial_text_sensor_ = s; }
  void set_board_revision_text_sensor(text_sensor::TextSensor *s) { board_revision_text_sensor_ = s; }
  void set_board_strap_counts_sensor(sensor::Sensor *s) { board_strap_counts_sensor_ = s; }
  void set_system_request_sensor(sensor::Sensor *s) { system_request_sensor_ = s; }
  void set_soc_sensor(sensor::Sensor *s) { soc_sensor_ = s; }
  void set_current_mA_sensor(sensor::Sensor *s) { current_mA_sensor_ = s; }
  void set_charge_raw_sensor(sensor::Sensor *s) { charge_raw_sensor_ = s; }
  void set_battery_voltage_mV_sensor(sensor::Sensor *s) { battery_voltage_mV_sensor_ = s; }
  void set_output_voltage_mV_sensor(sensor::Sensor *s) { output_voltage_mV_sensor_ = s; }
  void set_pos_contactor_voltage_mV_sensor(sensor::Sensor *s) { pos_contactor_voltage_mV_sensor_ = s; }
  void set_neg_contactor_voltage_mV_sensor(sensor::Sensor *s) { neg_contactor_voltage_mV_sensor_ = s; }
  void set_temperature_min_dC_sensor(sensor::Sensor *s) { temperature_min_dC_sensor_ = s; }
  void set_temperature_max_dC_sensor(sensor::Sensor *s) { temperature_max_dC_sensor_ = s; }
  void set_temperature_delta_sensor(sensor::Sensor *s) { temperature_delta_sensor_ = s; }
  void set_cell_voltage_min_mV_sensor(sensor::Sensor *s) { cell_voltage_min_mV_sensor_ = s; }
  void set_cell_voltage_max_mV_sensor(sensor::Sensor *s) { cell_voltage_max_mV_sensor_ = s; }
  void set_cell_voltage_delta_sensor(sensor::Sensor *s) { cell_voltage_delta_sensor_ = s; }
  void set_cell_count_sensor(sensor::Sensor *s) { cell_count_sensor_ = s; }
  void set_system_sm_sensor(sensor::Sensor *s) { system_sm_sensor_ = s; }
  void set_contactor_sm_sensor(sensor::Sensor *s) { contactor_sm_sensor_ = s; }
  void set_soc_voltage_based_sensor(sensor::Sensor *s) { soc_voltage_based_sensor_ = s; }
  void set_soc_basic_count_sensor(sensor::Sensor *s) { soc_basic_count_sensor_ = s; }
  void set_capacity_mC_sensor(sensor::Sensor *s) { capacity_mC_sensor_ = s; }
  void set_supply_voltage_3V3_mV_sensor(sensor::Sensor *s) { supply_voltage_3V3_mV_sensor_ = s; }
  void set_supply_voltage_5V_mV_sensor(sensor::Sensor *s) { supply_voltage_5V_mV_sensor_ = s; }
  void set_supply_voltage_12V_mV_sensor(sensor::Sensor *s) { supply_voltage_12V_mV_sensor_ = s; }
  void set_supply_voltage_contactor_mV_sensor(sensor::Sensor *s) { supply_voltage_contactor_mV_sensor_ = s; }
  void set_pack_pos_to_chassis_sensor(sensor::Sensor *s) { pack_pos_to_chassis_sensor_ = s; }
  void set_pack_neg_to_chassis_sensor(sensor::Sensor *s) { pack_neg_to_chassis_sensor_ = s; }
  void set_isolation_check_age_sensor(sensor::Sensor *s) { isolation_check_age_sensor_ = s; }
  void set_isolation_resistance_sensor(sensor::Sensor *s) { isolation_resistance_sensor_ = s; }
  void set_inverter_soc_sensor(sensor::Sensor *s) { inverter_soc_sensor_ = s; }
  void set_inverter_full_capacity_sensor(sensor::Sensor *s) { inverter_full_capacity_sensor_ = s; }
  void set_inverter_remaining_capacity_sensor(sensor::Sensor *s) { inverter_remaining_capacity_sensor_ = s; }
  void set_inverter_min_voltage_limit_sensor(sensor::Sensor *s) { inverter_min_voltage_limit_sensor_ = s; }
  void set_inverter_max_voltage_limit_sensor(sensor::Sensor *s) { inverter_max_voltage_limit_sensor_ = s; }
  void set_cell_voltage_working_min_number(HMIBMSNumber *n) { cell_voltage_working_min_number_ = n; }
  void set_cell_voltage_working_max_number(HMIBMSNumber *n) { cell_voltage_working_max_number_ = n; }
  void set_soc_number(HMIBMSNumber *n) { soc_number_ = n; }
  void set_soc_scaling_min_number(HMIBMSNumber *n) { soc_scaling_min_number_ = n; }
  void set_soc_scaling_max_number(HMIBMSNumber *n) { soc_scaling_max_number_ = n; }
  void set_voltage_limit_offset_lower_number(HMIBMSNumber *n) { voltage_limit_offset_lower_number_ = n; }
  void set_voltage_limit_offset_upper_number(HMIBMSNumber *n) { voltage_limit_offset_upper_number_ = n; }
  void set_cell_voltage_soft_min_number(HMIBMSNumber *n) { cell_voltage_soft_min_number_ = n; }
  void set_cell_voltage_soft_max_number(HMIBMSNumber *n) { cell_voltage_soft_max_number_ = n; }
  void set_auto_balancing_period_ms_number(HMIBMSNumber *n) { auto_balancing_period_ms_number_ = n; }
  void set_balancing_periods_per_mV_number(HMIBMSNumber *n) { balancing_periods_per_mV_number_ = n; }
  void set_balance_min_offset_mV_number(HMIBMSNumber *n) { balance_min_offset_mV_number_ = n; }
  void set_minimum_balancing_voltage_mV_number(HMIBMSNumber *n) { minimum_balancing_voltage_mV_number_ = n; }
  void set_balancing_start_voltage_mV_number(HMIBMSNumber *n) { balancing_start_voltage_mV_number_ = n; }
  void set_max_charge_current_number(HMIBMSNumber *n) { max_charge_current_number_ = n; }
  void set_max_discharge_current_number(HMIBMSNumber *n) { max_discharge_current_number_ = n; }
  void set_charge_current_limit_sensor(sensor::Sensor *s) { charge_current_limit_sensor_ = s; }
  void set_discharge_current_limit_sensor(sensor::Sensor *s) { discharge_current_limit_sensor_ = s; }
  void set_temp_charge_current_limit_sensor(sensor::Sensor *s) { temp_charge_current_limit_sensor_ = s; }
  void set_temp_discharge_current_limit_sensor(sensor::Sensor *s) { temp_discharge_current_limit_sensor_ = s; }
  void set_cell_voltage_charge_current_limit_sensor(sensor::Sensor *s) { cell_voltage_charge_current_limit_sensor_ = s; }
  void set_cell_voltage_discharge_current_limit_sensor(sensor::Sensor *s) { cell_voltage_discharge_current_limit_sensor_ = s; }
  void set_working_charge_current_limit_sensor(sensor::Sensor *s) { working_charge_current_limit_sensor_ = s; }
  void set_working_discharge_current_limit_sensor(sensor::Sensor *s) { working_discharge_current_limit_sensor_ = s; }
  void set_delta_charge_current_limit_sensor(sensor::Sensor *s) { delta_charge_current_limit_sensor_ = s; }
  void set_hard_charge_current_limit_sensor(sensor::Sensor *s) { hard_charge_current_limit_sensor_ = s; }
  void set_hard_discharge_current_limit_sensor(sensor::Sensor *s) { hard_discharge_current_limit_sensor_ = s; }

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
  void add_raw_temperature_sensor(sensor::Sensor *s) { raw_temperature_sensors_.push_back(s); }
  const std::vector<sensor::Sensor *> &get_cell_voltage_sensors() const { return cell_voltage_sensors_; }
  const std::vector<sensor::Sensor *> &get_balancing_time_sensors() const { return balancing_time_sensors_; }
  void set_bypass_crc(bool bypass) { bypass_crc_ = bypass; }
  void set_baud_rate(uint32_t baud) { baud_rate_ = baud; }
  void set_dump_raw(bool dump) { dump_raw_ = dump; }
  void set_address(uint8_t address) { address_ = address; }
  void set_log_sync_prefix(const std::string &prefix) { log_sync_prefix_ = prefix; }
  void set_log_enable_switch(HMIBMSLogSwitch *s) { log_enable_switch_ = s; }
  bool is_log_enabled() const { return log_enabled_; }
  void set_log_enabled(bool enabled) { log_enabled_ = enabled; }
  uint32_t get_log_update_interval() const { return log_update_interval_ms_; }
  void set_log_update_interval(uint32_t ms) { log_update_interval_ms_ = ms; }
  void set_settings_unlocked(bool unlocked);
  bool is_settings_unlocked() const { return settings_unlocked_; }
  void set_settings_status_text_sensor(text_sensor::TextSensor *s) { settings_status_text_sensor_ = s; }
  void publish_settings_status(const char *status);
  void publish_settings_status_temporary(const char *status, uint32_t duration_ms);
  void send_system_request(uint8_t request);
  void write_register(uint16_t reg_id, uint8_t type, float value, float scale);

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
  void handle_read_cell_voltages_response_(const uint8_t *data, size_t length);
  void handle_read_balancing_times_response_(const uint8_t *data, size_t length);
  void handle_read_log_response_(const uint8_t *data, size_t length);
  void handle_read_events_response_(const uint8_t *data, size_t length);
  
  void send_packet_(const std::vector<uint8_t> &payload);
  
  uint16_t crc16_modbus_(const uint8_t *data, size_t len);

  uint8_t address_{0x00};
  uint64_t serial_{0};
  uint32_t update_count_{0};
  uint8_t temp_poll_index_{0};
  std::vector<uint8_t> rx_buffer_;
  
  sensor::Sensor *millis_sensor_{nullptr};
  text_sensor::TextSensor *serial_text_sensor_{nullptr};
  text_sensor::TextSensor *board_revision_text_sensor_{nullptr};
  sensor::Sensor *board_strap_counts_sensor_{nullptr};
  sensor::Sensor *system_request_sensor_{nullptr};
  sensor::Sensor *soc_sensor_{nullptr};
  sensor::Sensor *current_mA_sensor_{nullptr};
  sensor::Sensor *charge_raw_sensor_{nullptr};
  sensor::Sensor *battery_voltage_mV_sensor_{nullptr};
  sensor::Sensor *output_voltage_mV_sensor_{nullptr};
  sensor::Sensor *pos_contactor_voltage_mV_sensor_{nullptr};
  sensor::Sensor *neg_contactor_voltage_mV_sensor_{nullptr};
  sensor::Sensor *temperature_min_dC_sensor_{nullptr};
  sensor::Sensor *temperature_max_dC_sensor_{nullptr};
  sensor::Sensor *temperature_delta_sensor_{nullptr};
  sensor::Sensor *cell_voltage_min_mV_sensor_{nullptr};
  sensor::Sensor *cell_voltage_max_mV_sensor_{nullptr};
  sensor::Sensor *cell_voltage_delta_sensor_{nullptr};
  sensor::Sensor *cell_count_sensor_{nullptr};
  sensor::Sensor *system_sm_sensor_{nullptr};
  sensor::Sensor *contactor_sm_sensor_{nullptr};
  sensor::Sensor *soc_voltage_based_sensor_{nullptr};
  sensor::Sensor *soc_basic_count_sensor_{nullptr};
  sensor::Sensor *capacity_mC_sensor_{nullptr};
  sensor::Sensor *supply_voltage_3V3_mV_sensor_{nullptr};
  sensor::Sensor *supply_voltage_5V_mV_sensor_{nullptr};
  sensor::Sensor *supply_voltage_12V_mV_sensor_{nullptr};
  sensor::Sensor *supply_voltage_contactor_mV_sensor_{nullptr};
  sensor::Sensor *pack_pos_to_chassis_sensor_{nullptr};
  sensor::Sensor *pack_neg_to_chassis_sensor_{nullptr};
  sensor::Sensor *isolation_check_age_sensor_{nullptr};
  sensor::Sensor *isolation_resistance_sensor_{nullptr};
  sensor::Sensor *inverter_soc_sensor_{nullptr};
  sensor::Sensor *inverter_full_capacity_sensor_{nullptr};
  sensor::Sensor *inverter_remaining_capacity_sensor_{nullptr};
  sensor::Sensor *inverter_min_voltage_limit_sensor_{nullptr};
  sensor::Sensor *inverter_max_voltage_limit_sensor_{nullptr};
  HMIBMSNumber *cell_voltage_working_min_number_{nullptr};
  HMIBMSNumber *cell_voltage_working_max_number_{nullptr};
  HMIBMSNumber *soc_number_{nullptr};
  HMIBMSNumber *soc_scaling_min_number_{nullptr};
  HMIBMSNumber *soc_scaling_max_number_{nullptr};
  HMIBMSNumber *voltage_limit_offset_lower_number_{nullptr};
  HMIBMSNumber *voltage_limit_offset_upper_number_{nullptr};
  HMIBMSNumber *cell_voltage_soft_min_number_{nullptr};
  HMIBMSNumber *cell_voltage_soft_max_number_{nullptr};
  HMIBMSNumber *auto_balancing_period_ms_number_{nullptr};
  HMIBMSNumber *balancing_periods_per_mV_number_{nullptr};
  HMIBMSNumber *balance_min_offset_mV_number_{nullptr};
  HMIBMSNumber *minimum_balancing_voltage_mV_number_{nullptr};
  HMIBMSNumber *balancing_start_voltage_mV_number_{nullptr};
  HMIBMSNumber *max_charge_current_number_{nullptr};
  HMIBMSNumber *max_discharge_current_number_{nullptr};
  sensor::Sensor *charge_current_limit_sensor_{nullptr};
  sensor::Sensor *discharge_current_limit_sensor_{nullptr};
  sensor::Sensor *temp_charge_current_limit_sensor_{nullptr};
  sensor::Sensor *temp_discharge_current_limit_sensor_{nullptr};
  sensor::Sensor *cell_voltage_charge_current_limit_sensor_{nullptr};
  sensor::Sensor *cell_voltage_discharge_current_limit_sensor_{nullptr};
  sensor::Sensor *working_charge_current_limit_sensor_{nullptr};
  sensor::Sensor *working_discharge_current_limit_sensor_{nullptr};
  sensor::Sensor *delta_charge_current_limit_sensor_{nullptr};
  sensor::Sensor *hard_charge_current_limit_sensor_{nullptr};
  sensor::Sensor *hard_discharge_current_limit_sensor_{nullptr};
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
  std::vector<sensor::Sensor *> raw_temperature_sensors_;
  bool bypass_crc_{false};
  uint32_t baud_rate_{460800};
  bool dump_raw_{false};
  bool settings_unlocked_{false};
  uint32_t last_settings_write_ms_{0};
  static const uint32_t SETTINGS_UNLOCK_TIMEOUT_MS = 30000;  // 30 seconds
  text_sensor::TextSensor *settings_status_text_sensor_{nullptr};
  uint32_t last_settings_countdown_s_{0xFFFFFFFFUL};
  bool settings_status_restore_pending_{false};
  uint32_t settings_status_restore_at_{0};
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

  struct SensorUpdate {
    sensor::Sensor *sensor;
    float state;
  };
  std::vector<SensorUpdate> publish_queue_;
};

}  // namespace hmi_bms
}  // namespace esphome
