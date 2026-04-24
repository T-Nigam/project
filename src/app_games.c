#include "app_games.h"
#include "button.h"
#include "dvi.h"
#include "framebuffer.h"
#include "hardware/adc.h"
#include "mpu6050.h"
#include "pico/stdlib.h"
#include "pong_game.h"
#include "tft_ili9341.h"
#include "tilt_game.h"

#define ADC_GPIO_TOP 47
#define ADC_GPIO_BOTTOM 40
#define ADC_SAMPLES 8

#define BUTTON_PIN 26

#define MPU_I2C_PORT i2c0
#define MPU_SDA_PIN 4
#define MPU_SCL_PIN 5

typedef enum {
  GAME_PONG_ADC = 0,
  GAME_TILT_MPU6050 = 1,
} GameMode;

#define COLOR_HSTX_ON rgb332(0, 255, 0)
#define COLOR_HSTX_OFF rgb332(255, 0, 0)
#define COLOR_HSTX_RAW rgb332(0, 180, 255)

static void draw_hstx_indicator(bool connected, bool raw_pin_high) {
  // Big block: interpreted connected state. Small block: raw GPIO state.
  fb_fill_rect(FB_WIDTH - 18, 24, 14, 14,
               connected ? COLOR_HSTX_ON : COLOR_HSTX_OFF);
  fb_fill_rect(FB_WIDTH - 8, 40, 4, 4,
               raw_pin_high ? COLOR_HSTX_RAW : rgb332(70, 70, 70));
}

static uint16_t adc_read_avg(uint gpio) {
  adc_select_input((uint)(gpio - ADC_BASE_PIN));
  uint32_t sum = 0;
  for (uint i = 0; i < ADC_SAMPLES; ++i)
    sum += adc_read();
  return (uint16_t)(sum / ADC_SAMPLES);
}

static void sync_dvi_state(bool *dvi_running, bool *detect_stable,
                           bool *detect_last_raw, uint8_t *detect_change_count) {
  bool detect_raw = dvi_screen_connected();

  if (detect_raw == *detect_last_raw) {
    if (detect_raw != *detect_stable && *detect_change_count < 4) {
      ++(*detect_change_count);
      if (*detect_change_count >= 4) {
        *detect_stable = detect_raw;
        *detect_change_count = 0;
      }
    }
  } else {
    *detect_last_raw = detect_raw;
    *detect_change_count = 0;
  }

  if (*detect_stable && !*dvi_running) {
    dvi_start();
    *dvi_running = true;
  } else if (!*detect_stable && *dvi_running) {
    dvi_stop();
    *dvi_running = false;
  }
}

void app_games_run(void) {
  adc_init();
  adc_gpio_init(ADC_GPIO_TOP);
  adc_gpio_init(ADC_GPIO_BOTTOM);

  button_init(BUTTON_PIN);

  mpu6050_i2c_init(MPU_I2C_PORT, MPU_SDA_PIN, MPU_SCL_PIN, 400 * 1000);
  bool mpu_ok = mpu6050_probe_and_wake(MPU_I2C_PORT, MPU6050_ADDR);
  uint32_t mpu_next_retry_ms = to_ms_since_boot(get_absolute_time()) + 500;

  PongGame pong;
  TiltGame tilt;

  pong_game_init(&pong);
  tilt_game_init(&tilt, time_us_32());

  GameMode current_game = GAME_PONG_ADC;
  pong_game_render(&pong);

  dvi_init();
  dvi_set_display_buffer(fb_buffer());

  tft_init();
  tft_blit_from_fb_2x(fb_buffer());

  bool dvi_running = false;
  bool detect_stable = dvi_screen_connected();
  bool detect_last_raw = detect_stable;
  uint8_t detect_change_count = 0;
  uint8_t tft_div = 0;

  if (detect_stable) {
    dvi_start();
    dvi_running = true;
  }

  while (true) {
    sync_dvi_state(&dvi_running, &detect_stable, &detect_last_raw,
                   &detect_change_count);

    if (dvi_running) {
      while (!dvi_vsync_occurred())
        tight_loop_contents();
    } else {
      sleep_ms(16);
    }

    if (button_pressed_event(BUTTON_PIN)) {
      if (current_game == GAME_PONG_ADC) {
        current_game = GAME_TILT_MPU6050;
        tilt_game_init(&tilt, time_us_32());
      } else {
        current_game = GAME_PONG_ADC;
        pong_game_init(&pong);
      }
    }

    int16_t ax = 0, ay = 0, az = 0;
    if (mpu_ok) {
      if (!mpu6050_read_accel(MPU_I2C_PORT, MPU6050_ADDR, &ax, &ay, &az)) {
        mpu_ok = false;
      }
    }

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if (!mpu_ok && now_ms >= mpu_next_retry_ms) {
      mpu_ok = mpu6050_probe_and_wake(MPU_I2C_PORT, MPU6050_ADDR);
      mpu_next_retry_ms = now_ms + 500;
    }

    if (current_game == GAME_PONG_ADC) {
      uint16_t adc_top = adc_read_avg(ADC_GPIO_TOP);
      uint16_t adc_bottom = adc_read_avg(ADC_GPIO_BOTTOM);
      pong_game_update(&pong, adc_top, adc_bottom);
      pong_game_render(&pong);
    } else {
      tilt_game_update(&tilt, mpu_ok, ax, ay);
      tilt_game_render(&tilt, mpu_ok);
    }

    draw_hstx_indicator(detect_stable, dvi_screen_connected_raw());

    if (++tft_div >= 2) {
      tft_div = 0;
      tft_blit_from_fb_2x(fb_buffer());
    }
  }
}
