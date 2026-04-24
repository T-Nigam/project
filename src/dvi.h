#ifndef DVI_H
#define DVI_H
#include <stdint.h>
#include <stdbool.h>

#define DVI_GPIO_FIRST 12
#define DVI_GPIO_LAST 19

#define DOCK_DETECT_PIN 11

#ifndef DVI_DETECT_ACTIVE_LOW
#define DVI_DETECT_ACTIVE_LOW 0
#endif

#define DVI_H_FRONT_PORCH 16
#define DVI_H_SYNC_WIDTH 96
#define DVI_H_BACK_PORCH 48
#define DVI_H_ACTIVE_PIXELS 640

#define DVI_V_FRONT_PORCH 10
#define DVI_V_SYNC_WIDTH 2
#define DVI_V_BACK_PORCH 33
#define DVI_V_ACTIVE_LINES 480

#define DVI_H_TOTAL_PIXELS (DVI_H_FRONT_PORCH + DVI_H_SYNC_WIDTH + DVI_H_BACK_PORCH + DVI_H_ACTIVE_PIXELS)
#define DVI_V_TOTAL_LINES (DVI_V_FRONT_PORCH + DVI_V_SYNC_WIDTH + DVI_V_BACK_PORCH + DVI_V_ACTIVE_LINES)

void dvi_init(void);
void dvi_start(void);
void dvi_stop(void);
void dvi_set_display_buffer(uint8_t *buf);
bool dvi_vsync_occurred(void);

// Screen-detect input (HPD/dock detect) on DOCK_DETECT_PIN.
// Returns the current raw GPIO state (before polarity mapping).
bool dvi_screen_connected_raw(void);
// Returns true when a display is present (after polarity mapping).
bool dvi_screen_connected(void);

#endif 