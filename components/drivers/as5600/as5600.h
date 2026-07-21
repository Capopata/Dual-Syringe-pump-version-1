#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

       /*!< Address of the MPU9250 sensor */
#define AS5600_STATUS_MD   (1 << 5)
#define AS5600_STATUS_ML   (1 << 4)
#define AS5600_STATUS_MH   (1 << 3)

#define AS5600_SENSOR_ADDR         0x36 
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000                      /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

typedef enum {
    AS5600_REG_ZMCO        = 0x00,
    AS5600_REG_ZPOS_H     = 0x01,
    AS5600_REG_ZPOS_L     = 0x02,
    AS5600_REG_MPOS_H     = 0x03,
    AS5600_REG_MPOS_L     = 0x04,
    AS5600_REG_MANG_H     = 0x05,
    AS5600_REG_MANG_L     = 0x06,
    /**
     * h
     */
    AS5600_REG_CONF_H    = 0x07,
    AS5600_REG_CONF_L    = 0x08,
    AS5600_REG_RAW_ANGLE_H = 0x0C,
    AS5600_REG_RAW_ANGLE_L = 0x0D,
    AS5600_REG_ANGLE_H   = 0x0E,
    AS5600_REG_ANGLE_L   = 0x0F,
    AS5600_REG_STATUS    = 0x0B,
    AS5600_REG_AGC       = 0x1A,
    AS5600_REG_MAGNITUDE_H = 0x1B,
    AS5600_REG_MAGNITUDE_L = 0x1C
} as5600_reg_t;


typedef struct {
    i2c_master_dev_handle_t dev;
}as5600_t;

typedef struct {
    float last_raw_degree;
    float accumulated_angle;
    bool is_started;
    uint16_t last_raw;
} as5600_logic_t;

// Hàm khởi tạo/reset logic
void as5600_logic_reset(as5600_logic_t *logic);

void as5600_process_multi_turn(as5600_t *dev, as5600_logic_t *logic, float *display_angle, bool is_running);
esp_err_t as5600_init(as5600_t *sensor, i2c_master_dev_handle_t dev);
esp_err_t as5600_read_raw_angle(as5600_t *sensor, uint16_t *angle);
esp_err_t as5600_read_angle(as5600_t *sensor, uint16_t *angle);
esp_err_t as5600_config_slow_filter(as5600_t *sensor);
esp_err_t as5600_disable_filter(as5600_t *sensor);
esp_err_t as5600_read_status(as5600_t *sensor, uint8_t *status );
void i2c_master_init(i2c_master_bus_handle_t *bus_handle, 
                     i2c_master_dev_handle_t *dev_handle,
                     i2c_port_t port,
                     int sda, int scl);
void read_angle_task(void *arg);