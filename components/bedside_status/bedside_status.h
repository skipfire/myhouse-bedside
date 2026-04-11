#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/time/real_time_clock.h"

#include <string>

namespace esphome {
namespace bedside_status {

class BedsideStatus : public PollingComponent {
 public:
  BedsideStatus() : PollingComponent(5000) {}

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_pin_sck(GPIOPin *pin) { this->pin_sck_ = pin; }
  void set_pin_mosi(GPIOPin *pin) { this->pin_mosi_ = pin; }
  void set_pin_rst(GPIOPin *pin) { this->pin_rst_ = pin; }
  void set_pin_dc(GPIOPin *pin) { this->pin_dc_ = pin; }
  void set_pin_cs(GPIOPin *pin) { this->pin_cs_ = pin; }
  void set_pin_busy(GPIOPin *pin) { this->pin_busy_ = pin; }
  void set_screen_power_pin(GPIOPin *pin) { this->pin_screen_power_ = pin; }
  void set_time(time::RealTimeClock *time) { this->time_src_ = time; }

  void set_status_url(const std::string &url) { this->status_url_ = url; }
  void set_verify_ssl(bool v) { this->verify_ssl_ = v; }
  void set_footer_ip_left(bool v) { this->footer_ip_left_ = v; }
  void set_display_status_filter(int v) { this->display_status_filter_ = v; }
  void set_text_size(uint16_t v) { this->text_size_ = v; }
  void set_footer_text_size(uint16_t v) { this->footer_text_size_ = v; }
  void set_display_passes(uint8_t v) { this->display_passes_ = v; }
  void set_epd_full_refresh_on_boot(bool v) { this->epd_full_refresh_on_boot_ = v; }
  void set_epd_full_refresh_interval_ms(uint32_t v) { this->epd_full_refresh_interval_ms_ = v; }

 private:
  static BedsideStatus *epd_busy_parent_;
  static int epd_busy_read_trampoline_(void);

 protected:
  void epd_init_sequence_();
  void apply_elecrow_pin_numbers_();
  void draw_status_screen_();
  void fetch_status_http_();
  void clip_line_(const std::string &in, char *out, size_t out_sz);
  void status_lines_from_json_(const std::string &json, bool *ok);
  std::string format_time_ampm_();

  GPIOPin *pin_sck_{nullptr};
  GPIOPin *pin_mosi_{nullptr};
  GPIOPin *pin_rst_{nullptr};
  GPIOPin *pin_dc_{nullptr};
  GPIOPin *pin_cs_{nullptr};
  GPIOPin *pin_busy_{nullptr};
  GPIOPin *pin_screen_power_{nullptr};

  time::RealTimeClock *time_src_{nullptr};

  std::string status_url_;
  bool verify_ssl_{true};
  bool footer_ip_left_{true};
  int display_status_filter_{3};
  uint16_t text_size_{24};
  /** Match MicroPython bedside_render (scale=3): use 24 with double-width 2412 font. */
  uint16_t footer_text_size_{24};
  /** Repeat bitmap push + EPD_PartUpdate (strengthens partial refresh; each pass ~1s+ BUSY). */
  uint8_t display_passes_{3};
  /** One full refresh (0xF7) on first paint after boot; then partial unless interval hits. */
  bool epd_full_refresh_on_boot_{true};
  /** If >0, run full refresh (0xF7) when this many ms have passed since last full (~5s+ each). */
  uint32_t epd_full_refresh_interval_ms_{120UL * 60UL * 1000UL};
  bool epd_boot_full_refresh_done_{false};
  uint32_t last_full_epd_refresh_ms_{0};

  uint8_t image_bw_[27200]{};
  std::string lines_[6];
  bool last_fetch_ok_{true};
  uint32_t last_partial_ms_{0};
  /** First full paint runs from loop(), not setup(), so ESPHome setup/TWDT are not blocked by long SPI. */
  bool pending_first_epd_draw_{true};

  static int gpio_num_(GPIOPin *pin);
};

}  // namespace bedside_status
}  // namespace esphome
