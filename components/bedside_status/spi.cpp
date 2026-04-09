#include "spi.h"

static bedside_epd_busy_read_fn g_bedside_epd_busy_read = nullptr;

void bedside_epd_set_busy_read_fn(bedside_epd_busy_read_fn fn) { g_bedside_epd_busy_read = fn; }

int bedside_epd_read_busy_level(void) {
  if (g_bedside_epd_busy_read != nullptr) {
    return g_bedside_epd_busy_read();
  }
  const gpio_num_t g = static_cast<gpio_num_t>(BUSY);
  if (!GPIO_IS_VALID_GPIO(g)) {
    return 0;
  }
  return gpio_get_level(g);
}

static void configure_output(int pin) {
  gpio_reset_pin((gpio_num_t) pin);
  gpio_set_direction((gpio_num_t) pin, GPIO_MODE_OUTPUT);
}

static void configure_input(int pin) {
  /* Do not gpio_reset_pin() here: BUSY is configured first by ESPHome's GPIOPin::setup().
   * Resetting then re-reading with gpio_get_level() has faulted on ESP-IDF 5.x (S3). */
  gpio_set_direction((gpio_num_t) pin, GPIO_MODE_INPUT);
}

void EPD_GPIOInit(void) {
  configure_output(SCK);
  configure_output(MOSI);
  configure_output(RES);
  configure_output(DC);
  configure_output(CS);
  configure_input(BUSY);
}

void EPD_WR_Bus(uint8_t dat) {
  uint8_t i;
  EPD_CS_Clr();
  for (i = 0; i < 8; i++) {
    EPD_SCK_Clr();
    if (dat & 0x80) {
      EPD_MOSI_Set();
    } else {
      EPD_MOSI_Clr();
    }
    EPD_SCK_Set();
    dat <<= 1;
  }
  EPD_CS_Set();
}

void EPD_WR_REG(uint8_t reg) {
  EPD_DC_Clr();
  EPD_WR_Bus(reg);
  EPD_DC_Set();
}

void EPD_WR_DATA8(uint8_t dat) {
  EPD_DC_Set();
  EPD_WR_Bus(dat);
  EPD_DC_Set();
}
