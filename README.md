# Proton display + game setup

This project supports:
- **DVI over HSTX** (via HDMI connector wiring)
- **SPI TFT** mirror display (ILI9341)
- **Tilt test app** (MPU6050 bring-up)
- **Games app** (ADC pong + MPU tilt game)

## Build targets

- Tilt test (default):
  ```bash
  pio run -e proton -t upload
  ```
- Games app:
  ```bash
  pio run -e proton_games -t upload
  ```

---

## SPI TFT wiring (ILI9341)

Use SPI0:
- `CS` -> `GPIO1`
- `SCK` -> `GPIO2`
- `MOSI` -> `GPIO3`
- `DC` -> `GPIO22`
- `RESET` -> `GPIO23`
- `VCC` -> `3.3V`
- `GND` -> `GND`

Optional:
- `MISO` -> `GPIO0` (if needed)
- `BL/LED` -> `3.3V` (or controlled pin/circuit)

---

## HSTX / DVI / HDMI wiring

HSTX pins used by the firmware are `GPIO12..GPIO19`.

### RP2350 GPIO -> HDMI TMDS
- `GPIO12` -> TMDS Data0+ (HDMI pin **7**)
- `GPIO13` -> TMDS Data0- (HDMI pin **9**)
- `GPIO14` -> TMDS Clock+ (HDMI pin **10**)
- `GPIO15` -> TMDS Clock- (HDMI pin **12**)
- `GPIO16` -> TMDS Data2+ (HDMI pin **1**)
- `GPIO17` -> TMDS Data2- (HDMI pin **3**)
- `GPIO18` -> TMDS Data1+ (HDMI pin **4**)
- `GPIO19` -> TMDS Data1- (HDMI pin **6**)

### HDMI grounds
Connect board GND to HDMI pins:
- **2, 5, 8, 11, 17**

### Screen detect (HPD)
- Firmware detect pin: `GPIO11`
- HDMI HPD pin: **19** (**5V logic from monitor**)
- Do **not** connect HPD directly to GPIO.
- Use divider, e.g.:
  - HPD -> **10k** -> GPIO11
  - GPIO11 -> **20k** -> GND

---

## MPU6050 wiring

- `SDA` -> `GPIO4`
- `SCL` -> `GPIO5`
- `VCC` -> `3.3V`
- `GND` -> `GND`

Button input:
- `GPIO26` (active-low, internal pull-up enabled)

---

## Code structure note (MPU driver shared)

Yes: the **tilt test** and **games app** use the same MPU driver module:
- `src/mpu6050.c`
- `src/mpu6050.h`

So if you change MPU behavior in that driver, both apps pick up the change.
