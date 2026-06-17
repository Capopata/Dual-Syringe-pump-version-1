#pragma once
#include "freertos/FreeRTOS.h"  
#include "driver/gptimer.h"   
#include "freertos/task.h" 
#include <stdbool.h>
#include <stdint.h>

// Config
#define MAX_CHANNEL 2

// pump state
typedef enum{
    PUMP_IDLE = 0,
    PUMP_RUN,
    PUMP_PAUSED,
    PUMP_DONE,
    PUMP_HOMING,
    PUMP_ERROR,

} pump_state_t;

//UI SCREEN
typedef enum{
    UI_MENU = 0,
    UI_SETTING,
    UI_RUN, 
    UI_SELECT_CHANNEL,
}ui_screen_t;   

//profile state
typedef enum {
    PROFILE_IDLE = 0,
    PROFILE_ACCEL,
    PROFILE_CONST,
    PROFILE_DECEL
} profile_state_t;
// SYSTEM OPERATION MODE
typedef enum {
    SYS_MODE_INDEPENDENT = 0, // Đơn lẻ: Các kênh Start/Stop/Config hoàn toàn độc lập
    SYS_MODE_SIMULTANEOUS,    // Đồng thời: Bấm 1 nút Start chạy cả 2 kênh, 1 kênh lỗi dừng cả 2
    SYS_MODE_SEQUENTIAL,       // Liên tiếp: Kênh 1 chạy xong (PUMP_DONE) tự động trigger kênh 2 chạy
    SYS_MODE_HOMING,
} sys_op_mode_t;

typedef enum{
    EVENT_UP = 0,
    EVENT_DOWN,
    EVENT_LEFT, 
    EVENT_RIGHT,
    EVENT_SELECT
}system_event_t;
typedef enum{
    ALGO_TRAPEZOIDAL = 0,
    ALGO_TRAP_PID,
}pump_algorithm_t;
typedef struct {
    float alpha;
    float current_value;
    bool is_init;
} ema_filter_t;
// CHANNEL STATE
typedef struct {
    // --- Configuration (Thông số cài đặt) ---
    float volume_target;  // ml
    float flow_setpoint;  //ml/h
    
    // --- Motion Profile Settings ---
    float target_velocity;  // max velocity (mm/s)
    float acceleration;     // for smooth (mm/s^2)

    // --- Runtime Status (Trạng thái khi đang chạy) ---
    float volume_infused;   //ml
    float flow_actual;     //ml/h
    float velocity;         //mm/s
    float time_run;         // time ran (s)
    float run_time_total;   // Total time (s)

    pump_algorithm_t algorithm;
    
    // --- Hardware Mapping (Cụ thể cho Step Motor) ---
    uint32_t current_steps;
    uint32_t target_steps;
    uint32_t accel_steps;   // Điểm kết thúc quá trình gia tốc
    uint32_t decel_steps;   // Điểm bắt đầu quá trình giảm tốc

    pump_state_t state;           // Trạng thái tổng quát của kênh
    profile_state_t profile_state; // Trạng thái cụ thể của profile chuyển động
    bool is_running;
    volatile int32_t last_sensor_raw;
    volatile int32_t prev_sensor_raw;
    int32_t raw_threshold;

    float    volume_commanded;   // Từ step counter
    float    flow_commanded;     // Từ interval

    // Nguồn từ AS5600 (dùng để hiển thị / feedback)
    float    volume_measured;    // Từ AS5600
    float    flow_measured;      // Từ AS5600
    float    sensor_angle;       // Raw angle tích lũy
    float kp;
    float ki;
    float kd;

    float offset;
} pump_channel_t;

// UI STATE
typedef struct {
    ui_screen_t screen;
    uint8_t selected_channel;
    float editing_value;
    bool is_editing;        
} ui_state_t;

// SYSTEM STATE
typedef struct {
    pump_channel_t channels[MAX_CHANNEL];
    system_event_t sys_evt;
    ui_state_t ui;
    TaskHandle_t manager_task_handle;
    sys_op_mode_t op_mode;    // Chế độ phối hợp các kênh
    bool is_system_running;   // Flag tổng (hữu ích cho chế độ Đồng thời/Liên tiếp)
} system_state_t;

// ===== API =====
/**
 * @brief Khởi tạo trạng thái mặc định của hệ thống, set các giá trị về 0
 */
void system_state_init(void);

/**
 * @brief truy cập vào biến g_system_state thông qua con trỏ
 * @return Trả về con trỏ trỏ vào biến g_system_state
 */
system_state_t* system_get(void);

/**
 * @brief Reset các biến trong các kênh về trạng thái ban đầu
 */
void system_reset_channel(uint8_t ch);