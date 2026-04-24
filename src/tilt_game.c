#include "tilt_game.h"
#include "framebuffer.h"

#define TILT_PLAYER_SIZE 14
#define TILT_TARGET_SIZE 12
#define TILT_ENEMY_SIZE 12

#define COLOR_TILT_BG rgb332(10, 6, 15)
#define COLOR_MID rgb332(70, 70, 70)
#define COLOR_TILT_PLAYER rgb332(0, 255, 255)
#define COLOR_TILT_TARGET rgb332(255, 255, 0)
#define COLOR_TILT_ENEMY rgb332(255, 60, 60)
#define COLOR_TILT_BAR rgb332(80, 180, 255)
#define COLOR_SCORE rgb332(255, 220, 0)
#define COLOR_WARN rgb332(255, 0, 0)

static inline int clampi(int v, int lo, int hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

static uint32_t rng_next(TiltGame *g) {
  uint32_t x = g->rng;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  g->rng = x;
  return x;
}

static void tilt_spawn_target(TiltGame *g) {
  int max_x = FB_WIDTH - TILT_TARGET_SIZE - 6;
  int max_y = FB_HEIGHT - TILT_TARGET_SIZE - 30;
  g->target_x = 3 + (int)(rng_next(g) % (uint32_t)max_x);
  g->target_y = 24 + (int)(rng_next(g) % (uint32_t)max_y);
}

void tilt_game_init(TiltGame *g, uint32_t seed) {
  g->rng = seed ? seed : 0x12345678u;
  g->player_x = FB_WIDTH / 2 - TILT_PLAYER_SIZE / 2;
  g->player_y = FB_HEIGHT / 2 - TILT_PLAYER_SIZE / 2;

  // Three moving enemies with different paths.
  g->enemy_x[0] = FB_WIDTH / 3;
  g->enemy_y[0] = FB_HEIGHT / 3;
  g->enemy_vx[0] = 3;
  g->enemy_vy[0] = 2;

  g->enemy_x[1] = FB_WIDTH * 2 / 3;
  g->enemy_y[1] = FB_HEIGHT / 4;
  g->enemy_vx[1] = -2;
  g->enemy_vy[1] = 3;

  g->enemy_x[2] = FB_WIDTH / 2;
  g->enemy_y[2] = FB_HEIGHT * 2 / 3;
  g->enemy_vx[2] = 4;
  g->enemy_vy[2] = -2;

  g->ax_f = 0;
  g->ay_f = 0;
  g->score = 0;
  g->time_frames = 60u * 45u;
  tilt_spawn_target(g);
}

void tilt_game_update(TiltGame *g, bool mpu_ok, int16_t ax, int16_t ay) {
  if (!mpu_ok) {
    ax = 0;
    ay = 0;
  }

  g->ax_f = (int16_t)(((int32_t)g->ax_f * 3 + ax) / 4);
  g->ay_f = (int16_t)(((int32_t)g->ay_f * 3 + ay) / 4);

  // Match tilt-test orientation:
  // +AY => move right, +AX => move down
  // Triple movement speed vs previous tuning.
  int dx = clampi((int)(g->ay_f / 2500), -6, 6) * 3;
  int dy = clampi((int)(g->ax_f / 2500), -6, 6) * 3;

  g->player_x = clampi(g->player_x + dx, 0, FB_WIDTH - TILT_PLAYER_SIZE);
  g->player_y = clampi(g->player_y + dy, 20, FB_HEIGHT - TILT_PLAYER_SIZE);

  for (int i = 0; i < TILT_ENEMY_COUNT; ++i) {
    g->enemy_x[i] += g->enemy_vx[i];
    g->enemy_y[i] += g->enemy_vy[i];

    if (g->enemy_x[i] <= 0 || g->enemy_x[i] + TILT_ENEMY_SIZE >= FB_WIDTH) {
      g->enemy_vx[i] = -g->enemy_vx[i];
      g->enemy_x[i] = clampi(g->enemy_x[i], 0, FB_WIDTH - TILT_ENEMY_SIZE);
    }
    if (g->enemy_y[i] <= 20 || g->enemy_y[i] + TILT_ENEMY_SIZE >= FB_HEIGHT) {
      g->enemy_vy[i] = -g->enemy_vy[i];
      g->enemy_y[i] = clampi(g->enemy_y[i], 20, FB_HEIGHT - TILT_ENEMY_SIZE);
    }
  }

  bool hit_target =
      (g->player_x < g->target_x + TILT_TARGET_SIZE) &&
      (g->player_x + TILT_PLAYER_SIZE > g->target_x) &&
      (g->player_y < g->target_y + TILT_TARGET_SIZE) &&
      (g->player_y + TILT_PLAYER_SIZE > g->target_y);

  if (hit_target) {
    if (g->score < 999)
      ++g->score;
    tilt_spawn_target(g);
  }

  bool hit_enemy = false;
  for (int i = 0; i < TILT_ENEMY_COUNT; ++i) {
    bool hit = (g->player_x < g->enemy_x[i] + TILT_ENEMY_SIZE) &&
               (g->player_x + TILT_PLAYER_SIZE > g->enemy_x[i]) &&
               (g->player_y < g->enemy_y[i] + TILT_ENEMY_SIZE) &&
               (g->player_y + TILT_PLAYER_SIZE > g->enemy_y[i]);
    if (hit) {
      hit_enemy = true;
      break;
    }
  }

  if (hit_enemy) {
    if (g->score > 0)
      --g->score;
    g->player_x = FB_WIDTH / 2 - TILT_PLAYER_SIZE / 2;
    g->player_y = FB_HEIGHT / 2 - TILT_PLAYER_SIZE / 2;
  }

  if (g->time_frames > 0)
    --g->time_frames;
  else
    tilt_game_init(g, g->rng);
}

void tilt_game_render(const TiltGame *g, bool mpu_ok) {
  fb_clear(COLOR_TILT_BG);

  fb_fill_rect(0, 20, FB_WIDTH, 2, COLOR_MID);
  fb_fill_rect(0, FB_HEIGHT - 2, FB_WIDTH, 2, COLOR_MID);
  fb_fill_rect(0, 20, 2, FB_HEIGHT - 20, COLOR_MID);
  fb_fill_rect(FB_WIDTH - 2, 20, 2, FB_HEIGHT - 20, COLOR_MID);

  int score_bar = (int)((g->score % 100) * 3);
  fb_fill_rect(8, 4, 300, 10, COLOR_MID);
  fb_fill_rect(8, 4, score_bar, 10, COLOR_TILT_BAR);

  int time_bar = (int)((g->time_frames * 300u) / (60u * 45u));
  fb_fill_rect(FB_WIDTH - 308, 14, 300, 4, COLOR_MID);
  fb_fill_rect(FB_WIDTH - 308, 14, time_bar, 4, COLOR_SCORE);

  fb_fill_rect(g->target_x, g->target_y, TILT_TARGET_SIZE, TILT_TARGET_SIZE,
               COLOR_TILT_TARGET);

  for (int i = 0; i < TILT_ENEMY_COUNT; ++i) {
    fb_fill_rect(g->enemy_x[i], g->enemy_y[i], TILT_ENEMY_SIZE, TILT_ENEMY_SIZE,
                 COLOR_TILT_ENEMY);
  }

  fb_fill_rect(g->player_x, g->player_y, TILT_PLAYER_SIZE, TILT_PLAYER_SIZE,
               COLOR_TILT_PLAYER);

  if (!mpu_ok)
    fb_fill_rect(FB_WIDTH - 24, 2, 20, 16, COLOR_WARN);
}
