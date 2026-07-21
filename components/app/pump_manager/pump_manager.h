#ifndef PUMP_MANAGER_H
#define PUMP_MANAGER_H


/**
 * Thư viện của C
 */
#include <stdio.h>
#include <string.h>
#include <cJSON.h>

/**
 * Gọi thư viện của freeRTOS gồm:
 * - freertos/task để gọi task
 * - freertos/semphr để làm việc với semaphore
 */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

/**
 * Gọi thư viện của esp-idf để làm việc với gpio, timer, uart, log
 */
#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <driver/uart.h>
#include <esp_log.h>

/**
 * Thư viện tự khởi tạo:
 * - pump_channel làm việc với từng kênh
 * - as5600 làm việc với cảm biến
 * - tmc2209 làm việc với driver
 * - domain chứa struct dữ liệu của hệ thống
 * - io_config chứa chân io config của mcu
 */
#include "pump_channel.h"
#include "as5600.h"
#include "tmc2209.h"
#include "domain.h"
#include "io_config.h"


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


