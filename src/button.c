#include "button.h"
#include "hardware/gpio.h"

void button_init(uint pin) {
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_IN);
  gpio_pull_up(pin);
}

bool button_pressed_event(uint pin) {
  // Active-low button, debounced.
  static bool stable = true;
  static bool last_raw = true;
  static uint8_t stable_count = 0;

  bool raw = gpio_get(pin);

  if (raw != last_raw) {
    last_raw = raw;
    stable_count = 0;
    return false;
  }

  if (stable_count < 4)
    ++stable_count;

  if (stable_count >= 4 && raw != stable) {
    stable = raw;
    return !stable;
  }

  return false;
}
