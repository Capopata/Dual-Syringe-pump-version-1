#pragma once
#include "freertos/FreeRTOS.h"  
#include "driver/gptimer.h"    
#include <stdbool.h>
#include <stdint.h>

// Config
#define MAX_CHANNEL 2

// pump state
typedef enum{
    PUMP_IDLE = 0,
    PUMP_RUN,
    PUMP_PAUSED,
    PUMP_DONE

} pump_state_t;

//UI SCREEN
typedef enum{
    UI_MENU = 0,
    UI_SETTING,
    UI_RUN
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
    SYS_MODE_SEQUENTIAL       // Liên tiếp: Kênh 1 chạy xong (PUMP_DONE) tự động trigger kênh 2 chạy
} sys_op_mode_t;

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

    // --- Hardware Mapping (Cụ thể cho Step Motor) ---
    uint32_t current_steps;
    uint32_t target_steps;
    uint32_t accel_steps;   // Điểm kết thúc quá trình gia tốc
    uint32_t decel_steps;   // Điểm bắt đầu quá trình giảm tốc

    pump_state_t state;           // Trạng thái tổng quát của kênh
    profile_state_t profile_state; // Trạng thái cụ thể của profile chuyển động
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
    ui_state_t ui;
    
    sys_op_mode_t op_mode;    // Chế độ phối hợp các kênh
    bool is_system_running;   // Flag tổng (hữu ích cho chế độ Đồng thời/Liên tiếp)
} system_state_t;

// ===== API =====
void system_state_init(void);
system_state_t* system_get(void);
void system_reset_channel(uint8_t ch);