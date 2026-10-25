// The BMS console log: reading it in chunks, splitting lines, and the 50 log line sensors
#include "hmi_bms.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cctype>

namespace esphome {
namespace hmi_bms {

static const char *const TAG = "hmi_bms";

void HMIBMSLogSwitch::write_state(bool state) {
  this->publish_state(state);
  if (this->parent_) {
    this->parent_->set_log_enabled(state);
  }
}

void HMIBMS::publish_log_() {
  // One log message every 50 ms, to pace updates for Home Assistant from the queue every 50ms to pace updates for Home Assistant
  if (this->log_enabled_ && this->log_text_sensor_ && !this->log_publish_queue_.empty()) {
    uint32_t now = millis();
    if (now - this->last_log_publish_time_ >= 50) {
      std::string line = this->log_publish_queue_.front();
      this->log_publish_queue_.pop_front();
      this->log_text_sensor_->publish_state(line);
      this->last_log_publish_time_ = now;
    }
  }

  // The buffered log line snapshot every 2 seconds, only changed lines
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

}  // namespace hmi_bms
}  // namespace esphome
