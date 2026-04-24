#ifndef TFT_ILI9341_H
#define TFT_ILI9341_H

#include <stdint.h>

void tft_init(void);
void tft_blit_from_fb_2x(const uint8_t *fb);

#endif