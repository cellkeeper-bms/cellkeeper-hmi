// The LCD UI on the device: the ESPHome side of components/hmi_ui. It asks the device config
// for the present values (the `fill` lambda), runs the plain C++ UI on them, sends the rows
// that changed to the display, hands the touch panel's state to the UI, and turns what the UI
// asks for into automations. See __init__.py for the configuration.
#pragma once

#include "esphome/components/display/display.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include "ui.h"

#include <functional>
#include <string>

namespace esphome {
namespace hmi_ui {

class HmiUi : public Component, public touchscreen::TouchListener, public hmiui::Actions, public AsyncWebHandler {
 public:
  void set_display(display::Display *display) { this->display_ = display; }
  void set_touchscreen(touchscreen::Touchscreen *touchscreen) { this->touchscreen_ = touchscreen; }
  void set_web_server_base(web_server_base::WebServerBase *base) { this->base_ = base; }
  void set_screenshot(bool on) { this->screenshot_ = on; }
  void set_pin(const std::string &pin) { this->pin_ = pin; }
  void set_fill(std::function<void(hmiui::Model &)> &&fill) { this->fill_ = std::move(fill); }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // Stops the pages and shows one message instead (a firmware update is starting); resume()
  // brings the pages back.
  void show_message(const char *title, const char *text);
  void resume();

  // How long the last frame that was drawn took to draw and to send, in microseconds
  uint32_t draw_us() const { return this->draw_us_; }
  uint32_t push_us() const { return this->push_us_; }

  Trigger<hmiui::Command> *get_command_trigger() { return &this->command_trigger_; }
  Trigger<int, float> *get_setting_trigger() { return &this->setting_trigger_; }
  Trigger<bool> *get_unlock_trigger() { return &this->unlock_trigger_; }
  Trigger<int> *get_brightness_trigger() { return &this->brightness_trigger_; }
  Trigger<bool> *get_log_trigger() { return &this->log_trigger_; }
  Trigger<std::string, std::string> *get_wifi_trigger() { return &this->wifi_trigger_; }
  Trigger<> *get_idle_trigger() { return &this->idle_trigger_; }
  Trigger<> *get_wake_trigger() { return &this->wake_trigger_; }

  // The touch panel (touchscreen::TouchListener)
  void touch(touchscreen::TouchPoint tp) override;
  void update(const touchscreen::TouchPoints_t &points) override;
  void release() override;

  // What the UI asks for (hmiui::Actions)
  bool pin_ok(const char *pin) override { return this->pin_ == pin; }
  void command(hmiui::Command command) override { this->command_trigger_.trigger(command); }
  void write_setting(int index, float value) override { this->setting_trigger_.trigger(index, value); }
  void set_settings_unlocked(bool unlocked) override { this->unlock_trigger_.trigger(unlocked); }
  void set_brightness(int percent) override { this->brightness_trigger_.trigger(percent); }
  void set_log_enabled(bool on) override { this->log_trigger_.trigger(on); }
  void wifi_join(const char *ssid, const char *password) override { this->wifi_trigger_.trigger(ssid, password); }

  // The bench's screenshot request (AsyncWebHandler)
  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;

 protected:
  void draw_frame_(uint32_t now);
  void send_rows_(int y0, int y1);

  display::Display *display_{nullptr};
  touchscreen::Touchscreen *touchscreen_{nullptr};
  web_server_base::WebServerBase *base_{nullptr};
  bool screenshot_{false};
  std::string pin_;
  std::function<void(hmiui::Model &)> fill_;

  hmiui::Ui ui_;
  hmiui::Model model_;
  uint16_t *frame_{nullptr};
  bool paused_{false};

  // The touch panel as the next frame will see it. A finger that came down and went up again
  // between two frames is still seen: down for one frame, then up.
  hmiui::Input input_;
  bool touch_event_{false}, down_seen_{true}, release_waiting_{false};
  bool dimmed_{false}, waking_{false};
  volatile int pending_page_{-1};

  uint32_t last_frame_ms_{0}, last_touch_ms_{0}, last_report_ms_{0}, started_ms_{0};
  uint32_t fill_us_{0}, dry_us_{0}, draw_us_{0}, push_us_{0}, worst_us_{0};
  uint32_t frames_looked_{0}, frames_drawn_{0}, rows_sent_{0};

  Trigger<hmiui::Command> command_trigger_;
  Trigger<int, float> setting_trigger_;
  Trigger<bool> unlock_trigger_;
  Trigger<int> brightness_trigger_;
  Trigger<bool> log_trigger_;
  Trigger<std::string, std::string> wifi_trigger_;
  Trigger<> idle_trigger_;
  Trigger<> wake_trigger_;
};

}  // namespace hmi_ui
}  // namespace esphome
