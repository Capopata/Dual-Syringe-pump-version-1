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

/**
 * @brief Khởi tạo quản lý bơm, liên kết hardware handles với system state
 */
void pump_manager_init(void);

/**
 * @brief Lệnh bắt đầu bơm cho hệ thống (xử lý theo Op Mode)
 */
void pump_manager_system_start(void);

/**
 * @brief Lệnh dừng toàn bộ hệ thống khẩn cấp
 */
void pump_manager_system_stop(void);

/**
 * @brief Bắt đầu một kênh đơn lẻ (Dùng cho mode INDEPENDENT)
 * @param channel_id 0 hoặc 1
 */
void pump_manager_start_channel(uint8_t channel_id);

/**
 * @brief Task quản lý logic liên tầng (Sequential trigger) và giám sát
 */
void pump_manager_task(void *pvParameters);
void pump_manager_home_channel(uint8_t channel_id);
void pump_manager_home_all(void);
void log_task(void *pvParameters);
void sensor_task(void *pvParameters);
void uart_cmd_task(void *pvParameters);

#endif


