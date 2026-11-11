// A development aid: GET /screenshot.raw sends the LCD's frame buffer (see __init__.py)
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include "esphome/components/mipi_dsi/mipi_dsi.h"
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace lcd_screenshot {

class LcdScreenshot : public Component, public AsyncWebHandler {
 public:
  void set_web_server_base(web_server_base::WebServerBase *base) { this->base_ = base; }
  void set_display(mipi_dsi::MipiDsi *display) { this->display_ = display; }
  void set_lvgl(lvgl::LvglComponent *lvgl) { this->lvgl_ = lvgl; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }

  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;

 protected:
  web_server_base::WebServerBase *base_{nullptr};
  mipi_dsi::MipiDsi *display_{nullptr};
  lvgl::LvglComponent *lvgl_{nullptr};
  // A page asked for by a request (handled on the web server's task), shown from loop()
  volatile int pending_page_{-1};
};

}  // namespace lcd_screenshot
}  // namespace esphome
