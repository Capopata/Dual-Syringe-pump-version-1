#include "pump_channel.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include <math.h>

static const char *TAG = "PUMP_CH";
portMUX_TYPE motor_mux = portMUX_INITIALIZER_UNLOCKED;
#define STEPS_PER_NOTIFY  10  // Notify mỗi 10 steps

static void motor_handle_done(stepper_hw_t *hw, system_state_t *sys);
static void gpio_motor_init(stepper_hw_t *hw){

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask = (1ULL<<hw->step_pin)|
                        (1ULL<<hw->dir_pin)|
                        (1ULL<<hw->en_pin),
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    //set state of stepper 
    gpio_set_level(hw->en_pin, 1);
}

static void step_timer_init(stepper_hw_t *hw){
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT, //nguồn xung
        .direction = GPTIMER_COUNT_UP, // count up
        .resolution_hz = 1000000, // 1tik = 1us
    };

    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &hw->timer_handle));

    gptimer_event_callbacks_t cb = {
        .on_alarm = stepper_isr_callback,
    };

    ESP_ERROR_CHECK(gptimer_register_event_callbacks(hw->timer_handle, &cb, hw));

    ESP_ERROR_CHECK(gptimer_enable(hw->timer_handle));
}

static bool IRAM_ATTR stepper_isr_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *data,
    void *user_ctx)
{
    stepper_hw_t *hw = (stepper_hw_t *)user_ctx;

    gpio_set_level(hw->step_pin, 1);
    gpio_set_level(hw->step_pin, 0);
    
    hw->profile.current_pos++; 

    // Áp dụng khoảng thời gian (interval) tiếp theo đã được calc_task tính toán sẵn
    uint32_t next = hw->next_interval;
    // Nếu chưa có khoảng thời gian mới (bằng 0), giữ nguyên chu kỳ hiện tại
    if(next == 0) next = hw->current_interval;

    // Cấu hình lại sự kiện báo thức (alarm) cho GPTimer với chu kỳ tiếp theo
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = next,
        .flags.auto_reload_on_alarm = true
    };
    gptimer_set_alarm_action(timer, &alarm_config);
    hw->current_interval = next;

    // Dùng notify_div đếm steps
    hw->notify_div++;
    BaseType_t hp = pdFALSE;
    if(hw->notify_div >= STEPS_PER_NOTIFY){
        hw->notify_div = 0;
        if(hw->calc_task_handle != NULL)
            vTaskNotifyGiveFromISR(hw->calc_task_handle, &hp);
    }

    // Chuyển đổi ngữ cảnh nếu task tính toán có độ ưu tiên cao hơn được đánh thức
    return (hp == pdTRUE);
}

static void pump_step_calc_task(void *pvParameters){
    stepper_hw_t *hw = (stepper_hw_t *)pvParameters;
    system_state_t *sys = system_get();

    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if(hw->stats->state == PUMP_HOMING){
            hw->stats->current_steps = (uint32_t)hw->profile.current_pos;
            if(hw->profile.current_pos >= hw->profile.target_pos){
                gptimer_stop(hw->timer_handle);
                motor_handle_done(hw, sys);
            }
            continue;
        }

        // Tính interval từ trapezoidal profile
        uint32_t interval = profile_compute_nex_step_interval(&hw->profile);
        if(interval == 0 || hw->profile.current_pos >= hw->profile.target_pos){
            gptimer_stop(hw->timer_handle);
            motor_handle_done(hw, sys);
            continue;
        }

        // PID correction — chỉ áp dụng khi đúng algo VÀ có dữ liệu mới
        if(hw->stats->algorithm == ALGO_TRAP_PID && hw->pid_update_ready){
            hw->pid_update_ready = false;

            float sp   = hw->stats->flow_setpoint;
            float meas = hw->stats->flow_actual;

            if(sp > 0.0f && meas >= 0.0f){
                float ratio      = meas / sp;
                float correction = PID_Update(&hw->pid, 1.0f, ratio);

                float new_interval = (float)interval / (1.0f + correction);
                if(new_interval < (float)INTERVAL_ABS_MIN) new_interval = INTERVAL_ABS_MIN;
                if(new_interval > (float)INTERVAL_ABS_MAX) new_interval = INTERVAL_ABS_MAX;
                interval = (uint32_t)new_interval;
            }
        }
        hw->next_interval = interval;
        hw->stats->current_steps = (uint32_t)hw->profile.current_pos;
        hw->stats->time_run = (float)(esp_timer_get_time() - hw->start_time_us) / 1e6f;
    }
}


static void motor_handle_done(stepper_hw_t *hw, system_state_t *sys){
    if(hw->stats->state == PUMP_HOMING){
        hw->stats->current_steps  = 0;
        hw->profile.current_pos   = 0;
        hw->stats->volume_infused = 0.0f;
        gpio_set_level(hw->en_pin, 1);
        hw->stats->state = PUMP_IDLE;
        ESP_LOGI(TAG, "CH%d homing complete", hw->channel_id);
    } else {
        hw->stats->current_steps = hw->profile.target_pos;
        hw->stats->state = PUMP_DONE;
        ESP_LOGI(TAG, "CH%d pump done", hw->channel_id);
    }
    if(sys->manager_task_handle != NULL)
        xTaskNotifyGive(sys->manager_task_handle);
}


void motor_prepare(stepper_hw_t *hw, int step_p, int dir_p, int en_p){
    if(hw == NULL || hw->stats == NULL) return;

    if(!(hw->is_initialized)){
        hw->step_pin = (gpio_num_t)step_p;
        hw->dir_pin = (gpio_num_t)dir_p;
        hw->en_pin = (gpio_num_t)en_p;

        gpio_motor_init(hw);
        step_timer_init(hw);

        xTaskCreate(pump_step_calc_task, "Step Calc", 4096,
                    (void*)hw, 10, &hw->calc_task_handle);
        
        hw->is_initialized = true;
    }
    gptimer_stop(hw->timer_handle);

    hw->notify_div = 0;
    //===================TÍNH TOÁN THỜI GIAN GIỮA CÁC GIAI ĐOẠN===================//  
    uint32_t target_steps = (uint32_t)converter_ml_to_steps(hw->stats->volume_target);
    float max_v_steps = converter_flow_to_velocity_mms(hw->stats->flow_setpoint)*STEPS_PER_MM; // (mm/s * step/mm -> step/s)
    float accel_steps = hw->stats->acceleration * STEPS_PER_MM; //(mm/s^2 * steps/mm -> steps/s^2)

    // Tính toán thời gian dự kiến cho trapezoidal profile
    if(accel_steps >0 && max_v_steps>0){
        //thời gian tăng/giảm tốc
        float t_ramp = max_v_steps /accel_steps; // (step/s / steps/s^2 -> s)
        //Quãng đường tăng/giảm tốc
        float s_ramp = 0.5f * accel_steps *t_ramp *t_ramp; // (s = (a * t^2)/2)

        if(2.0f *s_ramp >= (float) target_steps){
            // Hình tam giác (Không kịp đạt vận tốc tối đa)
            hw->stats->run_time_total = 2.0f *sqrtf((float)target_steps/accel_steps);

        }else{
            //Hình thang
            float s_const = (float)target_steps - (2.0f * s_ramp); 
            float t_const = s_const/max_v_steps;
            //hw->stats->run_time_total = hw->stats->volume_target/hw->stats->flow_setpoint;
            hw->stats->run_time_total = (2.0f * t_ramp) + t_const;
        }
    }else{
        hw->stats->run_time_total = 0;
    }

    //====================Khởi tạo trapezoidal profile====================//
    hw->stats->current_steps = 0;
    profile_init(&hw->profile, accel_steps, max_v_steps);
    //hw->profile.current_pos = (long)hw->stats->current_steps;
    hw->profile.current_pos = 0;
    profile_move_to(&hw->profile, (long)target_steps);
    hw->next_interval = (uint32_t)hw->profile.c0; // start time interval
    hw->current_interval = hw->next_interval; // time interval for next step 

    if(hw->stats->algorithm == ALGO_TRAP_PID){
        PID_Init(&hw->pid);

        hw->pid.Kp        = 5.0f;
        hw->pid.Ki        = 0.25f;
        hw->pid.Kd        = 0.0f;
        hw->pid.tau       = 0.1f;
        hw->pid.T         = 0.2f;

        // Output là hệ số tỉ lệ [-0.5 .. +0.5]
        hw->pid.limMinOut = -0.5f;
        hw->pid.limMaxOut =  0.5f;
        hw->pid.limMinInt = -0.3f;
        hw->pid.limMaxInt =  0.3f;

        //hw->pid_update_ready = false;  // ← init flag

        hw->stats->kp = hw->pid.Kp;
        hw->stats->ki = hw->pid.Ki;
        hw->stats->kd = hw->pid.Kd;
    }else{
        ESP_LOGI(TAG, "CH%d algorithm: TRAPEZOIDAL only", hw->channel_id);
    }

    hw->stats->time_run = 0.0f; 
    hw->stats->volume_infused = 0.0f;
    hw->stats->flow_actual = 0.0f;
    
    hw->stats->state = PUMP_RUN;

    ESP_LOGI(TAG, "Sync: Target %lu steps, Total Time: %.2f s, c0: %lu", 
             target_steps, hw->stats->run_time_total, hw->current_interval);
    

    // Set timer, gpio, time start 
    gpio_set_level(hw->dir_pin, 0);
    gpio_set_level(hw->en_pin, 0);
    //motor_fire(hw);
    ESP_LOGI(TAG, "CH%d PREPARE: target=%lu steps, c0=%lu us, current_pos=%ld, distance=%ld",
        hw->channel_id,
        target_steps,
        hw->current_interval,
        hw->profile.current_pos,
        (long)target_steps - hw->profile.current_pos
    );
}
void motor_fire(stepper_hw_t *hw){
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = hw->current_interval,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(hw->timer_handle, &alarm_config));
    gptimer_stop(hw->timer_handle);  // stop trước phòng trường hợp đang chạy
    ESP_LOGI(TAG, "Starting Trapezoidal profile...");
    ESP_ERROR_CHECK(gptimer_start(hw->timer_handle));
    
    
    hw->start_time_us = esp_timer_get_time();
}

void motor_stop(stepper_hw_t *hw){
    if(hw == NULL || hw->timer_handle == NULL) return;

    //Dừng timer
    gptimer_stop(hw->timer_handle);

    //Ngắt điện cuộn dây motor
    gpio_set_level(hw->en_pin, 1);

    //Reset trạng thái động cơ
    if(hw->stats != NULL){
        hw->stats->state = PUMP_IDLE;
        hw->stats->flow_actual = 0.0f;
    }

    ESP_LOGI("PUMP_CH", "Motor Channel %d Stopped", hw->channel_id);
}
void motor_home(stepper_hw_t *hw){
    if(hw == NULL || hw->stats == NULL) return;

    // Chưa init thì phải init trước
    if(!hw->is_initialized){
        const uint8_t step_pins[] = {CH0_STEP_PIN, CH1_STEP_PIN};
        const uint8_t dir_pins[]  = {CH0_DIR_PIN,  CH1_DIR_PIN};
        const uint8_t en_pins[]   = {CH0_EN_PIN,   CH1_EN_PIN};
        uint8_t id = hw->channel_id;
        hw->step_pin = (gpio_num_t)step_pins[id];
        hw->dir_pin  = (gpio_num_t)dir_pins[id];
        hw->en_pin   = (gpio_num_t)en_pins[id];
        gpio_motor_init(hw);
        step_timer_init(hw);
        xTaskCreate(pump_step_calc_task, "Step Calc", 4096,
                    (void*)hw, 10, &hw->calc_task_handle);
        hw->is_initialized = true;
        ESP_LOGI("PUMP_CH", "CH%d lazy-init for homing", hw->channel_id);
    }

    gptimer_stop(hw->timer_handle);

    uint32_t home_interval = 20; // µs

    hw->profile.current_pos = 0;
    hw->profile.target_pos  = HOMING_STEPS;
    hw->current_interval    = home_interval;
    hw->next_interval       = home_interval;  // ← calc_task sẽ giữ nguyên giá trị này
    hw->notify_div          = 0;
    hw->stats->state        = PUMP_HOMING;

    gpio_set_level(hw->dir_pin, 1);
    gpio_set_level(hw->en_pin, 0);

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = hw->current_interval,
        .flags.auto_reload_on_alarm = true,
    };
    gptimer_set_alarm_action(hw->timer_handle, &alarm_config);
    gptimer_start(hw->timer_handle);

    ESP_LOGI("PUMP_CH", "CH%d homing started (%lu steps)",
             hw->channel_id, (uint32_t)HOMING_STEPS);
}

