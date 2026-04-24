#include "app_tilt_test.h"
#include "button.h"
#include "dvi.h"
#include "framebuffer.h"
#include "mpu6050.h"
#include "pico/stdlib.h"
#include "tft_ili9341.h"

#define BUTTON_PIN 26

#define MPU_I2C_PORT i2c0
#define MPU_SDA_PIN 4
#define MPU_SCL_PIN 5

#define COLOR_BG rgb332(8, 10, 18)
#define COLOR_GRID rgb332(60, 70, 80)
#define COLOR_AXIS_X rgb332(0, 180, 255)
#define COLOR_AXIS_Y rgb332(255, 180, 0)
#define COLOR_DOT rgb332(255, 255, 255)
#define COLOR_GOOD rgb332(0, 255, 0)
#define COLOR_WARN rgb332(255, 90, 0)
#define COLOR_BAD rgb332(255, 0, 0)

static inline int clampi(int v, int lo, int hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
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

static void render_tilt_test(int32_t filt_ax, int32_t filt_ay, int32_t off_ax,
                             int32_t off_ay, bool mpu_online,
                             bool mpu_last_read_ok, bool have_offset,
                             bool hstx_connected, bool hstx_raw) {
  fb_clear(COLOR_BG);

  int cx = FB_WIDTH / 2;
  int cy = FB_HEIGHT / 2;

  fb_fill_rect(cx - 1, 20, 2, FB_HEIGHT - 20, COLOR_GRID);
  fb_fill_rect(0, cy - 1, FB_WIDTH, 2, COLOR_GRID);

  int tx = clampi((int)((filt_ay - off_ay) / 64), -260, 260);
  int ty = clampi((int)((filt_ax - off_ax) / 64), -180, 180);

  int dot_x = clampi(cx + tx - 8, 0, FB_WIDTH - 16);
  int dot_y = clampi(cy + ty - 8, 20, FB_HEIGHT - 16);

  int xbar = clampi(tx, -240, 240);
  if (xbar >= 0)
    fb_fill_rect(cx, 6, xbar, 8, COLOR_AXIS_X);
  else
    fb_fill_rect(cx + xbar, 6, -xbar, 8, COLOR_AXIS_X);

  int ybar = clampi(ty, -160, 160);
  if (ybar >= 0)
    fb_fill_rect(8, cy, 8, ybar, COLOR_AXIS_Y);
  else
    fb_fill_rect(8, cy + ybar, 8, -ybar, COLOR_AXIS_Y);

  fb_fill_rect(dot_x, dot_y, 16, 16, COLOR_DOT);
  fb_fill_rect(dot_x + 4, dot_y + 4, 8, 8, COLOR_BG);

  // Status blocks (left->right): HSTX link, MPU online, read OK, calibrated
  fb_fill_rect(FB_WIDTH - 88, 4, 18, 14,
               hstx_connected ? COLOR_GOOD : COLOR_BAD);
  // Tiny raw detect pin indicator
  fb_fill_rect(FB_WIDTH - 90, 20, 4, 4, hstx_raw ? COLOR_AXIS_X : COLOR_GRID);
  fb_fill_rect(FB_WIDTH - 66, 4, 18, 14, mpu_online ? COLOR_GOOD : COLOR_BAD);
  fb_fill_rect(FB_WIDTH - 44, 4, 18, 14,
               mpu_last_read_ok ? COLOR_GOOD : COLOR_WARN);
  fb_fill_rect(FB_WIDTH - 22, 4, 18, 14, have_offset ? COLOR_GOOD : COLOR_WARN);
}

void app_tilt_test_run(void) {
  button_init(BUTTON_PIN);
  mpu6050_i2c_init(MPU_I2C_PORT, MPU_SDA_PIN, MPU_SCL_PIN, 400 * 1000);

  bool mpu_online = mpu6050_probe_and_wake(MPU_I2C_PORT, MPU6050_ADDR);
  bool mpu_last_read_ok = false;
  uint32_t mpu_next_retry_ms = to_ms_since_boot(get_absolute_time()) + 500;

  int32_t filt_ax = 0;
  int32_t filt_ay = 0;
  int32_t off_ax = 0;
  int32_t off_ay = 0;
  bool have_offset = false;

  dvi_init();
  dvi_set_display_buffer(fb_buffer());

  tft_init();

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

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if (!mpu_online && now_ms >= mpu_next_retry_ms) {
      mpu_online = mpu6050_probe_and_wake(MPU_I2C_PORT, MPU6050_ADDR);
      mpu_next_retry_ms = now_ms + 500;
      mpu_last_read_ok = false;
    }

    int16_t ax = 0, ay = 0, az = 0;
    if (mpu_online) {
      if (mpu6050_read_accel(MPU_I2C_PORT, MPU6050_ADDR, &ax, &ay, &az)) {
        mpu_last_read_ok = true;
      } else {
        mpu_online = false;
        mpu_last_read_ok = false;
      }
    }

    filt_ax = (filt_ax * 3 + ax) / 4;
    filt_ay = (filt_ay * 3 + ay) / 4;

    if (!have_offset) {
      off_ax = filt_ax;
      off_ay = filt_ay;
      have_offset = true;
    }

    if (button_pressed_event(BUTTON_PIN)) {
      off_ax = filt_ax;
      off_ay = filt_ay;
      have_offset = true;
    }

    render_tilt_test(filt_ax, filt_ay, off_ax, off_ay, mpu_online,
                     mpu_last_read_ok, have_offset, detect_stable,
                     dvi_screen_connected_raw());

    if (++tft_div >= 2) {
      tft_div = 0;
      tft_blit_from_fb_2x(fb_buffer());
    }
  }
}
