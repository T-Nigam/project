#include "framebuffer.h"
#include <string.h>

uint8_t fb_pixels[FB_SIZE_BYTES] __attribute__((aligned(4)));

uint8_t *fb_buffer(void) { return fb_pixels; }

void fb_clear(uint8_t color) { memset(fb_pixels, color, FB_SIZE_BYTES); }

void fb_fill_rect(int x, int y, int w, int h, uint8_t color) {
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
    memset(&fb_pixels[(y + row) * FB_WIDTH + x], color, (size_t)w);
  }
}
