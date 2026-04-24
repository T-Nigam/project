#include "tft_ili9341.h"
#include "framebuffer.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#define TFT_SPI_PORT spi0
#define TFT_PIN_CS 1
#define TFT_PIN_SCK 2
#define TFT_PIN_MOSI 3
#define TFT_PIN_DC 22
#define TFT_PIN_RST 23
#define TFT_SPI_BAUD 40000000

#define TFT_WIDTH 320
#define TFT_HEIGHT 240

#define ILI9341_SWRESET 0x01
#define ILI9341_SLPOUT 0x11
#define ILI9341_DISPOFF 0x28
#define ILI9341_DISPON 0x29
#define ILI9341_CASET 0x2A
#define ILI9341_PASET 0x2B
#define ILI9341_RAMWR 0x2C
#define ILI9341_MADCTL 0x36
#define ILI9341_PIXFMT 0x3A
#define ILI9341_FRMCTR1 0xB1
#define ILI9341_DFUNCTR 0xB6
#define ILI9341_PWCTR1 0xC0
#define ILI9341_PWCTR2 0xC1
#define ILI9341_VMCTR1 0xC5
#define ILI9341_VMCTR2 0xC7
#define ILI9341_GAMMASET 0x26
#define ILI9341_GMCTRP1 0xE0
#define ILI9341_GMCTRN1 0xE1

static uint8_t tft_linebuf[TFT_WIDTH * 2];

static inline void tft_select(void) { gpio_put(TFT_PIN_CS, 0); }
static inline void tft_deselect(void) { gpio_put(TFT_PIN_CS, 1); }

static inline uint16_t rgb332_to_rgb565(uint8_t c) {
  uint8_t r3 = (uint8_t)((c >> 5) & 0x07);
  uint8_t g3 = (uint8_t)((c >> 2) & 0x07);
  uint8_t b2 = (uint8_t)(c & 0x03);

  uint8_t r5 = (uint8_t)((r3 << 2) | (r3 >> 1));
  uint8_t g6 = (uint8_t)((g3 << 3) | g3);
  uint8_t b5 = (uint8_t)((b2 << 3) | (b2 << 1) | (b2 >> 1));

  return (uint16_t)((r5 << 11) | (g6 << 5) | b5);
}

static void tft_write_command(uint8_t command) {
  tft_select();
  gpio_put(TFT_PIN_DC, 0);
  spi_write_blocking(TFT_SPI_PORT, &command, 1);
  tft_deselect();
}

static void tft_write_data(const uint8_t *data, size_t len) {
  tft_select();
  gpio_put(TFT_PIN_DC, 1);
  spi_write_blocking(TFT_SPI_PORT, data, len);
  tft_deselect();
}

static void tft_write_data_byte(uint8_t data) { tft_write_data(&data, 1); }

static void tft_reset(void) {
  gpio_put(TFT_PIN_RST, 1);
  sleep_ms(20);
  gpio_put(TFT_PIN_RST, 0);
  sleep_ms(20);
  gpio_put(TFT_PIN_RST, 1);
  sleep_ms(150);
}

static void tft_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1,
                                uint16_t y1) {
  uint8_t data[4];

  tft_write_command(ILI9341_CASET);
  data[0] = (uint8_t)(x0 >> 8);
  data[1] = (uint8_t)(x0 & 0xff);
  data[2] = (uint8_t)(x1 >> 8);
  data[3] = (uint8_t)(x1 & 0xff);
  tft_write_data(data, sizeof(data));

  tft_write_command(ILI9341_PASET);
  data[0] = (uint8_t)(y0 >> 8);
  data[1] = (uint8_t)(y0 & 0xff);
  data[2] = (uint8_t)(y1 >> 8);
  data[3] = (uint8_t)(y1 & 0xff);
  tft_write_data(data, sizeof(data));

  tft_write_command(ILI9341_RAMWR);
}

void tft_init(void) {
  gpio_init(TFT_PIN_CS);
  gpio_set_dir(TFT_PIN_CS, GPIO_OUT);
  gpio_put(TFT_PIN_CS, 1);

  spi_init(TFT_SPI_PORT, TFT_SPI_BAUD);
  spi_set_format(TFT_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  gpio_set_function(TFT_PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(TFT_PIN_MOSI, GPIO_FUNC_SPI);

  gpio_init(TFT_PIN_DC);
  gpio_set_dir(TFT_PIN_DC, GPIO_OUT);
  gpio_put(TFT_PIN_DC, 1);

  gpio_init(TFT_PIN_RST);
  gpio_set_dir(TFT_PIN_RST, GPIO_OUT);
  gpio_put(TFT_PIN_RST, 1);

  tft_reset();

  tft_write_command(ILI9341_SWRESET);
  sleep_ms(150);

  tft_write_command(ILI9341_DISPOFF);

  tft_write_command(ILI9341_PWCTR1);
  tft_write_data_byte(0x23);

  tft_write_command(ILI9341_PWCTR2);
  tft_write_data_byte(0x10);

  tft_write_command(ILI9341_VMCTR1);
  {
    const uint8_t data[] = {0x3E, 0x28};
    tft_write_data(data, sizeof(data));
  }

  tft_write_command(ILI9341_VMCTR2);
  tft_write_data_byte(0x86);

  tft_write_command(ILI9341_MADCTL);
  tft_write_data_byte(0x28); // landscape, BGR

  tft_write_command(ILI9341_PIXFMT);
  tft_write_data_byte(0x55);

  tft_write_command(ILI9341_FRMCTR1);
  {
    const uint8_t data[] = {0x00, 0x18};
    tft_write_data(data, sizeof(data));
  }

  tft_write_command(ILI9341_DFUNCTR);
  {
    const uint8_t data[] = {0x08, 0x82, 0x27};
    tft_write_data(data, sizeof(data));
  }

  tft_write_command(ILI9341_GAMMASET);
  tft_write_data_byte(0x01);

  tft_write_command(ILI9341_GMCTRP1);
  {
    const uint8_t data[] = {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E,
                            0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09,
                            0x00};
    tft_write_data(data, sizeof(data));
  }

  tft_write_command(ILI9341_GMCTRN1);
  {
    const uint8_t data[] = {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31,
                            0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36,
                            0x0F};
    tft_write_data(data, sizeof(data));
  }

  tft_write_command(ILI9341_SLPOUT);
  sleep_ms(120);

  tft_write_command(ILI9341_DISPON);
  sleep_ms(20);
}

void tft_blit_from_fb_2x(const uint8_t *fb) {
  tft_set_addr_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);

  tft_select();
  gpio_put(TFT_PIN_DC, 1);

  for (int ty = 0; ty < TFT_HEIGHT; ++ty) {
    const uint8_t *src_row = &fb[(ty * 2) * FB_WIDTH];
    for (int tx = 0; tx < TFT_WIDTH; ++tx) {
      uint16_t c565 = rgb332_to_rgb565(src_row[tx * 2]);
      tft_linebuf[tx * 2] = (uint8_t)(c565 >> 8);
      tft_linebuf[tx * 2 + 1] = (uint8_t)(c565 & 0xff);
    }
    spi_write_blocking(TFT_SPI_PORT, tft_linebuf, sizeof(tft_linebuf));
  }

  tft_deselect();
}
