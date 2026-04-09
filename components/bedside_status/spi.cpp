#include "spi.h"

static void configure_output(int pin) {
  gpio_reset_pin((gpio_num_t) pin);
  gpio_set_direction((gpio_num_t) pin, GPIO_MODE_OUTPUT);
}

static void configure_input(int pin) {
  gpio_reset_pin((gpio_num_t) pin);
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
