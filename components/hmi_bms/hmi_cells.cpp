// The BMS's arrays: cell voltages, balancing times, module temperatures and module IDs,
// each read a page at a time (READ_ARRAY)
#include "hmi_bms.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <algorithm>
#include <cctype>

namespace esphome {
namespace hmi_bms {

static const char *const TAG = "hmi_bms";

// One array is read at a time. A read that stalls (a lost page) is dropped after a second.
void HMIBMS::request_array_(uint8_t array_id) {
  auto &rd = this->array_read_;
  if (rd.active && millis() - rd.started_ms < 1000) {
    if (rd.id != array_id &&
        std::find(this->array_queue_.begin(), this->array_queue_.end(), array_id) == this->array_queue_.end()) {
      this->array_queue_.push_back(array_id);
    }
    return;
  }
  rd.active = true;
  rd.id = array_id;
  rd.pages = 0;
  rd.started_ms = millis();
  rd.values.clear();
  this->send_array_request_(array_id, 0);
}

void HMIBMS::send_array_request_(uint8_t array_id, uint16_t start) {
  this->send_packet_(protocol::read_array_request(this->address_, array_id, start));
}

void HMIBMS::handle_array_page_(const uint8_t *data, size_t length) {
  static const uint8_t MAX_ARRAY_PAGES = 16;
  auto &rd = this->array_read_;
  protocol::ArrayPage page;
  if (!protocol::parse_array_page(data, length, page)) {
    ESP_LOGW(TAG, "Array reply not understood (%zu bytes)", length);
    return;
  }
  // Only the next page of the array being read is wanted
  if (!rd.active || page.array_id != rd.id || page.start != rd.values.size()) return;
  for (size_t i = 0; i < page.count; i++) {
    double v;
    if (protocol::array_element(page, i, v)) rd.values.push_back(v);
  }
  rd.pages++;
  if (page.count > 0 && rd.values.size() < page.total && rd.pages < MAX_ARRAY_PAGES) {
    this->send_array_request_(rd.id, (uint16_t) rd.values.size());
    return;
  }
  // No elements at all means the BMS has no reading yet (or doesn't have the array)
  this->finish_array_read_(!rd.values.empty() && rd.values.size() >= page.total);
}

// Ends the current read, publishing it if it is complete, and starts the next one waiting
void HMIBMS::finish_array_read_(bool complete) {
  auto &rd = this->array_read_;
  if (!rd.active) return;
  rd.active = false;
  if (complete) {
    switch (rd.id) {
      case HMI_ARRAY_CELL_VOLTAGES: this->publish_cell_voltages_(rd.values); break;
      case HMI_ARRAY_BALANCING_TIMES: this->publish_balancing_times_(rd.values); break;
      case HMI_ARRAY_MODULE_TEMPS: this->publish_module_temperatures_(rd.values); break;
      case HMI_ARRAY_MODULE_IDS: this->publish_module_ids_(rd.values); break;
      default: break;
    }
  }
  if (!this->array_queue_.empty()) {
    const uint8_t next = this->array_queue_.front();
    this->array_queue_.erase(this->array_queue_.begin());
    this->request_array_(next);
  }
}

void HMIBMS::publish_module_temperatures_(const std::vector<double> &values) {
  for (size_t i = 0; i < values.size() && i < this->module_temperature_sensors_.size(); i++) {
    if (this->module_temperature_sensors_[i] != nullptr) {
      this->publish_queue_.push_back({this->module_temperature_sensors_[i], (float) values[i]});
    }
  }
}

// "023 023": each BMB's module ID in chain order (as the BMS's pack type table has them),
// "?" for one whose last read failed its checks; "none" for an empty list.
void HMIBMS::publish_module_ids_(const std::vector<double> &values) {
  if (this->module_ids_text_sensor_ == nullptr) return;
  std::string text;
  for (double v : values) {
    const uint16_t id = (uint16_t) v;
    if (!text.empty()) text += " ";
    text += id == 0xFFFF ? std::string("?") : str_sprintf("%03X", id & 0x07FF);
  }
  if (text.empty()) text = "none";
  if (!this->module_ids_text_sensor_->has_state() || this->module_ids_text_sensor_->state != text) {
    this->module_ids_text_sensor_->publish_state(text);
  }
}

// Cell voltages in mV, one per cell slot; -1 = no reading
void HMIBMS::publish_cell_voltages_(const std::vector<double> &values) {
  uint16_t cells_found = 0;
  uint16_t nonzero_cells = 0;
  for (size_t i = 0; i < values.size(); i++) {
    const int32_t cell_voltage = (int32_t) values[i];
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
    ESP_LOGD(TAG, "Cell count nonzero=%u (slots=%zu valid=%u)", nonzero_cells, values.size(), cells_found);
  }
  ESP_LOGD(TAG, "Read %u cell voltages (queued)", cells_found);
}

// Balancing time remaining per cell slot
void HMIBMS::publish_balancing_times_(const std::vector<double> &times) {
  const size_t num_cells = times.size();
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
    const int32_t time_val = times[i] < 0 ? 0 : (int32_t) times[i];

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
