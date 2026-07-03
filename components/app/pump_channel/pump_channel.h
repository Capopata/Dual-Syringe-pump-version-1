#pragma once
#include "driver/gptimer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "trapezoidal_profile.h" // Sử dụng file bạn vừa gửi
#include "unit_converter.h"
#include "pid.h"
#include "domain.h"
#include "tmc2209.h"
#include "io_config.h"

#define us_to_s 1000000.0f
/**
 * @brief Số vi bước cần thiết để pit-tông tịnh tiến 1 mm (Steps per Millimeter)
 * Cơ khí: Motor 1.8° (200 steps/rev) | Vi bước: 256 | Đai giảm tốc: 5:1 | Vít me T8x2 (Pitch = 2mm)
 * Công thức: (200 * 256 * 5) / 2 = 128000
 */
#define STEPS_PER_MM 128000.0f
#define HOMING_STEPS  5000000UL   // = 1 xi lanh đầy, chạy ngược về gốc

// PID interval limits (µs): giới hạn mức PID được phép hiệu chỉnh interval
// PID output dương → motor chậm lại (interval tăng)
// PID output âm   → motor nhanh hơn (interval giảm)
#define PID_INTERVAL_CORR_MAX   5000.0f // Tối đa +5000 µs (làm chậm tối đa)
#define PID_INTERVAL_CORR_MIN  -5000.0f // Tối đa -5000 µs (làm nhanh tối đa)

// Giới hạn tuyệt đối của interval sau correction để bảo vệ phần cứng
#define INTERVAL_ABS_MIN        200     // µs (~5000 step/s, tuỳ driver)
#define INTERVAL_ABS_MAX        1000000 // µs (1 step/s)
// Cấu trúc chân và handle cho mỗi motor
typedef struct {
    gpio_num_t step_pin;
    gpio_num_t dir_pin;
    gpio_num_t en_pin;
    

    gptimer_handle_t timer_handle;

    trapezoidal_profile_t profile;
    
    PIDcontroller pid;

    volatile uint32_t current_interval;
    volatile uint32_t next_interval;

    pump_channel_t *stats;
    TaskHandle_t calc_task_handle;

    int channel_id;
    bool is_initialized;

    uint8_t notify_div;

    uint64_t start_time_us;
    volatile bool pid_update_ready;
    // volatile float current_pid_correction;
    
} stepper_hw_t;

extern portMUX_TYPE motor_mux;

void motor_prepare(stepper_hw_t *hw, int step_p, int dir_p, int en_p);
void motor_fire(stepper_hw_t *hw);
void motor_stop(stepper_hw_t *hw); // Thêm hàm stop để quản lý Task
void motor_home(stepper_hw_t *hw);