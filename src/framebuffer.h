#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "dvi.h"
#include <stdint.h>

#define FB_WIDTH DVI_H_ACTIVE_PIXELS
#define FB_HEIGHT DVI_V_ACTIVE_LINES
#define FB_SIZE_BYTES (FB_WIDTH * FB_HEIGHT)

static inline uint8_t rgb332(uint8_t r, uint8_t g, uint8_t b) {
  return (uint8_t)((r & 0xE0) | ((g & 0xE0) >> 3) | (b >> 6));
}

extern uint8_t fb_pixels[FB_SIZE_BYTES];

uint8_t *fb_buffer(void);
void fb_clear(uint8_t color);
void fb_fill_rect(int x, int y, int w, int h, uint8_t color);

#endif