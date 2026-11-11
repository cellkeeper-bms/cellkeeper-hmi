// The screenshot request reads the panel's own frame buffer, so that it shows what is on the
// glass and not what this component believes it sent. The display class keeps its panel handle
// protected and is final, so for this bench-only request its header is read with "protected"
// opened up; that changes access only, not the layout of anything.
#define protected public
#include "esphome/components/mipi_dsi/mipi_dsi.h"
#undef protected

#include "hmi_ui.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_http_server.h>
#include <esp_lcd_mipi_dsi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome {
namespace hmi_ui {

static const char *const TAG = "hmi_ui";

using hmiui::SCREEN_H;
using hmiui::SCREEN_W;

// With a finger on the panel a frame is drawn this often; otherwise a few times a second
static const uint32_t TOUCH_FRAME_MS = 30, QUIET_FRAME_MS = 250, AFTER_TOUCH_MS = 400;

void HmiUi::setup() {
  RAMAllocator<uint16_t> allocator(RAMAllocator<uint16_t>::ALLOC_EXTERNAL);
  this->frame_ = allocator.allocate((size_t) SCREEN_W * SCREEN_H);
  if (this->frame_ == nullptr) {
    ESP_LOGE(TAG, "No memory for the frame");
    this->mark_failed();
    return;
  }
  memset(this->frame_, 0, (size_t) SCREEN_W * SCREEN_H * sizeof(uint16_t));
  this->touchscreen_->register_listener(this);
  if (this->screenshot_) {
    this->base_->init();
    this->base_->add_handler(this);
  }
  this->started_ms_ = millis();
}

void HmiUi::dump_config() {
  ESP_LOGCONFIG(TAG,
                "LCD UI:\n"
                "  Frame: %dx%d in PSRAM\n"
                "  Last frame drawn: draw %u us, send %u us\n"
                "  Screenshot request: %s",
                SCREEN_W, SCREEN_H, (unsigned) this->draw_us_, (unsigned) this->push_us_, YESNO(this->screenshot_));
}

// ---- touch ----

void HmiUi::touch(touchscreen::TouchPoint tp) {
  this->last_touch_ms_ = millis();
  if (this->dimmed_) {
    // The first touch on a dimmed screen only wakes it
    this->dimmed_ = false;
    this->waking_ = true;
    this->ui_.poke(this->last_touch_ms_);
    this->wake_trigger_.trigger();
    return;
  }
  if (this->waking_) return;
  this->input_.down = true;
  this->input_.x = tp.x;
  this->input_.y = tp.y;
  this->down_seen_ = false;
  this->release_waiting_ = false;
  this->touch_event_ = true;
}

void HmiUi::update(const touchscreen::TouchPoints_t &points) {
  if (points.empty() || this->waking_ || !this->input_.down) return;
  this->last_touch_ms_ = millis();
  if (this->input_.x != points[0].x || this->input_.y != points[0].y) {
    this->input_.x = points[0].x;
    this->input_.y = points[0].y;
    this->touch_event_ = true;
  }
}

void HmiUi::release() {
  this->last_touch_ms_ = millis();
  if (this->waking_) {
    this->waking_ = false;
    return;
  }
  if (!this->down_seen_) {
    // No frame has seen the finger down yet: lift it after the next one
    this->release_waiting_ = true;
  } else {
    this->input_.down = false;
  }
  this->touch_event_ = true;
}

// ---- frames ----

void HmiUi::loop() {
  if (this->frame_ == nullptr) return;
  const uint32_t now = millis();
  const int page = this->pending_page_;
  if (page >= 0) {
    this->pending_page_ = -1;
    if (page < (int) hmiui::Page::COUNT) {
      this->ui_.state = hmiui::UiState();
      this->ui_.state.page = (hmiui::Page) page;
      this->ui_.poke(now);
    }
    this->touch_event_ = true;
  }
  if (this->paused_) return;
  const bool busy = this->input_.down || now - this->last_touch_ms_ < AFTER_TOUCH_MS;
  if (!this->touch_event_ && now - this->last_frame_ms_ < (busy ? TOUCH_FRAME_MS : QUIET_FRAME_MS)) return;
  this->touch_event_ = false;
  this->last_frame_ms_ = now;
  this->draw_frame_(now);

  if (this->input_.down) this->down_seen_ = true;
  if (this->release_waiting_) {
    this->release_waiting_ = false;
    this->input_.down = false;
    this->touch_event_ = true;
  }
  if (!this->dimmed_ && this->ui_.idle(now)) {
    this->dimmed_ = true;
    this->idle_trigger_.trigger();
  }
}

void HmiUi::draw_frame_(uint32_t now) {
  const uint32_t t0 = micros();
  this->model_ = hmiui::Model();
  if (this->fill_) this->fill_(this->model_);
  const uint32_t t1 = micros();
  hmiui::Canvas canvas(this->frame_, SCREEN_W, SCREEN_H);
  // While the panel may still be starting, every frame is drawn and sent whole
  const bool starting = now - this->started_ms_ < 3000;
  if (starting) this->ui_.redraw();
  const bool drew = this->ui_.frame(canvas, this->model_, this->input_, *this, now);
  const uint32_t t2 = micros();
  this->frames_looked_++;
  this->fill_us_ = t1 - t0;
  if (drew) {
    for (int i = 0; i < this->ui_.redrawn_count; i++) this->send_rows_(this->ui_.redrawn[i].y0, this->ui_.redrawn[i].y1);
    const uint32_t t3 = micros();
    this->draw_us_ = t2 - t1;
    this->push_us_ = t3 - t2;
    this->worst_us_ = std::max(this->worst_us_, t3 - t0);
    this->frames_drawn_++;
  } else {
    // Nothing on the screen would have changed: the dry run was all there was to do
    this->dry_us_ = t2 - t1;
  }
  if (now - this->last_report_ms_ >= 30000) {
    ESP_LOGD(TAG,
             "%u s: %u frames looked at, %u drawn, %u rows sent. Fill %u us, dry run %u us; last drawn: draw %u us, send "
             "%u us; slowest %u us",
             (unsigned) ((now - this->last_report_ms_) / 1000), (unsigned) this->frames_looked_, (unsigned) this->frames_drawn_,
             (unsigned) this->rows_sent_, (unsigned) this->fill_us_, (unsigned) this->dry_us_, (unsigned) this->draw_us_,
             (unsigned) this->push_us_, (unsigned) this->worst_us_);
    this->last_report_ms_ = now;
    this->frames_looked_ = 0;
    this->frames_drawn_ = 0;
    this->worst_us_ = 0;
    this->rows_sent_ = 0;
  }
}

void HmiUi::send_rows_(int y0, int y1) {
  // The frame is RGB565 in this chip's byte order, which is what the panel's buffer holds
  this->display_->draw_pixels_at(0, y0, SCREEN_W, y1 - y0, (const uint8_t *) (this->frame_ + (size_t) y0 * SCREEN_W),
                                 display::COLOR_ORDER_RGB, display::COLOR_BITNESS_565, false, 0, 0, 0);
  this->rows_sent_ += y1 - y0;
}

void HmiUi::show_message(const char *title, const char *text) {
  if (this->frame_ == nullptr) return;
  this->paused_ = true;
  hmiui::Canvas canvas(this->frame_, SCREEN_W, SCREEN_H);
  canvas.clear(this->ui_.theme.background);
  canvas.text(hmiui::FONT_M32, SCREEN_W / 2, SCREEN_H / 2 - 44, title, this->ui_.theme.text, hmiui::CENTER);
  canvas.text(hmiui::FONT_R20, SCREEN_W / 2, SCREEN_H / 2 + 8, text, this->ui_.theme.text_dim, hmiui::CENTER);
  this->send_rows_(0, SCREEN_H);
}

void HmiUi::resume() {
  this->paused_ = false;
  // The canvas no longer holds a page
  this->ui_.redraw();
  this->touch_event_ = true;
}

// ---- the bench's screenshot ----

bool HmiUi::canHandle(AsyncWebServerRequest *request) const {
  return request->method() == HTTP_GET && request->url() == "/screenshot.raw";
}

void HmiUi::handleRequest(AsyncWebServerRequest *request) {
  httpd_req_t *req = *request;
  if (request->hasParam("page")) {
    this->pending_page_ = atoi(request->arg("page").c_str());
    // Let the main loop show the page and send it to the display
    vTaskDelay(pdMS_TO_TICKS(1200));
  }
  void *fb = nullptr;
  esp_lcd_panel_handle_t panel = static_cast<mipi_dsi::MipiDsi *>(this->display_)->handle_;
  if (panel == nullptr || esp_lcd_dpi_panel_get_frame_buffer(panel, 1, &fb) != ESP_OK || fb == nullptr) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no frame buffer");
    return;
  }
  const size_t total = (size_t) SCREEN_W * SCREEN_H * 2;
  char format[48];
  snprintf(format, sizeof(format), "%dx%d rgb565le", SCREEN_W, SCREEN_H);
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

}  // namespace hmi_ui
}  // namespace esphome
