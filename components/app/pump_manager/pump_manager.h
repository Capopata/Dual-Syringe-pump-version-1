#ifndef PUMP_MANAGER_H
#define PUMP_MANAGER_H

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "pump_channel.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "as5600.h"
#include "tmc2209.h"
#include "domain.h"
#include "pcf8574.h"
#include "io_config.h"


extern SemaphoreHandle_t i2c_bus_ch0_mutex;
extern SemaphoreHandle_t sys_state_mutex;

/**
 * @brief Khởi tạo quản lý bơm, liên kết hardware handles với system state
 */
esp_err_t pump_manager_init(void);

/**
 * @brief Lệnh bắt đầu bơm cho hệ thống (xử lý theo Op Mode)
 */
void pump_manager_system_start(void);

/**
 * @brief Lệnh dừng toàn bộ hệ thống khẩn cấp
 */
void pump_manager_system_stop(void);
void pump_manager_home_channel(uint8_t channel_id);

#endif


