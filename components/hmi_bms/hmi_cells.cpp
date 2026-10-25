// Cell voltages and balancing times (delta-coded arrays of up to 120 cells)
#include "hmi_bms.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cctype>

namespace esphome {
namespace hmi_bms {

static const char *const TAG = "hmi_bms";

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

}  // namespace hmi_bms
}  // namespace esphome
