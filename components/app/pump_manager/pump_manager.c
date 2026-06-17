#include "pump_manager.h"
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "driver/uart.h"

static const char *TAG = "PUMP_MGR";
// static const char *TAG1 = "RUN";
static stepper_hw_t motors[MAX_CHANNEL];
static portMUX_TYPE angle_mux = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t i2c_bus_ch0_mutex = NULL;

as5600_t as5600_ch0;
as5600_t as5600_ch1;
as5600_logic_t as5600_logic_ch0 = {0};
as5600_logic_t as5600_logic_ch1 = {0};
// Khởi tạo bus I2C và cảm biến AS5600
static i2c_master_bus_handle_t bus_ch0;
static i2c_master_dev_handle_t dev_ch0;

static i2c_master_bus_handle_t bus_ch1;
static i2c_master_dev_handle_t dev_ch1;
// Khai báo 2 bộ lọc cho 2 kênh

float display_angle_ch0 = 0.0f;
float prev_degree_measure_ch0 = 0.0f;

float display_angle_ch1 = 0.0f;
float prev_degree_measure_ch1 = 0.0f;

// static float EMA_Update(ema_filter_t *filter, float raw_value) {
//     if (!filter->is_init) {
//         filter->current_value = raw_value;
//         filter->is_init = true;
//     } else {
//         filter->current_value = (filter->alpha * raw_value) + ((1.0f - filter->alpha) * filter->current_value);
//     }
//     return filter->current_value;
// }
static void cmd_uart_init(void) {
    uart_config_t cfg = {
        .baud_rate  = CMD_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(CMD_UART_PORT, &cfg);
    // UART0: TX=1, RX=3 — mặc định của ESP32, không cần set lại
    uart_driver_install(CMD_UART_PORT, 1024, 0, 0, NULL, 0);
}


void pump_manager_init(void){
    //Xóa rác RAM và thiết lập các giá trị mặc định cho motors
    i2c_bus_ch0_mutex = xSemaphoreCreateMutex();
    configASSERT(i2c_bus_ch0_mutex != NULL);
    memset(motors, 0, sizeof(motors));
    system_state_t *sys = system_get();

    for(int i = 0; i<MAX_CHANNEL; i++){
        motors[i].stats = &sys->channels[i];
        motors[i].channel_id = i;
        motors[i].is_initialized = false;
        motors[i].dir_inverted = false;

        sys->channels[i].state = PUMP_IDLE;
        sys->channels[i].current_steps = 0;
        sys->channels[i].raw_threshold = 5;
        sys->channels[i].last_sensor_raw = 0;
        sys->channels[i].prev_sensor_raw = 0;

    }
    
    uart_init(SHARED_UART_PORT, SHARED_TX_PIN, SHARED_RX_PIN);
    // uart_loopback_test(SHARED_UART_PORT, SHARED_TX_PIN, SHARED_RX_PIN);
    
    TMC_Init(SHARED_UART_PORT, PUMP_CH1_NODE);
    TMC_Init(SHARED_UART_PORT, PUMP_CH2_NODE);

    uart_driver_delete(SHARED_UART_PORT);

    cmd_uart_init();
    vTaskDelay(pdMS_TO_TICKS(500));


    i2c_master_init(&bus_ch0, &dev_ch0, I2C_NUM_0, 21, 22);// die
    i2c_master_init(&bus_ch1, &dev_ch1, I2C_NUM_1, 32, 33);

    as5600_init(&as5600_ch0, dev_ch0);
    as5600_init(&as5600_ch1, dev_ch1);


    pcf8574_init(bus_ch0, 0x20);

    as5600_config_slow_filter(&as5600_ch0);
    as5600_disable_filter(&as5600_ch0);

    as5600_config_slow_filter(&as5600_ch1);
    as5600_disable_filter(&as5600_ch1);

    sys->channels[0].volume_target = 0.0f; //0.004 0.002
    sys->channels[0].flow_setpoint = 0.0f; // 0.06 0.03
    sys->channels[0].acceleration  = 3.0f;  
    sys->channels[0].algorithm     = ALGO_TRAP_PID;

    sys->channels[1].volume_target = 0.0; //0.004 0.002
    sys->channels[1].flow_setpoint = 0.0f; // 0.06 0.03
    sys->channels[1].acceleration  = 3.0f;   
    sys->channels[1].algorithm     = ALGO_TRAP_PID;

    pump_manager_system_start();

    // printf("{\"status\":\"ready\",\"msg\":\"System initialized\"}\n");

    xTaskCreate(pump_manager_task, 
            "pump_mgr_logic", 
            4096, 
            (void*)sys, 
            6,
            &sys->manager_task_handle);
    
    xTaskCreate(sensor_task, 
                "sensor", 
                8192, 
                NULL, 
                8, 
                NULL);
    xTaskCreate(log_task,
                "log task",
                4096,
                NULL,
                2,
                NULL);
    xTaskCreate(uart_cmd_task, 
                "uart_cmd", 
                4096, 
                NULL, 
                5, 
                NULL);
    ESP_LOGI(TAG, "Starting System...");

}


void pump_manager_start_channel(uint8_t channel_id){
    if(channel_id >= MAX_CHANNEL) return;

    if (channel_id == 0) {
        motors[0].stats->is_running = true;
        motor_prepare(&motors[0], CH0_STEP_PIN, CH0_DIR_PIN, CH0_EN_PIN);
        motor_fire(&motors[0]);
    } else {
        motors[1].stats->is_running = true;
        motor_prepare(&motors[1], CH1_STEP_PIN, CH1_DIR_PIN, CH1_EN_PIN);
        motor_fire(&motors[1]);
    }
    ESP_LOGI(TAG, "Channel %d started", channel_id);
}

void pump_manager_system_start(void){
    system_state_t *sys = system_get();

    switch (sys->op_mode){
        case SYS_MODE_INDEPENDENT:
            sys->is_system_running = true;
            pump_manager_start_channel(sys->ui.selected_channel);
            break;
        case SYS_MODE_SIMULTANEOUS:
            motors[0].stats->is_running = true;
            motors[1].stats->is_running = true;
            motor_prepare(&motors[0], CH0_STEP_PIN, CH0_DIR_PIN, CH0_EN_PIN);
            motor_prepare(&motors[1], CH1_STEP_PIN, CH1_DIR_PIN, CH1_EN_PIN);
            // Khoảng cách giữa 2 lệnh fire chỉ còn ~1µs
            motor_fire(&motors[0]);
            motor_fire(&motors[1]);
            sys->is_system_running = true;
            break;

        case SYS_MODE_SEQUENTIAL:
            // Chế độ liên tiếp: Start kênh 0 trước
            sys->is_system_running = true;
            pump_manager_start_channel(0);
            break;
        case SYS_MODE_HOMING:
            //pump_manager_home_all();
            pump_manager_home_channel(sys->ui.selected_channel);
            break;

        default:
            ESP_LOGW(TAG, "Unknown op_mode: %d", sys->op_mode);
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

void pump_manager_task(void *pvParameters){
    system_state_t *sys = system_get();
    //pump_manager_system_start();

    while(1){
        //Block vô thời hạn, chỉ wake khi có notify
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));


        //Xử lý logic chạy liên tiếp
        if(sys->op_mode == SYS_MODE_SEQUENTIAL && sys->is_system_running){
            if(sys->channels[0].state == PUMP_DONE&&
                sys->channels[1].state == PUMP_IDLE){
                ESP_LOGI(TAG, "Sequential: CH0 done → starting CH1");
                pump_manager_start_channel(1);
            }
            if(sys->channels[0].state == PUMP_DONE &&
                sys->channels[1].state == PUMP_DONE){
                    ESP_LOGI(TAG, "Sequential: all done");
                    // sys->op_mode = SYS_MODE_HOMING;
                    // pump_manager_home_all();
                    sys->is_system_running = false;
                }
        }
        //Xử lý logic chạy 2 kênh
        if(sys->op_mode == SYS_MODE_SIMULTANEOUS && sys->is_system_running){
            if(sys->channels[0].state == PUMP_DONE &&
                sys->channels[1].state == PUMP_DONE){
                    ESP_LOGI(TAG, "Simultaneous done");
                    // sys->op_mode = SYS_MODE_HOMING;
                    // pump_manager_home_all();
                    sys->is_system_running = false;
                }
        }
        if(sys->op_mode == SYS_MODE_INDEPENDENT && sys->is_system_running){
            uint8_t selct_ch = sys->ui.selected_channel;
            if(sys->channels[selct_ch].state == PUMP_DONE){
                ESP_LOGI(TAG, "CH %d: DONE", selct_ch);
                // sys->op_mode = SYS_MODE_HOMING;
                // pump_manager_home_channel(selct_ch);
                sys->is_system_running = false;
            }
        }

        if(sys->op_mode == SYS_MODE_HOMING){
            bool all_idle = true;
            for(int i = 0; i < MAX_CHANNEL; i++){
                if(sys->channels[i].state == PUMP_HOMING){
                    all_idle = false;
                    break;
                }
            }
            if(all_idle){
                ESP_LOGI(TAG, "Homing complete → INDEPENDENT");
                sys->op_mode = SYS_MODE_INDEPENDENT;
                sys->is_system_running = false;
            }
        }
    }
}

void pump_manager_home_channel(uint8_t channel_id){
    if(channel_id >= MAX_CHANNEL) return;

    system_state_t *sys= system_get();
    pump_state_t st = sys->channels[channel_id].state;

    if(st == PUMP_RUN){
        ESP_LOGW(TAG, "CH%d still running, cannot home now", channel_id);
        return;
    }
    motor_home(&motors[channel_id]);
}

void pump_manager_home_all(void){

    system_state_t *sys = system_get();

    if(sys->channels[0].state == PUMP_RUN||
       sys->channels[1].state == PUMP_RUN){
        ESP_LOGW(TAG, "Cannot home: channel still running");
        return;
       }
    motor_home(&motors[0]);
    motor_home(&motors[1]);
    ESP_LOGI(TAG, "Both channels homing");
}

/**
 * @brief Cập nhật volume_infused và flow_actual từ dữ liệu cảm biến AS5600.
 *        Gọi mỗi chu kỳ log (1 giây).
 *
 * @param ch         Con trỏ channel stats cần cập nhật
 * @param display_angle  Góc tích lũy (multi-turn, đơn vị độ) từ as5600_process_multi_turn
 * @param prev_angle Góc chu kỳ trước (được hàm tự cập nhật)
 * @param dt_s       Thời gian 1 chu kỳ log, tính bằng giây (ví dụ: 1.0f)
 */
static void pump_manager_update_channel_stats(pump_channel_t *ch,
                                              float display_angle,
                                              float *prev_angle,
                                              float dt_s)
{
    if(!ch->is_running) {
        ch->flow_actual = 0.0f;
        *prev_angle = display_angle; // Giữ sync để không tích lũy sai khi idle
        return;
    }

    float delta_deg = display_angle - *prev_angle;
    float delta_ml  = converter_degree_to_ml(delta_deg);

    ch->volume_infused += delta_ml;
    ch->flow_actual = (dt_s > 0.0f) ? (delta_ml / dt_s * 3600.0f) : 0.0f;
    *prev_angle = display_angle;
}

void log_task(void *pvParameters) {
    system_state_t *sys = system_get();

    const float DT_S = 1.0f;
    const TickType_t period = pdMS_TO_TICKS((uint32_t)(DT_S * 1000));
    TickType_t last_wake = xTaskGetTickCount();
    while(1) {
        vTaskDelayUntil(&last_wake, period);

        float angle0, angle1;
        portENTER_CRITICAL(&angle_mux);
        angle0 = display_angle_ch0;
        angle1 = display_angle_ch1;
        portEXIT_CRITICAL(&angle_mux);

        pump_manager_update_channel_stats(&sys->channels[0], angle0, &prev_degree_measure_ch0, DT_S);
        pump_manager_update_channel_stats(&sys->channels[1], angle1, &prev_degree_measure_ch1, DT_S);
        
        for(int i = 0; i < MAX_CHANNEL; i++){
            if(sys->channels[i].algorithm == ALGO_TRAP_PID &&
               sys->channels[i].is_running){
                motors[i].pid_update_ready = true;
            }
        }
        for(int ch = 0; ch < MAX_CHANNEL; ch++) {
                printf(
                    "{\"ch\":%d,"
                    "\"algo\":\"%s\","
                    "\"vol_infused\":%.5f,"
                    "\"vol_target\":%.5f,"
                    "\"flow_measure\":%.5f,"
                    "\"flow_setpoint\":%.5f,"
                    "\"time_run\":%.1f,"
                    "\"state\":%d,"
                    "\"steps\":%" PRIu32 ","
                    "\"kp\":%.2f,"
                    "\"ki\":%.2f,"
                    "\"kd\":%.2f}\n",
                    ch,
                    (sys->channels[ch].algorithm == ALGO_TRAP_PID) ? "TRAP+PID" : "TRAP",
                    sys->channels[ch].volume_infused,
                    sys->channels[ch].volume_target,
                    sys->channels[ch].flow_actual,
                    sys->channels[ch].flow_setpoint,
                    sys->channels[ch].time_run,
                    sys->channels[ch].state,
                    sys->channels[ch].current_steps,
                    sys->channels[ch].kp,
                    sys->channels[ch].ki,
                    sys->channels[ch].kd
                );
            }
    }
}

void sensor_task(void *pvParameters) {
    system_state_t *sys = system_get();
    const TickType_t period = pdMS_TO_TICKS(10);
    TickType_t last_wake = xTaskGetTickCount();

    while(1) {
        // Lấy mutex trước khi đọc AS5600_ch0 trên bus_ch0 (chung với PCF8574)
        if (xSemaphoreTake(i2c_bus_ch0_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            as5600_process_multi_turn(&as5600_ch0, &as5600_logic_ch0,
                                      &display_angle_ch0,
                                      sys->channels[0].is_running);
            sys->channels[0].last_sensor_raw = (int32_t)as5600_logic_ch0.last_raw;
            xSemaphoreGive(i2c_bus_ch0_mutex);
        }

        as5600_process_multi_turn(&as5600_ch1, &as5600_logic_ch1,
                                  &display_angle_ch1,
                                  sys->channels[1].is_running);
        sys->channels[1].last_sensor_raw = (int32_t)as5600_logic_ch1.last_raw;

        vTaskDelayUntil(&last_wake, period);
    }
}
static void handle_command(const char *json_str) {
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE("UART_CMD", "JSON Parse Error: %s", json_str);
        return;
    }

    cJSON *jcmd = cJSON_GetObjectItem(root, "cmd");
    if (!jcmd || !jcmd->valuestring) {
        ESP_LOGW("UART_CMD", "Thiếu field 'cmd'");
        cJSON_Delete(root);
        return;
    }

    const char *cmd = jcmd->valuestring;
    system_state_t *sys = system_get();

    if (strcmp(cmd, "START") == 0) {
        cJSON *jmode = cJSON_GetObjectItem(root, "mode");
        const char *mode = (jmode && jmode->valuestring) ? jmode->valuestring : "";

        if (strcmp(mode, "SIMUL") == 0) {
            cJSON *jflow = cJSON_GetObjectItem(root, "flow");
            cJSON *jvol  = cJSON_GetObjectItem(root, "vol");
            if (!jflow || !jvol) {
                ESP_LOGW("UART_CMD", "SIMUL: thiếu flow hoặc vol");
                cJSON_Delete(root); return;
            }
            float flow = (float)jflow->valuedouble;
            float vol  = (float)jvol->valuedouble;
            sys->channels[0].flow_setpoint = flow;
            sys->channels[0].volume_target = vol;
            sys->channels[1].flow_setpoint = flow;
            sys->channels[1].volume_target = vol;
            sys->op_mode = SYS_MODE_SIMULTANEOUS;

        } else if (strcmp(mode, "SEQ") == 0) {
            cJSON *jc0f = cJSON_GetObjectItem(root, "ch0_flow");
            cJSON *jc0v = cJSON_GetObjectItem(root, "ch0_vol");
            cJSON *jc1f = cJSON_GetObjectItem(root, "ch1_flow");
            cJSON *jc1v = cJSON_GetObjectItem(root, "ch1_vol");
            if (!jc0f || !jc0v || !jc1f || !jc1v) {
                ESP_LOGW("UART_CMD", "SEQ: thiếu field");
                cJSON_Delete(root); return;
            }
            sys->channels[0].flow_setpoint = (float)jc0f->valuedouble;
            sys->channels[0].volume_target = (float)jc0v->valuedouble;
            sys->channels[1].flow_setpoint = (float)jc1f->valuedouble;
            sys->channels[1].volume_target = (float)jc1v->valuedouble;
            sys->op_mode = SYS_MODE_SEQUENTIAL;

        } else if (strcmp(mode, "INDEP") == 0 || strcmp(mode, "") == 0)  { // Independent
            cJSON *jch   = cJSON_GetObjectItem(root, "ch");
            cJSON *jflow = cJSON_GetObjectItem(root, "flow");
            cJSON *jvol  = cJSON_GetObjectItem(root, "vol");
            if (!jch || !jflow || !jvol) {
                ESP_LOGW("UART_CMD", "INDEP: thiếu field");
                cJSON_Delete(root); return;
            }
            int ch = jch->valueint;
            if (ch < 0 || ch >= MAX_CHANNEL) {
                ESP_LOGW("UART_CMD", "ch không hợp lệ: %d", ch);
                cJSON_Delete(root); return;
            }
            sys->channels[ch].flow_setpoint = (float)jflow->valuedouble;
            sys->channels[ch].volume_target = (float)jvol->valuedouble;
            sys->op_mode = SYS_MODE_INDEPENDENT;
            sys->ui.selected_channel = ch;
        }
        pump_manager_system_start();

    } else if (strcmp(cmd, "STOP") == 0) {
        cJSON *jch = cJSON_GetObjectItem(root, "ch");
        if (!jch) {
            pump_manager_system_stop();
        } else {
            int ch = jch->valueint;
            if (ch >= 0 && ch < MAX_CHANNEL) {
                motor_stop(&motors[ch]);
                sys->channels[ch].is_running = false;
            }
        }

    }  else if (strcmp(cmd, "HOME") == 0) {
        cJSON *jch = cJSON_GetObjectItem(root, "ch");
        sys->op_mode = SYS_MODE_HOMING; // Thêm dòng này
        sys->is_system_running = true;  // Nên set cờ này để đồng bộ logic

        if (jch) {
            int ch = jch->valueint;
            if (ch >= 0 && ch < MAX_CHANNEL) {
                sys->ui.selected_channel = ch;
                pump_manager_home_channel(ch);
            }
        } else {
            pump_manager_home_all();
        }
    }

    cJSON_Delete(root);
}

void uart_cmd_task(void *pvParameters) {
    char buf[256];
    int  pos = 0;

    while (1) {
        uint8_t c;
        // ✅ Đọc từ CMD_UART_PORT (UART0) thay vì SHARED_UART_PORT
        if (uart_read_bytes(CMD_UART_PORT, &c, 1, pdMS_TO_TICKS(10)) <= 0)
            continue;

        if (c == '\n') {
            buf[pos] = '\0';
            if (pos > 0)
                handle_command(buf);
            pos = 0;
        } else if (c == '\r') {
            // bỏ qua
        } else if (pos < 255) {
            buf[pos++] = c;
        } else {
            ESP_LOGW("UART_CMD", "Buffer overflow, reset frame");
            pos = 0;
        }
    }
}