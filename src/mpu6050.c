#include "mpu6050.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_WHO_AM_I 0x75
#define MPU6050_REG_ACCEL_XOUT_H 0x3B

static bool mpu_read_reg(i2c_inst_t *i2c, uint8_t addr, uint8_t reg,
                         uint8_t *value) {
  if (i2c_write_blocking(i2c, addr, &reg, 1, true) != 1)
    return false;
  return i2c_read_blocking(i2c, addr, value, 1, false) == 1;
}

static bool mpu_write_reg(i2c_inst_t *i2c, uint8_t addr, uint8_t reg,
                          uint8_t value) {
  uint8_t pkt[2] = {reg, value};
  return i2c_write_blocking(i2c, addr, pkt, 2, false) == 2;
}

void mpu6050_i2c_init(i2c_inst_t *i2c, uint sda_pin, uint scl_pin, uint baud_hz) {
  i2c_init(i2c, baud_hz);
  gpio_set_function(sda_pin, GPIO_FUNC_I2C);
  gpio_set_function(scl_pin, GPIO_FUNC_I2C);
  gpio_pull_up(sda_pin);
  gpio_pull_up(scl_pin);
}

bool mpu6050_probe_and_wake(i2c_inst_t *i2c, uint8_t addr) {
  uint8_t who = 0;

  if (!mpu_read_reg(i2c, addr, MPU6050_REG_WHO_AM_I, &who))
    return false;

  if ((who & 0x7e) != 0x68)
    return false;

  if (!mpu_write_reg(i2c, addr, MPU6050_REG_PWR_MGMT_1, 0x00))
    return false;

  sleep_ms(10);
  return true;
}

bool mpu6050_read_accel(i2c_inst_t *i2c, uint8_t addr, int16_t *ax, int16_t *ay,
                        int16_t *az) {
  uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
  uint8_t data[6];

  if (i2c_write_blocking(i2c, addr, &reg, 1, true) != 1)
    return false;
  if (i2c_read_blocking(i2c, addr, data, 6, false) != 6)
    return false;

  int16_t raw_x = (int16_t)((data[0] << 8) | data[1]);
  int16_t raw_y = (int16_t)((data[2] << 8) | data[3]);
  int16_t raw_z = (int16_t)((data[4] << 8) | data[5]);

  // Rotate accelerometer frame 90 degrees counter-clockwise:
  // (x, y) -> (-y, x)
  int32_t rot_x = -(int32_t)raw_y;
  if (rot_x > 32767)
    rot_x = 32767;
  if (rot_x < -32768)
    rot_x = -32768;

  *ax = (int16_t)rot_x;
  *ay = raw_x;
  *az = raw_z;
  return true;
}
