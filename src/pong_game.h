#ifndef PONG_GAME_H
#define PONG_GAME_H

#include <stdint.h>

typedef struct {
  int top_paddle_x;
  int bottom_paddle_x;
  int ball_x;
  int ball_y;
  int ball_vx;
  int ball_vy;
  uint8_t top_score;
  uint8_t bottom_score;
} PongGame;

void pong_game_init(PongGame *g);
void pong_game_update(PongGame *g, uint16_t adc_top, uint16_t adc_bottom);
void pong_game_render(const PongGame *g);

#endif