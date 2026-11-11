// BMS events: the event type names and reading the event list (several pages if needed)
#include "hmi_bms.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cctype>

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

// The names are in hmi_map.h, in the firmware's order
const char *HMIBMS::event_type_name(int type) {
  if (type < 0 || type >= HMI_EVENT_COUNT) return "UNKNOWN";
  return HMI_EVENT_NAMES[type];
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
    // Override: Inverter Detected is always Info level
    const uint16_t level = (type == HMI_EVENT_INVERTER_DETECTED) ? 1 : rec.level;
    const char *type_str = event_type_name(type);
    const char *level_str = (level < sizeof(EVENT_LEVELS) / sizeof(EVENT_LEVELS[0])) ? EVENT_LEVELS[level] : "??";

    // "WARNING (7)", and the data of its latest occurrence as hex when there is any:
    // "WARNING (7) d=FA0FFFF2D4C" (the BMS's events.h says how each event's data is laid out)
    char state_buf[48];
    if (rec.data != 0) {
      snprintf(state_buf, sizeof(state_buf), "%s (%u) d=%llX", level_str, rec.count, (unsigned long long) rec.data);
    } else {
      snprintf(state_buf, sizeof(state_buf), "%s (%u)", level_str, rec.count);
    }
    if (type < this->error_sensors_.size() && this->error_sensors_[type] != nullptr) {
      auto *error_sensor = this->error_sensors_[type];
      if (!error_sensor->has_state() || error_sensor->state != state_buf) {
        error_sensor->publish_state(state_buf);
      }
      if (type < sw.seen.size()) sw.seen[type] = true;
    }

    // The latest event is the one with the newest timestamp (the BMS lists them by type, not
    // by time); of several raised at the same moment, the most severe
    if (sw.latest_type < 0 || rec.timestamp > sw.latest_timestamp ||
        (rec.timestamp == sw.latest_timestamp && level > sw.latest_level)) {
      sw.latest_type = type;
      sw.latest_level = level;
      sw.latest_count = rec.count;
      sw.latest_timestamp = rec.timestamp;
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

}  // namespace hmi_bms
}  // namespace esphome
