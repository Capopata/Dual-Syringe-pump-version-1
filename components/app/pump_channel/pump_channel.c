#include "pump_channel.h"
#include "esp_attr.h"      // Bắt buộc để dùng IRAM_ATTR
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"   // Để dùng esp_rom_delay_us
#include <math.h>

static const char *TAG = "PUMP_CH";
portMUX_TYPE motor_mux = portMUX_INITIALIZER_UNLOCKED;

bool IRAM_ATTR stepper_isr_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    stepper_hw_t *hw = (stepper_hw_t *)user_ctx;

    // 1. Xuất xung STEP
    gpio_set_level(hw->step_pin, 1);
    esp_rom_delay_us(2); // Tăng lên 2us cho chắc
    gpio_set_level(hw->step_pin, 0);

    hw->profile.current_pos++;
    // 2. Nạp giá trị cho bước tiếp theo (ĐỂ CHẠY S-CURVE)
    uint32_t interval = hw->current_interval;
    if (interval < 50) interval = 50; // Giới hạn tốc độ tối đa (~20kHz)

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = interval,
        .flags.auto_reload_on_alarm = true // Phải là false để ISR nạp lại giá trị mới
    };
    gptimer_set_alarm_action(timer, &alarm_config);

    // 3. Cập nhật interval cho lần ngắt tới
    hw->current_interval = hw->next_interval;

    hw->time_accumulator += interval; 
    
    if (hw->time_accumulator >= 10000) { 
        hw->time_accumulator = 0; // Reset bộ đếm thời gian
        BaseType_t hp = pdFALSE;
        if(hw->calc_task_handle != NULL) {
            vTaskNotifyGiveFromISR(hw->calc_task_handle, &hp);
            return hp == pdTRUE; // Yêu cầu FreeRTOS switch context ngay lập tức
        }
    }
    return false;
}
static void gpio_motor_init(stepper_hw_t *hw)
{
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask = (1ULL << hw->step_pin) | 
                        (1ULL << hw->dir_pin) | 
                        (1ULL << hw->en_pin),
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Trạng thái mặc định
    gpio_set_level(hw->en_pin, 1); // Disable motor lúc mới init
    gpio_set_level(hw->step_pin, 0);
}

static void step_timer_init(stepper_hw_t *hw)
{
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,   // 1 tick = 1 us
    };

    // Tìm và tạo timer mới (Tự động gán group/id trống)
    // Lưu ý: ESP32 có 4 timer, nếu bạn dùng nhiều motor hãy cẩn thận số lượng
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &hw->timer_handle));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = stepper_isr_callback, // Sử dụng ISR đã viết ở bước trước
    };

    // QUAN TRỌNG: Truyền hw vào làm user_ctx để ISR biết đang điều khiển motor nào
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(hw->timer_handle, &cbs, hw));

    ESP_ERROR_CHECK(gptimer_enable(hw->timer_handle));
}
static void pump_step_calc_task(void *pvParameters) {
    stepper_hw_t *hw = (stepper_hw_t *)pvParameters;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // 1. ĐIỀU KIỆN DỪNG CỨNG: Kiểm tra kịch kim hành trình
        if (hw->profile.current_pos >= hw->profile.target_pos) {
            gptimer_stop(hw->timer_handle);
            hw->stats->state = PUMP_DONE;
            hw->next_interval = 0; // Cắt hoàn toàn xung
            
            // Ép vị trí hiện tại bằng đúng mục tiêu để tránh sai số hiển thị
            hw->stats->current_steps = hw->profile.target_pos;
            hw->stats->volume_infused = (float)hw->profile.target_pos / 7701955.0f;
            continue; // Bỏ qua phần tính toán bên dưới
        }

        // 2. Tính toán S-Curve bình thường
        uint32_t interval = profile_compute_nex_step_interval(&hw->profile);

        // Điều kiện dừng mềm (nếu hàm S-Curve trả về 0)
        if (interval == 0) {
            gptimer_stop(hw->timer_handle);
            hw->stats->state = PUMP_DONE;
            hw->next_interval = 0;
        } else {
            hw->next_interval = interval;

            // 3. FIX LỖI THỜI GIAN: Tính thời gian thực tế đã trôi qua
            int64_t current_time_us = esp_timer_get_time();
            hw->stats->time_run = (float)(current_time_us - hw->start_time_us) / 1000000.0f;

            // Cập nhật thông số hiển thị Log
            float current_hz = 1000000.0f / (float)interval;
            hw->stats->flow_actual = (current_hz / 7701955.0f) * 3600.0f;
            hw->stats->current_steps = (uint32_t)hw->profile.current_pos;
            hw->stats->volume_infused = (float)hw->profile.current_pos / 7701955.0f;
        }
    }
}
void motor_start(stepper_hw_t *hw, int step_p, int dir_p, int en_p) {
    if (hw == NULL || hw->stats == NULL) return;

    // 1. Khởi tạo phần cứng (chỉ làm 1 lần)
    if (!(hw->is_initialized)) {
        hw->step_pin = (gpio_num_t)step_p;
        hw->dir_pin  = (gpio_num_t)dir_p;
        hw->en_pin   = (gpio_num_t)en_p;

        gpio_motor_init(hw);
        step_timer_init(hw);
        
        xTaskCreate(pump_step_calc_task, "StepCalc", 4096,
                    (void*)hw, 10, &hw->calc_task_handle);

        hw->notify_div = 4; // Bác để 4 như đã test ổn định
        hw->is_initialized = true;
    }

    gptimer_stop(hw->timer_handle);

    // 2. Chuyển đổi dữ liệu từ UI/Logic sang đơn vị Xung (Steps)
    uint32_t target_steps = converter_ml_to_steps(hw->stats->volume_target);
    float max_v_steps = converter_flow_to_velocity_mms(hw->stats->flow_setpoint) * 128000.0f;
    float accel_steps = hw->stats->acceleration * 128000.0f;

    // 3. TÍNH TOÁN THỜI GIAN DỰ KIẾN (TRAPEZOIDAL MATH)
    if (accel_steps > 0 && max_v_steps > 0) {
        float t_ramp = max_v_steps / accel_steps;           // Thời gian tăng tốc (s)
        float s_ramp = 0.5f * accel_steps * t_ramp * t_ramp; // Quãng đường tăng tốc (steps)

        if (2.0f * s_ramp >= (float)target_steps) {
            // Hình tam giác: Không kịp đạt vận tốc tối đa
            hw->stats->run_time_total = 2.0f * sqrtf((float)target_steps / accel_steps);
        } else {
            // Hình thang chuẩn
            float s_const = (float)target_steps - (2.0f * s_ramp);
            float t_const = s_const / max_v_steps;
            hw->stats->run_time_total = (2.0f * t_ramp) + t_const;
        }
    } else {
        hw->stats->run_time_total = 0;
    }

    // 4. Khởi tạo Profile và Nạp dữ liệu mục tiêu
    profile_init(&hw->profile, accel_steps, max_v_steps);
    hw->profile.current_pos = (long)hw->stats->current_steps;
    profile_move_to(&hw->profile, (long)target_steps);

    // 5. Thiết lập thông số chạy ban đầu
    hw->next_interval = (uint32_t)hw->profile.c0;
    hw->current_interval = hw->next_interval;
    
    // Reset các biến runtime để Log in ra từ đầu
    hw->stats->time_run = 0.0f;
    hw->stats->volume_infused = 0.0f;
    hw->stats->flow_actual = 0.0f;
    hw->stats->state = PUMP_RUN;

    ESP_LOGI(TAG, "Sync: Target %lu steps, Total Time: %.2f s, c0: %lu", 
             target_steps, hw->stats->run_time_total, hw->current_interval);

    // 6. Kích hoạt phần cứng
    gpio_set_level(hw->dir_pin, 1); 
    gpio_set_level(hw->en_pin, 0); // Bật Driver

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = hw->current_interval,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(hw->timer_handle, &alarm_config));
    
    ESP_LOGI(TAG, "Starting Trapezoidal profile...");
    ESP_ERROR_CHECK(gptimer_start(hw->timer_handle));
    hw->start_time_us = esp_timer_get_time();
}
void motor_stop(stepper_hw_t *hw) {
    if (hw == NULL || hw->timer_handle == NULL) return;
    
    // 1. Dừng Timer ngay lập tức để ngắt xung STEP
    gptimer_stop(hw->timer_handle);

    // 2. Ngắt điện cuộn dây motor (Disable Driver)
    // Đối với TMC2209, mức CAO (1) là ngắt điện
    gpio_set_level(hw->en_pin, 1);

    // 3. Cập nhật trạng thái về IDLE
    if (hw->stats != NULL) {
        portENTER_CRITICAL(&motor_mux);
        hw->stats->state = PUMP_IDLE;
        hw->stats->flow_actual = 0.0f;
        hw->stats->velocity = 0.0f;
        portEXIT_CRITICAL(&motor_mux);
    }

    // Lưu ý: Tôi KHÔNG xóa Task (vTaskDelete) ở đây. 
    // Task tính toán sẽ tự động rơi vào trạng thái ngủ (Block) tại lệnh 
    // ulTaskNotifyTake và chờ lần nhấn Start tiếp theo. 
    // Cách này giúp tiết kiệm tài nguyên và tránh phân mảnh RAM.

    ESP_LOGI("PUMP_CH", "Motor Channel %d Stopped", hw->channel_id);
}