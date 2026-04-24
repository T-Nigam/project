#ifndef TILT_GAME_H
#define TILT_GAME_H

#include <stdbool.h>
#include <stdint.h>

#define TILT_ENEMY_COUNT 3

typedef struct {
  int player_x;
  int player_y;
  int target_x;
  int target_y;
  int enemy_x[TILT_ENEMY_COUNT];
  int enemy_y[TILT_ENEMY_COUNT];
  int enemy_vx[TILT_ENEMY_COUNT];
  int enemy_vy[TILT_ENEMY_COUNT];
  int16_t ax_f;
  int16_t ay_f;
  uint16_t score;
  uint16_t time_frames;
  uint32_t rng;
} TiltGame;

void tilt_game_init(TiltGame *g, uint32_t seed);
void tilt_game_update(TiltGame *g, bool mpu_ok, int16_t ax, int16_t ay);
void tilt_game_render(const TiltGame *g, bool mpu_ok);

#endif