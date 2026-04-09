#include "bedside_status.h"
#include "EPD.h"
#include "EPD_Init.h"

#include "esphome/core/gpio.h"
#include "esphome/core/log.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace esphome {
namespace bedside_status {

static const char *const TAG = "bedside_status";

static const int STATUS_LINE_COUNT = 6;
static const int LINE_CHAR_LIMIT = 32;
static const int LINE_CHAR_HEAD = 29;
static const char *BLANK_PLACEHOLDER = "--------";

int BedsideStatus::gpio_num_(GPIOPin *pin) { return static_cast<InternalGPIOPin *>(pin)->get_pin(); }

void BedsideStatus::apply_elecrow_pin_numbers_() {
  elecrow579_pin_sck = gpio_num_(this->pin_sck_);
  elecrow579_pin_mosi = gpio_num_(this->pin_mosi_);
  elecrow579_pin_res = gpio_num_(this->pin_rst_);
  elecrow579_pin_dc = gpio_num_(this->pin_dc_);
  elecrow579_pin_cs = gpio_num_(this->pin_cs_);
  elecrow579_pin_busy = gpio_num_(this->pin_busy_);
}

void BedsideStatus::epd_init_sequence_() {
  EPD_GPIOInit();
  Paint_NewImage(this->image_bw_, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();
  EPD_HW_RESET();
}

void BedsideStatus::clip_line_(const std::string &in, char *out, size_t out_sz) {
  if (out_sz == 0)
    return;
  out[0] = '\0';
  if (in.empty())
    return;
  if (in.size() <= static_cast<size_t>(LINE_CHAR_LIMIT)) {
    strncpy(out, in.c_str(), out_sz - 1);
    out[out_sz - 1] = '\0';
    return;
  }
  size_t n = std::min(static_cast<size_t>(LINE_CHAR_HEAD), out_sz - 4);
  memcpy(out, in.c_str(), n);
  out[n] = '\0';
  strncat(out, "...", out_sz - 1);
}

std::string BedsideStatus::format_time_ampm_() {
  uint8_t h = 0, m = 0, s = 0;
  if (this->time_src_ != nullptr) {
    auto t = this->time_src_->now();
    h = t.hour;
    m = t.minute;
    s = t.second;
  } else {
    time_t now = ::time(nullptr);
    struct tm lt {};
    localtime_r(&now, &lt);
    h = lt.tm_hour;
    m = lt.tm_min;
    s = lt.tm_sec;
  }
  bool pm = h >= 12;
  uint8_t h12 = h % 12;
  if (h12 == 0)
    h12 = 12;
  char buf[20];
  snprintf(buf, sizeof(buf), "%u:%02u:%02u %s", h12, m, s, pm ? "PM" : "AM");
  return std::string(buf);
}

void BedsideStatus::status_lines_from_json_(const std::string &json, bool *ok) {
  *ok = false;
  DynamicJsonDocument doc(12288);
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    ESP_LOGW(TAG, "JSON parse error: %s", err.c_str());
    this->lines_[STATUS_LINE_COUNT - 1] = std::string("JSON: ") + err.c_str();
    for (int i = 0; i < STATUS_LINE_COUNT - 1; i++)
      this->lines_[i].clear();
    return;
  }
  if (!doc.is<JsonArray>()) {
    this->lines_[STATUS_LINE_COUNT - 1] = "expected JSON array";
    for (int i = 0; i < STATUS_LINE_COUNT - 1; i++)
      this->lines_[i].clear();
    return;
  }
  JsonArray arr = doc.as<JsonArray>();
  std::vector<std::string> issues;
  for (size_t i = 0; i < arr.size(); i++) {
    JsonVariant v = arr[i];
    if (!v.is<JsonObject>())
      continue;
    JsonObject o = v.as<JsonObject>();
    if (!o.containsKey("DisplayStatus"))
      continue;
    int st = o["DisplayStatus"] | -1;
    if (st != this->display_status_filter_)
      continue;
    const char *name = o["DisplayName"] | "";
    const char *val = o["DisplayValue"] | "";
    char line[96];
    snprintf(line, sizeof(line), "%s - %s", name, val);
    issues.emplace_back(line);
  }

  for (auto &l : this->lines_)
    l.clear();

  size_t er = issues.size();
  if (er == 0) {
    this->lines_[0] = "ALL GOOD";
  } else if (er <= static_cast<size_t>(STATUS_LINE_COUNT)) {
    for (size_t i = 0; i < er; i++)
      this->lines_[i] = issues[i];
  } else {
    for (size_t i = 0; i < static_cast<size_t>(STATUS_LINE_COUNT - 1); i++)
      this->lines_[i] = issues[i];
    char more[48];
    snprintf(more, sizeof(more), "%u more issues", static_cast<unsigned>(er - (STATUS_LINE_COUNT - 1)));
    this->lines_[STATUS_LINE_COUNT - 1] = more;
  }
  *ok = true;
}

void BedsideStatus::fetch_status_http_() {
  if (this->status_url_.empty() || WiFi.status() != WL_CONNECTED) {
    return;
  }

  HTTPClient http;
  std::unique_ptr<WiFiClientSecure> tls(new WiFiClientSecure());
  WiFiClient *stream = nullptr;

  if (this->status_url_.find("https://") == 0) {
    if (!this->verify_ssl_) {
      tls->setInsecure();
    }
    stream = tls.get();
  }

  bool ok_http = false;
  if (stream != nullptr) {
    ok_http = http.begin(*stream, String(this->status_url_.c_str()));
  } else {
    ok_http = http.begin(this->status_url_.c_str());
  }

  if (!ok_http) {
    this->lines_[STATUS_LINE_COUNT - 1] = "HTTP begin failed";
    this->last_fetch_ok_ = false;
    return;
  }

  http.setTimeout(15000);
  http.addHeader("Accept", "application/json");
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    char err[48];
    snprintf(err, sizeof(err), "HTTP %d", code);
    this->lines_[STATUS_LINE_COUNT - 1] = err;
    this->last_fetch_ok_ = false;
    http.end();
    return;
  }

  String body = http.getString();
  http.end();

  bool parsed_ok = false;
  this->status_lines_from_json_(body.c_str(), &parsed_ok);
  this->last_fetch_ok_ = parsed_ok;
}

void BedsideStatus::draw_status_screen_() {
  Paint_Clear(WHITE);

  static const uint16_t y_positions[6] = {10, 50, 90, 130, 170, 210};
  char clipped[40];

  for (int i = 0; i < STATUS_LINE_COUNT; i++) {
    std::string raw = this->lines_[i];
    if (i >= 1 && i <= 5) {
      bool blank = true;
      for (char c : raw) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
          blank = false;
          break;
        }
      }
      if (blank)
        raw = BLANK_PLACEHOLDER;
    }
    clip_line_(raw, clipped, sizeof(clipped));
    if (clipped[0] != '\0')
      EPD_ShowString(8, y_positions[i], clipped, this->text_size_, BLACK);
  }

  std::string ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : std::string("---");
  clip_line_(ip, clipped, sizeof(clipped));
  std::string clk = format_time_ampm_();
  char time_buf[24];
  clip_line_(clk, time_buf, sizeof(time_buf));

  const uint16_t fy = 232;
  const uint16_t fs = this->footer_text_size_;
  const uint16_t cw = fs / 2;

  if (this->footer_ip_left_) {
    EPD_ShowString(8, fy, clipped, fs, BLACK);
    uint16_t tw = static_cast<uint16_t>(strlen(time_buf)) * cw;
    uint16_t tx = static_cast<uint16_t>(EPD_W - 8 - tw);
    if (tx < 8)
      tx = 8;
    EPD_ShowString(tx, fy, time_buf, fs, BLACK);
  } else {
    EPD_ShowString(8, fy, time_buf, fs, BLACK);
    uint16_t iw = static_cast<uint16_t>(strlen(clipped)) * cw;
    uint16_t ix = static_cast<uint16_t>(EPD_W - 8 - iw);
    if (ix < 8)
      ix = 8;
    EPD_ShowString(ix, fy, clipped, fs, BLACK);
  }

  EPD_Display(this->image_bw_);
  EPD_PartUpdate();
}

void BedsideStatus::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MyHouse bedside status (Elecrow 5.79\" EPD library)");

  if (this->pin_screen_power_ != nullptr) {
    this->pin_screen_power_->setup();
    this->pin_screen_power_->digital_write(true);
    delay(50);
  }

  this->pin_sck_->setup();
  this->pin_mosi_->setup();
  this->pin_rst_->setup();
  this->pin_dc_->setup();
  this->pin_cs_->setup();
  this->pin_busy_->setup();

  this->pin_sck_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_mosi_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_rst_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_dc_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_cs_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_busy_->pin_mode(gpio::FLAG_INPUT);

  this->apply_elecrow_pin_numbers_();
  this->epd_init_sequence_();

  for (auto &l : this->lines_)
    l.clear();
  this->lines_[0] = "ESPHome";
  this->lines_[1] = "Bedside status";
  this->lines_[2] = "Waiting for data";
  this->draw_status_screen_();

  ESP_LOGI(TAG, "Elecrow EPD initialized");
}

void BedsideStatus::loop() {
  uint32_t now = millis();
  uint32_t interval = this->last_fetch_ok_ ? 500u : 5000u;
  if (now - this->last_partial_ms_ < interval)
    return;
  this->last_partial_ms_ = now;
  this->draw_status_screen_();
}

void BedsideStatus::update() { this->fetch_status_http_(); }

void BedsideStatus::dump_config() {
  ESP_LOGCONFIG(TAG, "MyHouse bedside status");
  LOG_PIN("  SCK: ", this->pin_sck_);
  LOG_PIN("  MOSI: ", this->pin_mosi_);
  LOG_PIN("  RST: ", this->pin_rst_);
  LOG_PIN("  DC: ", this->pin_dc_);
  LOG_PIN("  CS: ", this->pin_cs_);
  LOG_PIN("  BUSY: ", this->pin_busy_);
  if (this->pin_screen_power_ != nullptr) {
    LOG_PIN("  Screen power: ", this->pin_screen_power_);
  }
  ESP_LOGCONFIG(TAG, "  Status URL: %s", this->status_url_.c_str());
  ESP_LOGCONFIG(TAG, "  Verify SSL: %s", this->verify_ssl_ ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  DisplayStatus filter: %d", this->display_status_filter_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace bedside_status
}  // namespace esphome
