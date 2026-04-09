#ifndef _SPI_H_
#define _SPI_H_

#include <Arduino.h>

// Runtime GPIO numbers for CrowPanel 5.79" (Elecrow sample uses bit-bang SPI).
// Set via esphome::bedside_status::elecrow579_apply_pin_numbers() before EPD_GPIOInit().
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

#define EPD_SCK_Clr() digitalWrite(SCK, LOW)
#define EPD_SCK_Set() digitalWrite(SCK, HIGH)

#define EPD_MOSI_Clr() digitalWrite(MOSI, LOW)
#define EPD_MOSI_Set() digitalWrite(MOSI, HIGH)

#define EPD_RES_Clr() digitalWrite(RES, LOW)
#define EPD_RES_Set() digitalWrite(RES, HIGH)

#define EPD_DC_Clr() digitalWrite(DC, LOW)
#define EPD_DC_Set() digitalWrite(DC, HIGH)

#define EPD_CS_Clr() digitalWrite(CS, LOW)
#define EPD_CS_Set() digitalWrite(CS, HIGH)

#define EPD_ReadBUSY digitalRead(BUSY)

void EPD_GPIOInit(void);
void EPD_WR_Bus(uint8_t dat);
void EPD_WR_REG(uint8_t reg);
void EPD_WR_DATA8(uint8_t dat);

#endif
