#ifndef BUTTON_H
#define BUTTON_H

#include "pico/types.h"
#include <stdbool.h>
#include <stdint.h>

void button_init(uint pin);
bool button_pressed_event(uint pin);

#endif