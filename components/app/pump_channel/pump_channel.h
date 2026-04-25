#pragma once
#include "driver/gptimer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "trapezoidal_profile.h" // Sử dụng file bạn vừa gửi
#include "unit_converter.h"
#include "domain.h"



// Cấu trúc chân và handle cho mỗi motor
typedef struct {
    gpio_num_t step_pin;
    gpio_num_t dir_pin;
    gpio_num_t en_pin;
    bool dir_inverted;

    gptimer_handle_t timer_handle;

    trapezoidal_profile_t profile;
    pump_channel_t *stats;

    TaskHandle_t calc_task_handle;

    volatile uint32_t current_interval;
    volatile uint32_t next_interval;

    uint32_t time_accumulator;
    uint8_t notify_div;
    uint32_t start_time_us;
    int channel_id;
    bool is_initialized;
} stepper_hw_t;

extern portMUX_TYPE motor_mux;

/**
 * @brief Khởi tạo (nếu cần) và bắt đầu chạy motor
 * @param hw Con trỏ quản lý motor
 * @param step_p  STEP pin
 * @param dir_p  DIR pin
 * @param en_p  EN pin
 */
void motor_start(stepper_hw_t *hw, int step_p, int dir_p, int en_p);
void motor_stop(stepper_hw_t *hw); // Thêm hàm stop để quản lý Task