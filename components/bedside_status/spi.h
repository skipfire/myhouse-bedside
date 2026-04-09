#ifndef _SPI_H_
#define _SPI_H_

#include "driver/gpio.h"
#include <stddef.h>

// Runtime GPIO numbers for CrowPanel 5.79" (Elecrow sample uses bit-bang SPI).
// Set before EPD_GPIOInit() (BedsideStatus copies YAML pins into these).
extern int elecrow579_pin_sck;
extern int elecrow579_pin_mosi;
extern int elecrow579_pin_res;
extern int elecrow579_pin_dc;
extern int elecrow579_pin_cs;
extern int elecrow579_pin_busy;

#define SCK elecrow579_pin_sck
#define MOSI elecrow579_pin_mosi
#define RES elecrow579_pin_res
#define DC elecrow579_pin_dc
#define CS elecrow579_pin_cs
#define BUSY elecrow579_pin_busy

#define EPD_SCK_Clr() gpio_set_level((gpio_num_t) (SCK), 0)
#define EPD_SCK_Set() gpio_set_level((gpio_num_t) (SCK), 1)

#define EPD_MOSI_Clr() gpio_set_level((gpio_num_t) (MOSI), 0)
#define EPD_MOSI_Set() gpio_set_level((gpio_num_t) (MOSI), 1)

#define EPD_RES_Clr() gpio_set_level((gpio_num_t) (RES), 0)
#define EPD_RES_Set() gpio_set_level((gpio_num_t) (RES), 1)

#define EPD_DC_Clr() gpio_set_level((gpio_num_t) (DC), 0)
#define EPD_DC_Set() gpio_set_level((gpio_num_t) (DC), 1)

#define EPD_CS_Clr() gpio_set_level((gpio_num_t) (CS), 0)
#define EPD_CS_Set() gpio_set_level((gpio_num_t) (CS), 1)

#define EPD_ReadBUSY bedside_epd_read_busy_level()

#ifdef __cplusplus
extern "C" {
#endif

/** If set, EPD_READBUSY() uses this (e.g. ESPHome GPIOPin); else gpio_get_level(BUSY). */
typedef int (*bedside_epd_busy_read_fn)(void);
void bedside_epd_set_busy_read_fn(bedside_epd_busy_read_fn fn);
int bedside_epd_read_busy_level(void);

#ifdef __cplusplus
}
#endif

void EPD_GPIOInit(void);
void EPD_WR_Bus(uint8_t dat);
void EPD_WR_REG(uint8_t reg);
void EPD_WR_DATA8(uint8_t dat);
/** DC=data; CS stays low for all bytes (matches MicroPython write_data_burst). */
void EPD_WR_DATA_BURST(const uint8_t *buf, size_t len);

#endif
