// The settings lock and the number entities that write BMS settings
#include "hmi_bms.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hmi_bms {

void SettingsLock::set_unlocked(bool unlocked) {
  this->unlocked_ = unlocked;
  this->last_countdown_s_ = UINT32_MAX;
  if (unlocked) {
    // Start/restart the inactivity window
    this->last_write_ms_ = millis();
    if (this->status_ != nullptr) {
      this->status_->publish_state("Settings Unlocked (30s)");
    }
    return;
  }
  this->publish_status("Settings locked");
}

void SettingsLock::publish_status(const char *status) {
  this->restore_pending_ = false;
  if (this->status_ != nullptr) {
    this->status_->publish_state(status);
  }
}

void SettingsLock::publish_status_temporary(const char *status, uint32_t duration_ms) {
  if (this->status_ != nullptr) {
    this->status_->publish_state(status);
  }
  this->restore_pending_ = true;
  this->restore_at_ms_ = millis() + duration_ms;
}

void SettingsLock::loop() {
  const uint32_t now = millis();
  if (this->unlocked_ && (int32_t) (now - this->last_write_ms_) >= (int32_t) UNLOCK_TIMEOUT_MS) {
    this->set_unlocked(false);
    this->publish_status_temporary("Settings auto-locked (30s idle)", 10000);
  }

  if (this->restore_pending_ && (int32_t) (now - this->restore_at_ms_) >= 0) {
    this->restore_pending_ = false;
    if (this->unlocked_) {
      this->last_countdown_s_ = UINT32_MAX;
    } else {
      this->publish_status("Settings locked");
    }
  }

  if (this->unlocked_ && !this->restore_pending_) {
    const uint32_t elapsed_ms = now - this->last_write_ms_;
    const uint32_t remaining_ms = (elapsed_ms >= UNLOCK_TIMEOUT_MS) ? 0 : (UNLOCK_TIMEOUT_MS - elapsed_ms);
    const uint32_t remaining_s = (remaining_ms + 999) / 1000;
    if (remaining_s != this->last_countdown_s_) {
      this->last_countdown_s_ = remaining_s;
      if (this->status_ != nullptr) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Settings Unlocked (%lus)", (unsigned long) remaining_s);
        this->status_->publish_state(buf);
      }
    }
  }
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

}  // namespace hmi_bms
}  // namespace esphome
