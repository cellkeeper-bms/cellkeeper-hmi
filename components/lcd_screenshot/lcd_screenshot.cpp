// The display component keeps its panel handle protected, and the class is final, so there is
// no clean way to reach the frame buffer. For this bench-only tool the header is read with
// "protected" opened up; it changes access only, not the layout of anything.
#define protected public
#include "esphome/components/mipi_dsi/mipi_dsi.h"
#undef protected

#include "lcd_screenshot.h"

#include "esphome/core/log.h"

#include <esp_http_server.h>
#include <esp_lcd_mipi_dsi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome {
namespace lcd_screenshot {

static const char *const TAG = "lcd_screenshot";

void LcdScreenshot::setup() {
  this->base_->init();
  this->base_->add_handler(this);
}

void LcdScreenshot::loop() {
  const int page = this->pending_page_;
  if (page < 0) return;
  this->pending_page_ = -1;
  this->lvgl_->show_page((size_t) page, LV_SCREEN_LOAD_ANIM_NONE, 0);
}

void LcdScreenshot::dump_config() { ESP_LOGCONFIG(TAG, "LCD screenshot: GET /screenshot.raw[?page=N]"); }

bool LcdScreenshot::canHandle(AsyncWebServerRequest *request) const {
  return request->method() == HTTP_GET && request->url() == "/screenshot.raw";
}

void LcdScreenshot::handleRequest(AsyncWebServerRequest *request) {
  httpd_req_t *req = *request;
  if (request->hasParam("page")) {
    this->pending_page_ = atoi(request->arg("page").c_str());
    // Let the main loop show the page, the UI fill it in and the display draw it
    vTaskDelay(pdMS_TO_TICKS(1500));
  }

  void *fb = nullptr;
  esp_lcd_panel_handle_t panel = this->display_->handle_;
  if (panel == nullptr || esp_lcd_dpi_panel_get_frame_buffer(panel, 1, &fb) != ESP_OK || fb == nullptr) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no frame buffer");
    return;
  }
  const int width = this->display_->get_width();
  const int height = this->display_->get_height();
  // 16 bits per pixel unless the display was set up for 24
  const size_t bytes_per_pixel = LV_COLOR_DEPTH == 16 ? 2 : 3;
  const size_t total = (size_t) width * height * bytes_per_pixel;

  char format[48];
  snprintf(format, sizeof(format), "%dx%d %s", width, height, bytes_per_pixel == 2 ? "rgb565le" : "rgb888");
  httpd_resp_set_type(req, "application/octet-stream");
  httpd_resp_set_hdr(req, "X-Screen-Format", format);
  const char *data = static_cast<const char *>(fb);
  static const size_t CHUNK = 16384;
  for (size_t sent = 0; sent < total; sent += CHUNK) {
    const size_t n = total - sent < CHUNK ? total - sent : CHUNK;
    if (httpd_resp_send_chunk(req, data + sent, n) != ESP_OK) {
      ESP_LOGW(TAG, "Screenshot aborted after %u bytes", (unsigned) sent);
      return;
    }
  }
  httpd_resp_send_chunk(req, nullptr, 0);
}

}  // namespace lcd_screenshot
}  // namespace esphome
