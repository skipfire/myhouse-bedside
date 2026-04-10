#include "bedside_status.h"
#include "EPD.h"
#include "EPD_Init.h"
#include "spi.h"

#include "esphome/core/gpio.h"
#include "esphome/core/log.h"

#include "cJSON.h"
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

namespace esphome {
namespace bedside_status {

static const char *const TAG = "bedside_status";

static const int STATUS_LINE_COUNT = 6;
static const int LINE_CHAR_LIMIT = 32;
static const int LINE_CHAR_HEAD = 29;
static const char *BLANK_PLACEHOLDER = "--------";

BedsideStatus *BedsideStatus::epd_busy_parent_{nullptr};

int BedsideStatus::epd_busy_read_trampoline_(void) {
  if (BedsideStatus::epd_busy_parent_ == nullptr || BedsideStatus::epd_busy_parent_->pin_busy_ == nullptr) {
    return 0;
  }
  return BedsideStatus::epd_busy_parent_->pin_busy_->digital_read() ? 1 : 0;
}

// Match bedside_render.py (FOOTER_BAND_HEIGHT, STATUS_TOP_MARGIN, six status rows).
static const uint16_t FOOTER_BAND_HEIGHT = 40;
static const uint16_t STATUS_TOP_MARGIN = 2;

static uint16_t elecrow_glyph_height(uint16_t sz) {
  switch (sz) {
    case 12:
      return 16;
    case 16:
      return 16;
    case 24:
      return 24;
    case 48:
      return 48;
    case 8:
      return 8;
    default:
      return 24;
  }
}

static uint16_t elecrow_char_advance_x(uint16_t sz) {
  if (sz == 24) {
    return 24;
  }
  return sz / 2;
}

static void epd_draw_string_sized(uint16_t x, uint16_t y, const char *s, uint16_t sz, uint16_t color) {
  if (sz == 24) {
    EPD_ShowString2412_DoubleWidth(x, y, s, color);
  } else {
    EPD_ShowString(x, y, s, sz, color);
  }
}

static inline void bedside_delay_ms(uint32_t ms) {
  if (ms == 0) {
    return;
  }
  vTaskDelay(pdMS_TO_TICKS(ms));
}

static inline uint32_t bedside_millis() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

static bool wifi_sta_has_ip() {
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif == nullptr) {
    return false;
  }
  esp_netif_ip_info_t ip{};
  if (esp_netif_get_ip_info(netif, &ip) != ESP_OK) {
    return false;
  }
  return ip.ip.addr != 0;
}

static std::string wifi_sta_ip_str() {
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif == nullptr) {
    return "---";
  }
  esp_netif_ip_info_t ip{};
  if (esp_netif_get_ip_info(netif, &ip) != ESP_OK) {
    return "---";
  }
  char buf[16];
  esp_ip4addr_ntoa(&ip.ip, buf, sizeof(buf));
  return std::string(buf);
}

static esp_err_t http_append_body_event(esp_http_client_event_t *evt) {
  if (evt->event_id != HTTP_EVENT_ON_DATA || evt->data_len <= 0) {
    return ESP_OK;
  }
  auto *body = static_cast<std::string *>(evt->user_data);
  if (body != nullptr) {
    body->append(static_cast<const char *>(evt->data), evt->data_len);
  }
  return ESP_OK;
}

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
  Paint_NewImage(this->image_bw_, EPD_W, EPD_H, Rotation, EPD_COLOR_WHITE);
  Paint_Clear(EPD_COLOR_WHITE);
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
  cJSON *root = cJSON_Parse(json.c_str());
  if (root == nullptr) {
    ESP_LOGW(TAG, "JSON parse error");
    for (int i = 0; i < STATUS_LINE_COUNT - 1; i++)
      this->lines_[i].clear();
    this->lines_[STATUS_LINE_COUNT - 1] = "JSON parse error";
    return;
  }
  if (!cJSON_IsArray(root)) {
    cJSON_Delete(root);
    for (int i = 0; i < STATUS_LINE_COUNT - 1; i++)
      this->lines_[i].clear();
    this->lines_[STATUS_LINE_COUNT - 1] = "expected JSON array";
    return;
  }

  std::vector<std::string> issues;
  cJSON *item = nullptr;
  cJSON_ArrayForEach(item, root) {
    if (!cJSON_IsObject(item)) {
      continue;
    }
    cJSON *st = cJSON_GetObjectItemCaseSensitive(item, "DisplayStatus");
    if (st == nullptr) {
      continue;
    }
    int st_val = -1;
    if (cJSON_IsNumber(st)) {
      st_val = static_cast<int>(cJSON_GetNumberValue(st));
    } else if (cJSON_IsString(st)) {
      const char *sv = cJSON_GetStringValue(st);
      if (sv != nullptr) {
        st_val = atoi(sv);
      }
    } else {
      continue;
    }
    if (st_val != this->display_status_filter_) {
      continue;
    }

    const char *name = "";
    const char *val = "";
    cJSON *jn = cJSON_GetObjectItemCaseSensitive(item, "DisplayName");
    if (cJSON_IsString(jn) && jn->valuestring != nullptr) {
      name = jn->valuestring;
    }
    cJSON *jv = cJSON_GetObjectItemCaseSensitive(item, "DisplayValue");
    if (cJSON_IsString(jv) && jv->valuestring != nullptr) {
      val = jv->valuestring;
    }

    char line[96];
    snprintf(line, sizeof(line), "%s - %s", name, val);
    issues.emplace_back(line);
  }

  cJSON_Delete(root);

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
  if (this->status_url_.empty() || !wifi_sta_has_ip()) {
    return;
  }

  std::string response_body;
  esp_http_client_config_t cfg{};
  cfg.url = this->status_url_.c_str();
  cfg.timeout_ms = 15000;
  cfg.method = HTTP_METHOD_GET;
  cfg.event_handler = http_append_body_event;
  cfg.user_data = &response_body;
  cfg.buffer_size = 4096;
  cfg.buffer_size_tx = 1024;

  const bool is_https =
      this->status_url_.size() >= 8 && this->status_url_.compare(0, 8, "https://") == 0;
  if (is_https) {
    if (this->verify_ssl_) {
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
      cfg.crt_bundle_attach = esp_crt_bundle_attach;
#endif
    } else {
      cfg.crt_bundle_attach = nullptr;
      cfg.skip_cert_common_name_check = true;
    }
  }

  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (client == nullptr) {
    this->lines_[STATUS_LINE_COUNT - 1] = "HTTP init failed";
    this->last_fetch_ok_ = false;
    return;
  }

  esp_http_client_set_header(client, "Accept", "application/json");
  esp_err_t err = esp_http_client_perform(client);
  int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  if (err != ESP_OK) {
    char ebuf[48];
    snprintf(ebuf, sizeof(ebuf), "HTTP err 0x%x", static_cast<unsigned>(err));
    this->lines_[STATUS_LINE_COUNT - 1] = ebuf;
    this->last_fetch_ok_ = false;
    return;
  }

  if (status != 200) {
    char err[48];
    snprintf(err, sizeof(err), "HTTP %d", status);
    this->lines_[STATUS_LINE_COUNT - 1] = err;
    this->last_fetch_ok_ = false;
    return;
  }

  bool parsed_ok = false;
  this->status_lines_from_json_(response_body, &parsed_ok);
  this->last_fetch_ok_ = parsed_ok;
}

void BedsideStatus::draw_status_screen_() {
  Paint_Clear(EPD_COLOR_WHITE);

  const uint16_t gh = elecrow_glyph_height(this->text_size_);
  const uint16_t footer_y0 = static_cast<uint16_t>(EPD_H - FOOTER_BAND_HEIGHT);
  const uint16_t avail =
      (footer_y0 > STATUS_TOP_MARGIN) ? static_cast<uint16_t>(footer_y0 - STATUS_TOP_MARGIN) : 0;
  uint16_t pitch = gh;
  if (avail > 0) {
    const uint16_t min_pitch = static_cast<uint16_t>(avail / STATUS_LINE_COUNT);
    if (min_pitch > pitch) {
      pitch = min_pitch;
    }
  }

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
    if (clipped[0] != '\0') {
      const uint16_t slot_top = static_cast<uint16_t>(STATUS_TOP_MARGIN + static_cast<uint16_t>(i) * pitch);
      uint16_t y_text = slot_top;
      if (pitch > gh) {
        y_text = static_cast<uint16_t>(y_text + (pitch - gh) / 2);
      }
      epd_draw_string_sized(8, y_text, clipped, this->text_size_, EPD_COLOR_BLACK);
    }
  }

  std::string ip = wifi_sta_has_ip() ? wifi_sta_ip_str() : std::string("---");
  clip_line_(ip, clipped, sizeof(clipped));
  std::string clk = format_time_ampm_();
  char time_buf[24];
  clip_line_(clk, time_buf, sizeof(time_buf));

  const uint16_t fs = this->footer_text_size_;
  const uint16_t fgh = elecrow_glyph_height(fs);
  const uint16_t cw = elecrow_char_advance_x(fs);
  const uint16_t room_below = static_cast<uint16_t>(EPD_H - footer_y0);
  uint16_t pad = 2;
  if (room_below > fgh) {
    const uint16_t p = static_cast<uint16_t>((room_below - fgh) / 2);
    pad = p > 2 ? p : 2;
  }
  const uint16_t fy = static_cast<uint16_t>(footer_y0 + pad);

  if (this->footer_ip_left_) {
    epd_draw_string_sized(8, fy, clipped, fs, EPD_COLOR_BLACK);
    uint16_t tw = static_cast<uint16_t>(strlen(time_buf)) * cw;
    uint16_t tx = static_cast<uint16_t>(EPD_W - 8 - tw);
    if (tx < 8)
      tx = 8;
    epd_draw_string_sized(tx, fy, time_buf, fs, EPD_COLOR_BLACK);
  } else {
    epd_draw_string_sized(8, fy, time_buf, fs, EPD_COLOR_BLACK);
    uint16_t iw = static_cast<uint16_t>(strlen(clipped)) * cw;
    uint16_t ix = static_cast<uint16_t>(EPD_W - 8 - iw);
    if (ix < 8)
      ix = 8;
    epd_draw_string_sized(ix, fy, clipped, fs, EPD_COLOR_BLACK);
  }

  const uint32_t now_ms = bedside_millis();
  bool use_full_waveform = false;
  if (this->epd_full_refresh_on_boot_ && !this->epd_boot_full_refresh_done_) {
    use_full_waveform = true;
    this->epd_boot_full_refresh_done_ = true;
    this->last_full_epd_refresh_ms_ = now_ms;
  } else if (this->epd_full_refresh_interval_ms_ > 0) {
    if (this->last_full_epd_refresh_ms_ == 0) {
      this->last_full_epd_refresh_ms_ = now_ms;
    }
    const uint32_t elapsed = now_ms - this->last_full_epd_refresh_ms_;
    if (elapsed >= this->epd_full_refresh_interval_ms_) {
      use_full_waveform = true;
      this->last_full_epd_refresh_ms_ = now_ms;
    }
  }

  /* Full (0xF7) flashes the whole panel — only on boot/interval. Normal draws use partial (0xDC). */
  if (use_full_waveform) {
    EPD_DisplayBukys792From800(this->image_bw_);
    EPD_Update();
  } else {
    const uint8_t passes = this->display_passes_ == 0 ? 1 : this->display_passes_;
    for (uint8_t p = 0; p < passes; p++) {
      EPD_DisplayBukys792From800(this->image_bw_);
      EPD_PartUpdate();
      if (p + 1 < passes) {
        bedside_delay_ms(30);
      }
    }
  }
}

void BedsideStatus::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MyHouse bedside status (Elecrow 5.79\" EPD library)");

  if (this->pin_screen_power_ != nullptr) {
    this->pin_screen_power_->setup();
    this->pin_screen_power_->digital_write(true);
    bedside_delay_ms(50);
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

  BedsideStatus::epd_busy_parent_ = this;
  bedside_epd_set_busy_read_fn(&BedsideStatus::epd_busy_read_trampoline_);

  this->apply_elecrow_pin_numbers_();
  this->epd_init_sequence_();

  for (auto &l : this->lines_)
    l.clear();
  this->lines_[0] = "ESPHome";
  this->lines_[1] = "Bedside status";
  this->lines_[2] = "Waiting for data";

}

void BedsideStatus::loop() {
  if (this->pending_first_epd_draw_) {
    this->pending_first_epd_draw_ = false;
    this->draw_status_screen_();
    this->last_partial_ms_ = bedside_millis();
    return;
  }
  uint32_t now = bedside_millis();
  uint32_t interval = this->last_fetch_ok_ ? 500u : 5000u;
  if (now - this->last_partial_ms_ < interval)
    return;
  this->draw_status_screen_();
  this->last_partial_ms_ = bedside_millis();
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
  ESP_LOGCONFIG(TAG, "  EPD partial refresh passes: %u", this->display_passes_);
  ESP_LOGCONFIG(TAG, "  EPD full refresh on boot: %s", this->epd_full_refresh_on_boot_ ? "yes" : "no");
  if (this->epd_full_refresh_interval_ms_ > 0) {
    ESP_LOGCONFIG(TAG, "  EPD full refresh interval: %u min", this->epd_full_refresh_interval_ms_ / 60000u);
  } else {
    ESP_LOGCONFIG(TAG, "  EPD full refresh interval: off (partial only)");
  }
  ESP_LOGCONFIG(TAG, "  EPD first paint runs in loop() after setup (not during setup())");
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace bedside_status
}  // namespace esphome
