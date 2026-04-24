// games_main.c
// Dual-game app (ADC Pong + MPU6050 Tilt game) with GPIO26 mode switch.
// Note: this file is a saved copy and is NOT built by PlatformIO by default
// (default src_dir is ./src).

#include "dvi.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define FB_WIDTH DVI_H_ACTIVE_PIXELS
#define FB_HEIGHT DVI_V_ACTIVE_LINES
#define FB_SIZE_BYTES (FB_WIDTH * FB_HEIGHT)

#define ADC_GPIO_LEFT 40
#define ADC_GPIO_RIGHT 47
#define ADC_SAMPLES 8

#define BUTTON_PIN 26

#define MPU_I2C_PORT i2c0
#define MPU_SDA_PIN 4
#define MPU_SCL_PIN 5
#define MPU6050_ADDR 0x68
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B

// Pong game constants (long-side paddles: top/bottom)
#define PADDLE_W 90
#define PADDLE_H 12
#define PADDLE_MARGIN 22
#define BALL_SIZE 10

#define BALL_SPEED_X 3
#define BALL_SPEED_Y 4
#define MAX_BALL_VX 7

// Tilt game constants (MPU6050)
#define TILT_PLAYER_SIZE 14
#define TILT_TARGET_SIZE 12
#define TILT_ENEMY_SIZE 12

// SPI TFT (ILI9341-compatible)
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

typedef enum {
  GAME_PONG_ADC = 0,
  GAME_TILT_MPU6050 = 1,
} GameMode;

static inline uint8_t rgb332(uint8_t r, uint8_t g, uint8_t b) {
  return (uint8_t)((r & 0xE0) | ((g & 0xE0) >> 3) | (b >> 6));
}

#define COLOR_BG rgb332(5, 7, 14)
#define COLOR_MID rgb332(70, 70, 70)
#define COLOR_LEFT rgb332(0, 255, 0)
#define COLOR_RIGHT rgb332(255, 70, 70)
#define COLOR_BALL rgb332(255, 255, 255)
#define COLOR_SCORE rgb332(255, 220, 0)
#define COLOR_TILT_BG rgb332(10, 6, 15)
#define COLOR_TILT_PLAYER rgb332(0, 255, 255)
#define COLOR_TILT_TARGET rgb332(255, 255, 0)
#define COLOR_TILT_ENEMY rgb332(255, 60, 60)
#define COLOR_TILT_BAR rgb332(80, 180, 255)
#define COLOR_WARN rgb332(255, 0, 0)

static uint8_t framebuffer[FB_SIZE_BYTES] __attribute__((aligned(4)));
static uint8_t tft_linebuf[TFT_WIDTH * 2];

static GameMode current_game = GAME_PONG_ADC;
static bool mpu_ok = false;

// Pong state
static int top_paddle_x = (FB_WIDTH - PADDLE_W) / 2;
static int bottom_paddle_x = (FB_WIDTH - PADDLE_W) / 2;
static int ball_x = (FB_WIDTH - BALL_SIZE) / 2;
static int ball_y = (FB_HEIGHT - BALL_SIZE) / 2;
static int ball_vx = BALL_SPEED_X;
static int ball_vy = BALL_SPEED_Y;
static uint8_t top_score = 0;
static uint8_t bottom_score = 0;

// Tilt game state
static int tilt_player_x = FB_WIDTH / 2;
static int tilt_player_y = FB_HEIGHT / 2;
static int tilt_target_x = 60;
static int tilt_target_y = 60;
static int tilt_enemy_x = 200;
static int tilt_enemy_y = 140;
static int tilt_enemy_vx = 3;
static int tilt_enemy_vy = 2;
static int16_t tilt_ax_f = 0;
static int16_t tilt_ay_f = 0;
static uint16_t tilt_score = 0;
static uint16_t tilt_time_frames = 60u * 45u;

static uint32_t rng_state = 0x1a2b3c4d;

static inline void tft_select(void) { gpio_put(TFT_PIN_CS, 0); }
static inline void tft_deselect(void) { gpio_put(TFT_PIN_CS, 1); }

static inline int clampi(int v, int lo, int hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

static uint32_t rng_next(void) {
  uint32_t x = rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_state = x;
  return x;
}

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

static void tft_init(void) {
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
  tft_write_data_byte(0x28);

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

static void tft_blit_from_framebuffer_2x(void) {
  tft_set_addr_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);

  tft_select();
  gpio_put(TFT_PIN_DC, 1);

  for (int ty = 0; ty < TFT_HEIGHT; ++ty) {
    const uint8_t *src_row = &framebuffer[(ty * 2) * FB_WIDTH];
    for (int tx = 0; tx < TFT_WIDTH; ++tx) {
      uint8_t c332 = src_row[tx * 2];
      uint16_t c565 = rgb332_to_rgb565(c332);
      tft_linebuf[tx * 2] = (uint8_t)(c565 >> 8);
      tft_linebuf[tx * 2 + 1] = (uint8_t)(c565 & 0xff);
    }
    spi_write_blocking(TFT_SPI_PORT, tft_linebuf, sizeof(tft_linebuf));
  }

  tft_deselect();
}

static void fill_rect(int x, int y, int w, int h, uint8_t color) {
  if (x >= FB_WIDTH || y >= FB_HEIGHT || w <= 0 || h <= 0)
    return;

  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > FB_WIDTH)
    w = FB_WIDTH - x;
  if (y + h > FB_HEIGHT)
    h = FB_HEIGHT - y;
  if (w <= 0 || h <= 0)
    return;

  for (int row = 0; row < h; ++row) {
    memset(&framebuffer[(y + row) * FB_WIDTH + x], color, (size_t)w);
  }
}

static uint16_t adc_read_avg(uint gpio) {
  adc_select_input((uint)(gpio - ADC_BASE_PIN));
  uint32_t sum = 0;
  for (uint i = 0; i < ADC_SAMPLES; ++i) {
    sum += adc_read();
  }
  return (uint16_t)(sum / ADC_SAMPLES);
}

static void button_init(void) {
  gpio_init(BUTTON_PIN);
  gpio_set_dir(BUTTON_PIN, GPIO_IN);
  gpio_pull_up(BUTTON_PIN);
}

static bool button_pressed_event(void) {
  static bool stable = true;
  static bool last_raw = true;
  static uint8_t stable_count = 0;

  bool raw = gpio_get(BUTTON_PIN);

  if (raw != last_raw) {
    last_raw = raw;
    stable_count = 0;
    return false;
  }

  if (stable_count < 4)
    ++stable_count;

  if (stable_count >= 4 && raw != stable) {
    stable = raw;
    if (!stable)
      return true;
  }

  return false;
}

static bool mpu6050_init(void) {
  i2c_init(MPU_I2C_PORT, 400 * 1000);
  gpio_set_function(MPU_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(MPU_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(MPU_SDA_PIN);
  gpio_pull_up(MPU_SCL_PIN);

  sleep_ms(50);

  uint8_t wake_cmd[2] = {MPU6050_REG_PWR_MGMT_1, 0x00};
  if (i2c_write_blocking(MPU_I2C_PORT, MPU6050_ADDR, wake_cmd, 2, false) != 2) {
    return false;
  }

  sleep_ms(10);
  return true;
}

static bool mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az) {
  uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
  uint8_t data[6];

  if (i2c_write_blocking(MPU_I2C_PORT, MPU6050_ADDR, &reg, 1, true) != 1)
    return false;
  if (i2c_read_blocking(MPU_I2C_PORT, MPU6050_ADDR, data, 6, false) != 6)
    return false;

  *ax = (int16_t)((data[0] << 8) | data[1]);
  *ay = (int16_t)((data[2] << 8) | data[3]);
  *az = (int16_t)((data[4] << 8) | data[5]);
  return true;
}

static int paddle_x_from_adc(uint16_t sample) {
  uint32_t range = (uint32_t)(FB_WIDTH - PADDLE_W);
  return (int)(((uint32_t)sample * range) / 4095u);
}

static void pong_reset_ball(bool serve_down) {
  ball_x = (FB_WIDTH - BALL_SIZE) / 2;
  ball_y = (FB_HEIGHT - BALL_SIZE) / 2;
  ball_vy = serve_down ? BALL_SPEED_Y : -BALL_SPEED_Y;

  uint32_t r = time_us_32();
  int vx = BALL_SPEED_X + (int)(r % 3u);
  ball_vx = (r & 1u) ? vx : -vx;
}

static void pong_init(void) {
  top_paddle_x = (FB_WIDTH - PADDLE_W) / 2;
  bottom_paddle_x = (FB_WIDTH - PADDLE_W) / 2;
  top_score = 0;
  bottom_score = 0;
  pong_reset_ball(true);
}

static void pong_update(void) {
  top_paddle_x = paddle_x_from_adc(adc_read_avg(ADC_GPIO_RIGHT));
  bottom_paddle_x = paddle_x_from_adc(adc_read_avg(ADC_GPIO_LEFT));

  ball_x += ball_vx;
  ball_y += ball_vy;

  if (ball_x <= 0) {
    ball_x = 0;
    ball_vx = -ball_vx;
  } else if (ball_x + BALL_SIZE >= FB_WIDTH) {
    ball_x = FB_WIDTH - BALL_SIZE;
    ball_vx = -ball_vx;
  }

  const int top_y = PADDLE_MARGIN;
  const int bottom_y = FB_HEIGHT - PADDLE_MARGIN - PADDLE_H;

  if (ball_vy < 0 && ball_y <= top_y + PADDLE_H &&
      ball_y + BALL_SIZE >= top_y && ball_x + BALL_SIZE >= top_paddle_x &&
      ball_x <= top_paddle_x + PADDLE_W) {
    ball_y = top_y + PADDLE_H;
    ball_vy = BALL_SPEED_Y;

    int paddle_center = top_paddle_x + PADDLE_W / 2;
    int ball_center = ball_x + BALL_SIZE / 2;
    ball_vx += (ball_center - paddle_center) / 16;
    ball_vx = clampi(ball_vx, -MAX_BALL_VX, MAX_BALL_VX);
    if (ball_vx == 0)
      ball_vx = -1;
  }

  if (ball_vy > 0 && ball_y + BALL_SIZE >= bottom_y &&
      ball_y <= bottom_y + PADDLE_H && ball_x + BALL_SIZE >= bottom_paddle_x &&
      ball_x <= bottom_paddle_x + PADDLE_W) {
    ball_y = bottom_y - BALL_SIZE;
    ball_vy = -BALL_SPEED_Y;

    int paddle_center = bottom_paddle_x + PADDLE_W / 2;
    int ball_center = ball_x + BALL_SIZE / 2;
    ball_vx += (ball_center - paddle_center) / 16;
    ball_vx = clampi(ball_vx, -MAX_BALL_VX, MAX_BALL_VX);
    if (ball_vx == 0)
      ball_vx = 1;
  }

  if (ball_y + BALL_SIZE < 0) {
    if (bottom_score < 99)
      bottom_score++;
    pong_reset_ball(true);
  } else if (ball_y > FB_HEIGHT) {
    if (top_score < 99)
      top_score++;
    pong_reset_ball(false);
  }
}

static void pong_render(void) {
  memset(framebuffer, COLOR_BG, FB_SIZE_BYTES);

  for (int y = 0; y < FB_HEIGHT; y += 24) {
    fill_rect(FB_WIDTH / 2 - 2, y, 4, 14, COLOR_MID);
  }

  fill_rect(top_paddle_x, PADDLE_MARGIN, PADDLE_W, PADDLE_H, COLOR_LEFT);
  fill_rect(bottom_paddle_x, FB_HEIGHT - PADDLE_MARGIN - PADDLE_H, PADDLE_W,
            PADDLE_H, COLOR_RIGHT);

  fill_rect(ball_x, ball_y, BALL_SIZE, BALL_SIZE, COLOR_BALL);

  int top_pips = top_score > 20 ? 20 : top_score;
  int bottom_pips = bottom_score > 20 ? 20 : bottom_score;

  for (int i = 0; i < top_pips; ++i) {
    fill_rect(10 + i * 8, 10, 6, 12, COLOR_SCORE);
  }
  for (int i = 0; i < bottom_pips; ++i) {
    fill_rect(10 + i * 8, FB_HEIGHT - 22, 6, 12, COLOR_SCORE);
  }
}

static void tilt_spawn_target(void) {
  int max_x = FB_WIDTH - TILT_TARGET_SIZE - 6;
  int max_y = FB_HEIGHT - TILT_TARGET_SIZE - 30;
  tilt_target_x = 3 + (int)(rng_next() % (uint32_t)max_x);
  tilt_target_y = 24 + (int)(rng_next() % (uint32_t)max_y);
}

static void tilt_init(void) {
  tilt_player_x = FB_WIDTH / 2 - TILT_PLAYER_SIZE / 2;
  tilt_player_y = FB_HEIGHT / 2 - TILT_PLAYER_SIZE / 2;
  tilt_enemy_x = FB_WIDTH / 3;
  tilt_enemy_y = FB_HEIGHT / 3;
  tilt_enemy_vx = 3;
  tilt_enemy_vy = 2;
  tilt_ax_f = 0;
  tilt_ay_f = 0;
  tilt_score = 0;
  tilt_time_frames = 60u * 45u;
  tilt_spawn_target();
}

static void tilt_update(void) {
  int16_t ax = 0, ay = 0, az = 0;
  if (mpu_ok) {
    if (!mpu6050_read_accel(&ax, &ay, &az)) {
      mpu_ok = false;
    }
  }

  tilt_ax_f = (int16_t)(((int32_t)tilt_ax_f * 3 + ax) / 4);
  tilt_ay_f = (int16_t)(((int32_t)tilt_ay_f * 3 + ay) / 4);

  int dx = clampi((int)(-tilt_ay_f / 2500), -6, 6);
  int dy = clampi((int)(tilt_ax_f / 2500), -6, 6);

  tilt_player_x = clampi(tilt_player_x + dx, 0, FB_WIDTH - TILT_PLAYER_SIZE);
  tilt_player_y = clampi(tilt_player_y + dy, 20, FB_HEIGHT - TILT_PLAYER_SIZE);

  tilt_enemy_x += tilt_enemy_vx;
  tilt_enemy_y += tilt_enemy_vy;

  if (tilt_enemy_x <= 0 || tilt_enemy_x + TILT_ENEMY_SIZE >= FB_WIDTH) {
    tilt_enemy_vx = -tilt_enemy_vx;
    tilt_enemy_x = clampi(tilt_enemy_x, 0, FB_WIDTH - TILT_ENEMY_SIZE);
  }
  if (tilt_enemy_y <= 20 || tilt_enemy_y + TILT_ENEMY_SIZE >= FB_HEIGHT) {
    tilt_enemy_vy = -tilt_enemy_vy;
    tilt_enemy_y = clampi(tilt_enemy_y, 20, FB_HEIGHT - TILT_ENEMY_SIZE);
  }

  bool hit_target =
      (tilt_player_x < tilt_target_x + TILT_TARGET_SIZE) &&
      (tilt_player_x + TILT_PLAYER_SIZE > tilt_target_x) &&
      (tilt_player_y < tilt_target_y + TILT_TARGET_SIZE) &&
      (tilt_player_y + TILT_PLAYER_SIZE > tilt_target_y);

  if (hit_target) {
    if (tilt_score < 999)
      ++tilt_score;
    tilt_spawn_target();
  }

  bool hit_enemy =
      (tilt_player_x < tilt_enemy_x + TILT_ENEMY_SIZE) &&
      (tilt_player_x + TILT_PLAYER_SIZE > tilt_enemy_x) &&
      (tilt_player_y < tilt_enemy_y + TILT_ENEMY_SIZE) &&
      (tilt_player_y + TILT_PLAYER_SIZE > tilt_enemy_y);

  if (hit_enemy) {
    if (tilt_score > 0)
      --tilt_score;
    tilt_player_x = FB_WIDTH / 2 - TILT_PLAYER_SIZE / 2;
    tilt_player_y = FB_HEIGHT / 2 - TILT_PLAYER_SIZE / 2;
  }

  if (tilt_time_frames > 0)
    --tilt_time_frames;
  else
    tilt_init();
}

static void tilt_render(void) {
  memset(framebuffer, COLOR_TILT_BG, FB_SIZE_BYTES);

  fill_rect(0, 20, FB_WIDTH, 2, COLOR_MID);
  fill_rect(0, FB_HEIGHT - 2, FB_WIDTH, 2, COLOR_MID);
  fill_rect(0, 20, 2, FB_HEIGHT - 20, COLOR_MID);
  fill_rect(FB_WIDTH - 2, 20, 2, FB_HEIGHT - 20, COLOR_MID);

  int score_bar = (int)((tilt_score % 100) * 3);
  fill_rect(8, 4, 300, 10, COLOR_MID);
  fill_rect(8, 4, score_bar, 10, COLOR_TILT_BAR);

  int time_bar = (int)((tilt_time_frames * 300u) / (60u * 45u));
  fill_rect(FB_WIDTH - 308, 14, 300, 4, COLOR_MID);
  fill_rect(FB_WIDTH - 308, 14, time_bar, 4, COLOR_SCORE);

  fill_rect(tilt_target_x, tilt_target_y, TILT_TARGET_SIZE, TILT_TARGET_SIZE,
            COLOR_TILT_TARGET);
  fill_rect(tilt_enemy_x, tilt_enemy_y, TILT_ENEMY_SIZE, TILT_ENEMY_SIZE,
            COLOR_TILT_ENEMY);
  fill_rect(tilt_player_x, tilt_player_y, TILT_PLAYER_SIZE, TILT_PLAYER_SIZE,
            COLOR_TILT_PLAYER);

  if (!mpu_ok) {
    fill_rect(FB_WIDTH - 24, 2, 20, 16, COLOR_WARN);
  }
}

int main(void) {
  rng_state ^= time_us_32();

  adc_init();
  adc_gpio_init(ADC_GPIO_LEFT);
  adc_gpio_init(ADC_GPIO_RIGHT);

  button_init();
  mpu_ok = mpu6050_init();

  pong_init();
  tilt_init();
  current_game = GAME_PONG_ADC;
  pong_render();

  dvi_init();
  dvi_set_display_buffer(framebuffer);

  tft_init();
  tft_blit_from_framebuffer_2x();

  uint8_t tft_div = 0;
  bool dvi_running = false;

  bool detect_stable = dvi_screen_connected();
  bool detect_last_raw = detect_stable;
  uint8_t detect_change_count = 0;

  if (detect_stable) {
    dvi_start();
    dvi_running = true;
  }

  while (true) {
    bool detect_raw = dvi_screen_connected();
    if (detect_raw == detect_last_raw) {
      if (detect_raw != detect_stable && detect_change_count < 4) {
        ++detect_change_count;
        if (detect_change_count >= 4) {
          detect_stable = detect_raw;
          detect_change_count = 0;
        }
      }
    } else {
      detect_last_raw = detect_raw;
      detect_change_count = 0;
    }

    if (detect_stable && !dvi_running) {
      dvi_start();
      dvi_running = true;
    } else if (!detect_stable && dvi_running) {
      dvi_stop();
      dvi_running = false;
    }

    if (dvi_running) {
      while (!dvi_vsync_occurred())
        tight_loop_contents();
    } else {
      sleep_ms(16);
    }

    if (button_pressed_event()) {
      if (current_game == GAME_PONG_ADC) {
        current_game = GAME_TILT_MPU6050;
        tilt_init();
      } else {
        current_game = GAME_PONG_ADC;
        pong_init();
      }
    }

    if (current_game == GAME_PONG_ADC) {
      pong_update();
      pong_render();
    } else {
      tilt_update();
      tilt_render();
    }

    if (++tft_div >= 2) {
      tft_div = 0;
      tft_blit_from_framebuffer_2x();
    }
  }
}
