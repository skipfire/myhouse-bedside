#include "EPD_Init.h"
#include "EPD.h"

#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static inline void epd_delay_ms(int ms) {
  if (ms <= 0) {
    return;
  }
  vTaskDelay(pdMS_TO_TICKS(ms));
}



/*******************************************************************
    函数说明:判忙函数
    入口参数:无
    说明:忙状态为1
*******************************************************************/
void EPD_READBUSY(void)
{
  const int64_t deadline_us = esp_timer_get_time() + (20LL * 1000000);
  while (bedside_epd_read_busy_level() != 0) {
    if (esp_timer_get_time() > deadline_us) {
      break;
    }
    vTaskDelay(1);
  }
}
/*******************************************************************
    函数说明:硬件复位函数
    入口参数:无
    说明:在E-Paper进入Deepsleep状态后需要硬件复位
*******************************************************************/
void EPD_HW_RESET(void)
{
  epd_delay_ms(10);
  EPD_RES_Clr();
  epd_delay_ms(10);
  EPD_RES_Set();
  epd_delay_ms(10);
  EPD_READBUSY();
}

/*******************************************************************
    函数说明:更新函数
    入口参数:无
    说明:更新显示内容到E-Paper
*******************************************************************/
void EPD_Update(void)
{
  EPD_WR_REG(0x22);
  EPD_WR_DATA8(0xF7);
  EPD_WR_REG(0x20);
  EPD_READBUSY();
}
/*******************************************************************
    函数说明:局刷更新函数
    入口参数:无
    说明:E-Paper工作在局刷模式
*******************************************************************/
void EPD_PartUpdate(void)
{
  EPD_WR_REG(0x22);
  EPD_WR_DATA8(0xDC);
  EPD_WR_REG(0x20);
  EPD_READBUSY();
}
/*******************************************************************
    函数说明:快刷更新函数
    入口参数:无
    说明:E-Paper工作在快刷模式
*******************************************************************/
void EPD_FastUpdate(void)
{
  EPD_WR_REG(0x22);
  EPD_WR_DATA8(0xC7);
  EPD_WR_REG(0x20);
  EPD_READBUSY();
}

/*******************************************************************
    函数说明:休眠函数
    入口参数:无
    说明:屏幕进入低功耗模式
*******************************************************************/
void EPD_DeepSleep(void)
{
//    EPD_WR_REG(0x3C);
//    EPD_WR_DATA8(0x01);
//    EPD_READBUSY();

  EPD_WR_REG(0x10);
  EPD_WR_DATA8(0x01);
  epd_delay_ms(5);
}

void EPD_Init(void)
{
  EPD_HW_RESET();
  EPD_READBUSY();
  EPD_WR_REG(0x12);
  EPD_READBUSY();
}

void EPD_FastMode1Init(void)
{
  EPD_HW_RESET();
  EPD_READBUSY();
  EPD_WR_REG(0x12);  //SWRESET
  EPD_READBUSY();

  EPD_WR_REG(0x18); //Read built-in temperature sensor
  EPD_WR_DATA8(0x80);

  EPD_WR_REG(0x22); // Load temperature value
  EPD_WR_DATA8(0xB1);
  EPD_WR_REG(0x20);
  EPD_READBUSY();

  EPD_WR_REG(0x1A); // Write to temperature register
  EPD_WR_DATA8(0x64);
  EPD_WR_DATA8(0x00);

  EPD_WR_REG(0x22); // Load temperature value
  EPD_WR_DATA8(0x91);
  EPD_WR_REG(0x20);
  EPD_READBUSY();

  EPD_WR_REG(0x3C);
  EPD_WR_DATA8(0x3);
  EPD_READBUSY();
}

void EPD_SetRAMMP(void)
{
  EPD_WR_REG(0x11);	 // Data Entry mode setting
  EPD_WR_DATA8(0x05);     // 1 –Y decrement, X increment
  EPD_WR_REG(0x44);	 						 // Set Ram X- address Start / End position
  EPD_WR_DATA8(0x00);     						 // XStart, POR = 00h
  EPD_WR_DATA8(0x31); //400/8-1
  EPD_WR_REG(0x45);	 									// Set Ram Y- address  Start / End position
  EPD_WR_DATA8(0x0f);
  EPD_WR_DATA8(0x01);  //300-1
  EPD_WR_DATA8(0x00);     									// YEnd L
  EPD_WR_DATA8(0x00);
}

void EPD_SetRAMMA(void)
{
  EPD_WR_REG(0x4e);
  EPD_WR_DATA8(0x00);
  EPD_WR_REG(0x4f);
  EPD_WR_DATA8(0x0f);
  EPD_WR_DATA8(0x01);
}

void EPD_SetRAMSP(void)
{
  EPD_WR_REG(0x91);
  EPD_WR_DATA8(0x04);
  EPD_WR_REG(0xc4);	 // Set Ram X- address Start / End position
  EPD_WR_DATA8(0x31);// XStart, POR = 00h
  EPD_WR_DATA8(0x00);
  EPD_WR_REG(0xc5);	 // Set Ram Y- address  Start / End position
  EPD_WR_DATA8(0x0f);
  EPD_WR_DATA8(0x01);
  EPD_WR_DATA8(0x00);// YEnd L
  EPD_WR_DATA8(0x00);
}

void EPD_SetRAMSA(void)
{
  EPD_WR_REG(0xce);
  EPD_WR_DATA8(0x31);
  EPD_WR_REG(0xcf);
  EPD_WR_DATA8(0x0f);
  EPD_WR_DATA8(0x01);
}

void EPD_Clear_R26A6H(void)
{
  uint16_t i, j;
  EPD_SetRAMMA();
  EPD_WR_REG(0x26);
  for (i = 0; i < Gate_BITS; i++)
  {
    for (j = 0; j < Source_BYTES; j++)
    {
      EPD_WR_DATA8(0xFF);
    }
  }
  EPD_SetRAMSA();
  EPD_WR_REG(0xA6);
  for (i = 0; i < Gate_BITS; i++)
  {
    for (j = 0; j < Source_BYTES; j++)
    {
      EPD_WR_DATA8(0xFF);
    }
  }
}

void EPD_Display_Clear(void)
{
  uint16_t i, j;
  EPD_SetRAMMP();
  EPD_SetRAMMA();
  EPD_WR_REG(0x24);
  for (i = 0; i < Gate_BITS; i++)
  {
    for (j = 0; j < Source_BYTES; j++)
    {
      EPD_WR_DATA8(0xFF);
    }
  }
  EPD_SetRAMMA();
  EPD_WR_REG(0x26);
  for (i = 0; i < Gate_BITS; i++)
  {
    for (j = 0; j < Source_BYTES; j++)
    {
      EPD_WR_DATA8(0x00);
    }
  }
  EPD_SetRAMSP();
  EPD_SetRAMSA();
  EPD_WR_REG(0xA4);
  for (i = 0; i < Gate_BITS; i++)
  {
    for (j = 0; j < Source_BYTES; j++)
    {
      EPD_WR_DATA8(0xFF);
    }
  }
  EPD_SetRAMSA();
  EPD_WR_REG(0xA6);
  for (i = 0; i < Gate_BITS; i++)
  {
    for (j = 0; j < Source_BYTES; j++)
    {
      EPD_WR_DATA8(0x00);
    }
  }
}


void EPD_Display(const uint8_t *ImageBW)
{
  uint32_t i;
  uint8_t tempOriginal;
  uint32_t tempcol = 0;
  uint32_t templine = 0;
  EPD_SetRAMMP();
  EPD_SetRAMMA();
  EPD_WR_REG(0x24);
  for (i = 0; i < ALLSCREEN_BYTES; i++)
  {
    tempOriginal = *(ImageBW + templine * Source_BYTES * 2 + tempcol);
    templine++;
    if (templine >= Gate_BITS)
    {
      tempcol++;
      templine = 0;
    }
    EPD_WR_DATA8(tempOriginal);
  }
  EPD_SetRAMSP();
  EPD_SetRAMSA();
  EPD_WR_REG(0xa4);   //write RAM for black(0)/white (1)
  /* Same dual-IC byte dislocation as EPD_WhiteScreen_ALL_Fast: slave transfer must not
   * repeat the master scan from (0,0); that misaligns the two RAMs and blanks most
   * of the panel. Leaving stale tempcol (e.g. 50) was also wrong — weak/washed ink. */
  if (tempcol > 0) {
    tempcol--;
  }
  templine = 0;
  for (i = 0; i < ALLSCREEN_BYTES; i++)
  {
    tempOriginal = *(ImageBW + templine * Source_BYTES * 2 + tempcol);
    templine++;
    if (templine >= Gate_BITS)
    {
      tempcol++;
      templine = 0;
    }
    EPD_WR_DATA8(tempOriginal);
  }
}

/* --- Bukys / MicroPython crowpanel579_epd.display_bitmap (792-wide linear buffer) --- */

#define BUKYS_BUF_BYTES ((792u * (uint32_t) Gate_BITS) / 8u)
#define BUKYS_ROW_BYTES (792u / 8u)
#define ELECROW_ROW_BYTES (EPD_W / 8u)

static inline void epd_wr_y9(uint16_t y)
{
  EPD_WR_DATA8(static_cast<uint8_t>(y & 0xFFu));
  EPD_WR_DATA8(static_cast<uint8_t>((y >> 8) & 0x01u));
}

static void EPD_SetRAMMP_Bukys_YWindow(uint16_t gate_y0, uint16_t gate_y1)
{
  EPD_WR_REG(0x11);
  EPD_WR_DATA8(0x02);
  EPD_WR_REG(0x44);
  EPD_WR_DATA8(0x31);
  EPD_WR_DATA8(0x00);
  EPD_WR_REG(0x45);
  epd_wr_y9(gate_y0);
  epd_wr_y9(gate_y1);
}

static void EPD_SetRAMMP_Bukys(void) { EPD_SetRAMMP_Bukys_YWindow(0, static_cast<uint16_t>(Gate_BITS - 1)); }

static void EPD_SetRAMMA_Bukys_At(uint8_t x_byte, uint16_t gate_y)
{
  EPD_WR_REG(0x4E);
  EPD_WR_DATA8(x_byte);
  EPD_WR_REG(0x4F);
  epd_wr_y9(gate_y);
}

static void EPD_SetRAMMA_Bukys(void) { EPD_SetRAMMA_Bukys_At(0x31, 0); }

static void EPD_SetRAMSP_Bukys_YWindow(uint16_t gate_y0, uint16_t gate_y1)
{
  EPD_WR_REG(0x91);
  EPD_WR_DATA8(0x03);
  EPD_WR_REG(0xC4);
  EPD_WR_DATA8(0x00);
  EPD_WR_DATA8(0x31);
  EPD_WR_REG(0xC5);
  epd_wr_y9(gate_y0);
  epd_wr_y9(gate_y1);
}

static void EPD_SetRAMSP_Bukys(void)
{
  EPD_SetRAMSP_Bukys_YWindow(0, static_cast<uint16_t>(Gate_BITS - 1));
}

static void EPD_SetRAMSA_Bukys_At(uint8_t x_byte, uint16_t gate_y)
{
  EPD_WR_REG(0xCE);
  EPD_WR_DATA8(x_byte);
  EPD_WR_REG(0xCF);
  epd_wr_y9(gate_y);
}

static void EPD_SetRAMSA_Bukys(void) { EPD_SetRAMSA_Bukys_At(0x00, 0); }

static uint8_t reverse_bits_u8(uint8_t b)
{
  b = (uint8_t)(((b & 0xF0u) >> 4) | ((b & 0x0Fu) << 4));
  b = (uint8_t)(((b & 0xCCu) >> 2) | ((b & 0x33u) << 2));
  b = (uint8_t)(((b & 0xAAu) >> 1) | ((b & 0x55u) << 1));
  return b;
}

/* Paint uses Elecrow scan (0x05 RAM); Bukys stream assumes Ignas entry mode (0x02). Remap 180° + MSB
 * reversal so the 792×272 slice matches on-screen orientation vs EPD_Display. */
static uint8_t epd_byte_visible792_from_800buf(const uint8_t *img, uint32_t linear792)
{
  const uint32_t row_b = linear792 / BUKYS_ROW_BYTES;
  const uint32_t col_b = linear792 % BUKYS_ROW_BYTES;
  const uint32_t row = (uint32_t) Gate_BITS - 1u - row_b;
  const uint32_t col = BUKYS_ROW_BYTES - 1u - col_b;
  if (row >= (uint32_t) Gate_BITS) {
    return 0xFF;
  }
  return reverse_bits_u8(img[row * ELECROW_ROW_BYTES + col]);
}

static void epd_bukys_stream_pos_range(const uint8_t *image_bw800, uint32_t pos, uint32_t end_pos)
{
  /* Static: large setup stack + font drawing; avoid stack pressure on ESPHome loop task. */
  static uint8_t chunk[50];

  while (pos < end_pos) {
    if (pos + 50 > end_pos) {
      break;
    }
    for (int i = 0; i < 50; i++) {
      chunk[i] = epd_byte_visible792_from_800buf(image_bw800, pos + (uint32_t) i);
    }
    EPD_WR_REG(0xA4);
    EPD_WR_DATA_BURST(chunk, 50);
    pos += 49;
    if (pos + 50 > end_pos) {
      break;
    }
    for (int i = 0; i < 50; i++) {
      chunk[i] = epd_byte_visible792_from_800buf(image_bw800, pos + (uint32_t) i);
    }
    EPD_WR_REG(0x24);
    EPD_WR_DATA_BURST(chunk, 50);
    pos += 50;
    /* Long bit-bang can trip ESP32-S3 task/idle WDT (crash BT often ends in prvIdleTask). */
    taskYIELD();
    (void) esp_task_wdt_reset();
  }
}

void EPD_DisplayBukys792From800(const uint8_t *image_bw800)
{
  EPD_SetRAMMP_Bukys();
  EPD_SetRAMMA_Bukys();
  EPD_SetRAMSP_Bukys();
  EPD_SetRAMSA_Bukys();
  epd_bukys_stream_pos_range(image_bw800, 0, BUKYS_BUF_BYTES);
}

void EPD_DisplayBukys792From800_YBand(const uint8_t *image_bw800, uint16_t paint_y0, uint16_t paint_y1_inclusive)
{
  if (paint_y0 > paint_y1_inclusive) {
    return;
  }
  const uint16_t gh = static_cast<uint16_t>(Gate_BITS);
  if (paint_y0 >= gh) {
    return;
  }
  if (paint_y1_inclusive >= gh) {
    paint_y1_inclusive = gh - 1;
  }
  /* Bukys stream row_b order: row_b=0 is bottom (paint y=gh-1); see epd_byte_visible792_from_800buf. */
  const uint16_t gate_y0 = static_cast<uint16_t>((gh - 1u) - paint_y1_inclusive);
  const uint16_t gate_y1 = static_cast<uint16_t>((gh - 1u) - paint_y0);

  EPD_SetRAMMP_Bukys_YWindow(gate_y0, gate_y1);
  EPD_SetRAMMA_Bukys_At(0x31, gate_y0);
  EPD_SetRAMSP_Bukys_YWindow(gate_y0, gate_y1);
  EPD_SetRAMSA_Bukys_At(0x00, gate_y0);

  const uint32_t pos0 = static_cast<uint32_t>(gate_y0) * BUKYS_ROW_BYTES;
  const uint32_t end_pos = static_cast<uint32_t>(gate_y1 + 1u) * BUKYS_ROW_BYTES;
  epd_bukys_stream_pos_range(image_bw800, pos0, end_pos);
}

//Horizontal scanning, from right to left, from bottom to top
void EPD_WhiteScreen_ALL_Fast(const unsigned char *datas)
{
  unsigned int i;
  unsigned char tempOriginal;
  unsigned int tempcol = 0;
  unsigned int templine = 0;

  EPD_WR_REG(0x11);
  EPD_WR_DATA8(0x05);

  EPD_WR_REG(0x44); //set Ram-X address start/end position
  EPD_WR_DATA8(0x00);
  EPD_WR_DATA8(0x31);    //0x12-->(18+1)*8=152

  EPD_WR_REG(0x45); //set Ram-Y address start/end position
  EPD_WR_DATA8(0x0F);   //0x97-->(151+1)=152
  EPD_WR_DATA8(0x01);
  EPD_WR_DATA8(0x00);
  EPD_WR_DATA8(0x00);

  EPD_WR_REG(0x4E);
  EPD_WR_DATA8(0x00);
  EPD_WR_REG(0x4F);
  EPD_WR_DATA8(0x0F);
  EPD_WR_DATA8(0x01);

  EPD_READBUSY();
  EPD_WR_REG(0x24);   //write RAM for black(0)/white (1)
  for (i = 0; i < Source_BYTES * Gate_BITS; i++)
  {
    tempOriginal = *(datas + templine * Source_BYTES * 2 + tempcol);
    templine++;
    if (templine >= Gate_BITS)
    {
      tempcol++;
      templine = 0;
    }
    EPD_WR_DATA8(~tempOriginal);
  }

  EPD_WR_REG(0x26);   //write RAM for black(0)/white (1)
  for (i = 0; i < Source_BYTES * Gate_BITS; i++)
  {
    EPD_WR_DATA8(0X00);
  }


  EPD_WR_REG(0x91);
  EPD_WR_DATA8(0x04);

  EPD_WR_REG(0xC4); //set Ram-X address start/end position
  EPD_WR_DATA8(0x31);
  EPD_WR_DATA8(0x00);    //0x12-->(18+1)*8=152

  EPD_WR_REG(0xC5); //set Ram-Y address start/end position
  EPD_WR_DATA8(0x0F);   //0x97-->(151+1)=152  ÐÞ¸ÄµÄ
  EPD_WR_DATA8(0x01);
  EPD_WR_DATA8(0x00);
  EPD_WR_DATA8(0x00);

  EPD_WR_REG(0xCE);
  EPD_WR_DATA8(0x31);
  EPD_WR_REG(0xCF);
  EPD_WR_DATA8(0x0F);
  EPD_WR_DATA8(0x01);

  EPD_READBUSY();

  tempcol = tempcol - 1; //Byte dislocation processing
  templine = 0;
  EPD_WR_REG(0xa4);   //write RAM for black(0)/white (1)
  for (i = 0; i < Source_BYTES * Gate_BITS; i++)
  {
    tempOriginal = *(datas + templine * Source_BYTES * 2 + tempcol);
    templine++;
    if (templine >= Gate_BITS)
    {
      tempcol++;
      templine = 0;
    }
    EPD_WR_DATA8(~tempOriginal);
  }

  EPD_WR_REG(0xa6);   //write RAM for black(0)/white (1)
  for (i = 0; i < Source_BYTES * Gate_BITS; i++)
  {
    EPD_WR_DATA8(0X00);
  }

  EPD_FastUpdate();
}
