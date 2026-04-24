#ifndef MPU6050_H
#define MPU6050_H

#include "hardware/i2c.h"
#include <stdbool.h>
#include <stdint.h>

#define MPU6050_ADDR 0x68

void mpu6050_i2c_init(i2c_inst_t *i2c, uint sda_pin, uint scl_pin, uint baud_hz);
bool mpu6050_probe_and_wake(i2c_inst_t *i2c, uint8_t addr);
bool mpu6050_read_accel(i2c_inst_t *i2c, uint8_t addr, int16_t *ax, int16_t *ay,
                        int16_t *az);

#endif