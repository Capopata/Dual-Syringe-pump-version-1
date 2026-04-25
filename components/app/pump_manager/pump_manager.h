#ifndef PUMP_MANAGER_H
#define PUMP_MANAGER_H

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pump_channel.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "domain.h"

#define CH0_STEP_PIN 16
#define CH0_DIR_PIN  4
#define CH0_EN_PIN   19

#define CH1_STEP_PIN 22
#define CH1_DIR_PIN  23
#define CH1_EN_PIN   25

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

#endif