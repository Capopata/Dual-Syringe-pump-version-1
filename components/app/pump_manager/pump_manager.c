#include "pump_manager.h"

static const char *TAG = "PUMP_MGR";


// Handle quản lý phần cứng cho 2 kênh
static stepper_hw_t motors[MAX_CHANNEL];

void pump_manager_init(void) {
    
    memset(motors, 0, sizeof(motors));
    system_state_t *sys = system_get();
    for (int i = 0; i < MAX_CHANNEL; i++) {
        motors[i].stats = &sys->channels[i];
        motors[i].channel_id = i;
        motors[i].is_initialized = false;
        motors[i].dir_inverted = false; // Tùy chỉnh theo cơ khí
        
        // Khởi tạo trạng thái ban đầu cho channel
        sys->channels[i].state = PUMP_IDLE;
        sys->channels[i].current_steps = 0;
    }
    
    ESP_LOGI(TAG, "Pump Manager Initialized");
}

void pump_manager_start_channel(uint8_t channel_id) {
    if (channel_id >= MAX_CHANNEL) return;
    
    if (channel_id == 0) {
        motor_start(&motors[0], CH0_STEP_PIN, CH0_DIR_PIN, CH0_EN_PIN);
    } else {
        motor_start(&motors[1], CH1_STEP_PIN, CH1_DIR_PIN, CH1_EN_PIN);
    }
    ESP_LOGI(TAG, "Channel %d started", channel_id);
}

void pump_manager_system_start(void) {
    system_state_t *sys = system_get();
    
    switch (sys->op_mode) {
        case SYS_MODE_INDEPENDENT:
            // Chế độ độc lập: Chỉ start kênh đang được chọn trên UI
            pump_manager_start_channel(sys->ui.selected_channel);
            break;

        case SYS_MODE_SIMULTANEOUS:
            // Chế độ đồng thời: Start cả 2 cùng lúc
            sys->is_system_running = true;
            pump_manager_start_channel(0);
            pump_manager_start_channel(1);
            break;

        case SYS_MODE_SEQUENTIAL:
            // Chế độ liên tiếp: Start kênh 0 trước
            sys->is_system_running = true;
            pump_manager_start_channel(0);
            break;
    }
}

void pump_manager_system_stop(void) {
    system_state_t *sys = system_get();
    sys->is_system_running = false;

    for (int i = 0; i < MAX_CHANNEL; i++) {
        // motors[i] là mảng stepper_hw_t trong pump_manager
        if (motors[i].is_initialized) {
            motor_stop(&motors[i]); 
        }
    }
    ESP_LOGI("PUMP_MGR", "EMERGENCY STOP EXECUTED");
}

// Task chạy nền để xử lý logic chuyển kênh trong chế độ Sequential
void pump_manager_task(void *pvParameters) {
    system_state_t *sys = system_get();
    bool ch0_was_running = false;

    while (1) {
        // --- Xử lý logic SEQUENTIAL (Liên tiếp) ---
        if (sys->op_mode == SYS_MODE_SEQUENTIAL && sys->is_system_running) {
            
            // Kiểm tra nếu kênh 0 vừa hoàn thành
            if (sys->channels[0].state == PUMP_DONE && ch0_was_running) {
                ESP_LOGI(TAG, "Channel 0 Done. Sequential trigger: Starting Channel 1");
                pump_manager_start_channel(1);
                ch0_was_running = false; 
            }
            
            if (sys->channels[0].state == PUMP_RUN) {
                ch0_was_running = true;
            }
        }

        // --- Xử lý logic SIMULTANEOUS (Nếu 1 kênh lỗi/dừng thì dừng cả 2) ---
        if (sys->op_mode == SYS_MODE_SIMULTANEOUS && sys->is_system_running) {
            // Nếu một trong 2 kênh chuyển về IDLE hoặc DONE bất thường, dừng toàn bộ
            if (sys->channels[0].state == PUMP_DONE && sys->channels[1].state == PUMP_DONE) {
                sys->is_system_running = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // Kiểm tra mỗi 50ms
    }
}