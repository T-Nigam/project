#include "pong_game.h"
#include "framebuffer.h"
#include "pico/stdlib.h"
#include <stdbool.h>

#define PADDLE_W 90
#define PADDLE_H 12
#define PADDLE_MARGIN 22
#define BALL_SIZE 10

#define BALL_SPEED_X 3
#define BALL_SPEED_Y 4
#define MAX_BALL_VX 7

#define COLOR_BG rgb332(5, 7, 14)
#define COLOR_MID rgb332(70, 70, 70)
#define COLOR_LEFT rgb332(0, 255, 0)
#define COLOR_RIGHT rgb332(255, 70, 70)
#define COLOR_BALL rgb332(255, 255, 255)
#define COLOR_SCORE rgb332(255, 220, 0)

static inline int clampi(int v, int lo, int hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

static int paddle_x_from_adc(uint16_t sample) {
  uint32_t range = (uint32_t)(FB_WIDTH - PADDLE_W);
  return (int)(((uint32_t)sample * range) / 4095u);
}

static void pong_reset_ball(PongGame *g, bool serve_down) {
  g->ball_x = (FB_WIDTH - BALL_SIZE) / 2;
  g->ball_y = (FB_HEIGHT - BALL_SIZE) / 2;
  g->ball_vy = serve_down ? BALL_SPEED_Y : -BALL_SPEED_Y;

  uint32_t r = time_us_32();
  int vx = BALL_SPEED_X + (int)(r % 3u);
  g->ball_vx = (r & 1u) ? vx : -vx;
}

void pong_game_init(PongGame *g) {
  g->top_paddle_x = (FB_WIDTH - PADDLE_W) / 2;
  g->bottom_paddle_x = (FB_WIDTH - PADDLE_W) / 2;
  g->top_score = 0;
  g->bottom_score = 0;
  pong_reset_ball(g, true);
}

void pong_game_update(PongGame *g, uint16_t adc_top, uint16_t adc_bottom) {
  g->top_paddle_x = paddle_x_from_adc(adc_top);
  g->bottom_paddle_x = paddle_x_from_adc(adc_bottom);

  g->ball_x += g->ball_vx;
  g->ball_y += g->ball_vy;

  if (g->ball_x <= 0) {
    g->ball_x = 0;
    g->ball_vx = -g->ball_vx;
  } else if (g->ball_x + BALL_SIZE >= FB_WIDTH) {
    g->ball_x = FB_WIDTH - BALL_SIZE;
    g->ball_vx = -g->ball_vx;
  }

  const int top_y = PADDLE_MARGIN;
  const int bottom_y = FB_HEIGHT - PADDLE_MARGIN - PADDLE_H;

  if (g->ball_vy < 0 && g->ball_y <= top_y + PADDLE_H &&
      g->ball_y + BALL_SIZE >= top_y && g->ball_x + BALL_SIZE >= g->top_paddle_x &&
      g->ball_x <= g->top_paddle_x + PADDLE_W) {
    g->ball_y = top_y + PADDLE_H;
    g->ball_vy = BALL_SPEED_Y;

    int paddle_center = g->top_paddle_x + PADDLE_W / 2;
    int ball_center = g->ball_x + BALL_SIZE / 2;
    g->ball_vx += (ball_center - paddle_center) / 16;
    g->ball_vx = clampi(g->ball_vx, -MAX_BALL_VX, MAX_BALL_VX);
    if (g->ball_vx == 0)
      g->ball_vx = -1;
  }

  if (g->ball_vy > 0 && g->ball_y + BALL_SIZE >= bottom_y &&
      g->ball_y <= bottom_y + PADDLE_H && g->ball_x + BALL_SIZE >= g->bottom_paddle_x &&
      g->ball_x <= g->bottom_paddle_x + PADDLE_W) {
    g->ball_y = bottom_y - BALL_SIZE;
    g->ball_vy = -BALL_SPEED_Y;

    int paddle_center = g->bottom_paddle_x + PADDLE_W / 2;
    int ball_center = g->ball_x + BALL_SIZE / 2;
    g->ball_vx += (ball_center - paddle_center) / 16;
    g->ball_vx = clampi(g->ball_vx, -MAX_BALL_VX, MAX_BALL_VX);
    if (g->ball_vx == 0)
      g->ball_vx = 1;
  }

  if (g->ball_y + BALL_SIZE < 0) {
    if (g->bottom_score < 99)
      g->bottom_score++;
    pong_reset_ball(g, true);
  } else if (g->ball_y > FB_HEIGHT) {
    if (g->top_score < 99)
      g->top_score++;
    pong_reset_ball(g, false);
  }
}

void pong_game_render(const PongGame *g) {
  fb_clear(COLOR_BG);

  for (int y = 0; y < FB_HEIGHT; y += 24) {
    fb_fill_rect(FB_WIDTH / 2 - 2, y, 4, 14, COLOR_MID);
  }

  fb_fill_rect(g->top_paddle_x, PADDLE_MARGIN, PADDLE_W, PADDLE_H, COLOR_LEFT);
  fb_fill_rect(g->bottom_paddle_x, FB_HEIGHT - PADDLE_MARGIN - PADDLE_H, PADDLE_W,
               PADDLE_H, COLOR_RIGHT);

  fb_fill_rect(g->ball_x, g->ball_y, BALL_SIZE, BALL_SIZE, COLOR_BALL);

  int top_pips = g->top_score > 20 ? 20 : g->top_score;
  int bottom_pips = g->bottom_score > 20 ? 20 : g->bottom_score;

  for (int i = 0; i < top_pips; ++i) {
    fb_fill_rect(10 + i * 8, 10, 6, 12, COLOR_SCORE);
  }
  for (int i = 0; i < bottom_pips; ++i) {
    fb_fill_rect(10 + i * 8, FB_HEIGHT - 22, 6, 12, COLOR_SCORE);
  }
}
