#ifndef DISPLAY_H
#define DISPLAY_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480

static inline uint8_t rgb332(uint8_t r, uint8_t g, uint8_t b) {
    return (r & 0xE0) | ((g & 0xE0) >> 3) | (b >> 6);
}

#define COLOR_BLACK     rgb332(0,   0,   0)
#define COLOR_WHITE     rgb332(255, 255, 255)
#define COLOR_RED       rgb332(255, 0,   0)
#define COLOR_GREEN     rgb332(0,   255, 0)
#define COLOR_BLUE      rgb332(0,   0,   255)
#define COLOR_YELLOW    rgb332(255, 255, 0)
#define COLOR_CYAN      rgb332(0,   255, 255)
#define COLOR_MAGENTA   rgb332(255, 0,   255)
#define COLOR_GRAY      rgb332(128, 128, 128)
#define COLOR_DARK_GRAY rgb332(64,  64,  64)
#define COLOR_ORANGE    rgb332(255, 165, 0)

typedef enum {
    DISPLAY_MODE_HANDHELD,  
    DISPLAY_MODE_DOCKED      
} DisplayMode;

void display_init(void);
void display_clear(uint8_t color);
void display_flip(void);
bool display_is_docked(void);
uint8_t *display_get_back_buffer(void);
void draw_pixel(int x, int y, uint8_t color);
void draw_rect(int x, int y, int w, int h, uint8_t color);
void draw_rect_outline(int x, int y, int w, int h, uint8_t color);
void draw_hline(int x, int y, int w, uint8_t color);
void draw_vline(int x, int y, int h, uint8_t color);
void draw_line(int x0, int y0, int x1, int y1, uint8_t color);
void draw_circle(int cx, int cy, int r, uint8_t color);
void draw_circle_filled(int cx, int cy, int r, uint8_t color);
void draw_sprite(int x, int y, int w, int h, const uint8_t *data);
void draw_sprite_transparent(int x, int y, int w, int h,
                              const uint8_t *data, uint8_t transparent_color);
                              void draw_char(int x, int y, char c, uint8_t color);
void draw_string(int x, int y, const char *str, uint8_t color);

#endif