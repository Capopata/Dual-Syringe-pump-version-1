#include "pump_manager.h"

static const char *TAG = "PUMP_MGR";

/**
 * Khởi tạo struct của hệ thống
 */
static stepper_hw_t motors[MAX_CHANNEL];

/**
 * Khởi tạo semaphore mutex cho 2 task là uart task và pump_manager_task
 */
SemaphoreHandle_t sys_state_sema = NULL;

/**
 * Khai báo notify giữa sensor task và log task
 */
static TaskHandle_t log_task_handle = NULL;

/**
 * Khai báo I2C cho 2 cảm biến
 */
as5600_t as5600_ch0;
as5600_t as5600_ch1;

/**
 * Khai báo biến logic cho 2 cảm biến
 */
as5600_logic_t as5600_logic_ch0 = {0};
as5600_logic_t as5600_logic_ch1 = {0};


// Khởi tạo bus I2C và cảm biến AS5600
static i2c_master_bus_handle_t bus_ch0;
static i2c_master_dev_handle_t dev_ch0;
static i2c_master_bus_handle_t bus_ch1;
static i2c_master_dev_handle_t dev_ch1;


static void pump_manager_task(void *pvParameters);
static void sensor_task(void *pvParameters);
static void log_task(void *pvParameters);
static void uart_cmd_task(void *pvParameters);
static void pump_manager_home_all(void);
static void pump_manager_start_channel(uint8_t channel_id);
static void pump_manager_update_channel_stats(pump_channel_t *ch,
                                              float display_angle,
                                              float *prev_angle,
                                              float dt_s);
static void handle_command(const char *json_str);


esp_err_t pump_manager_init(void){

    sys_state_sema = xSemaphoreCreateMutex();
    if(sys_state_sema== NULL){
        ESP_LOGE(TAG, "Failed to create Mutex");
        return ESP_FAIL;
    }
    system_state_t *sys = system_get();

    for(int i = 0; i<MAX_CHANNEL; i++){
        motors[i].stats = &sys->channels[i];
        motors[i].channel_id = i;
        motors[i].is_initialized = false;

        sys->channels[i].state = PUMP_IDLE;
        sys->channels[i].current_steps = 0;
        sys->channels[i].volume_target = 0.0f;
        sys->channels[i].flow_setpoint = 0.0f;
        sys->channels[i].acceleration = 3.0f;
        sys->channels[i].algorithm = ALGO_TRAP_PID;
    }
    
    uart_init(SHARED_UART_PORT, SHARED_TX_PIN, SHARED_RX_PIN);
    TMC_Init(SHARED_UART_PORT, PUMP_CH1_NODE);
    TMC_Init(SHARED_UART_PORT, PUMP_CH2_NODE);

    uart_driver_delete(SHARED_UART_PORT); // Giải phóng driver cũ để tránh xung đột với UART0 (Command Port)

    cmd_uart_init();

    // Khởi tạo I2C cho cảm biến AS5600
    i2c_master_init(&bus_ch0, &dev_ch0, I2C_NUM_0, 21, 22);
    i2c_master_init(&bus_ch1, &dev_ch1, I2C_NUM_1, 32, 33);
    as5600_init(&as5600_ch0, dev_ch0);
    as5600_init(&as5600_ch1, dev_ch1);

    // Cấu hình chế độ filter và output cho AS5600
    as5600_config_slow_filter(&as5600_ch0);
    as5600_config_slow_filter(&as5600_ch1);
    
    // Tạo các task
    xTaskCreate(sensor_task, 
                "sensor", 
                8192, 
                NULL, 
                8, 
                NULL);

    xTaskCreate(pump_manager_task, 
            "pump_mgr_logic", 
            4096, 
            (void*)sys, 
            6,
            &sys->manager_task_handle);
    
    xTaskCreate(uart_cmd_task, 
            "uart_cmd", 
            4096, 
            NULL, 
            5, 
            NULL);

    xTaskCreate(log_task,
                "log task",
                4096,
                NULL,
                2,
                &log_task_handle);

    ESP_LOGI(TAG, "Starting System...");

    return ESP_OK;

}

static void pump_manager_task(void *pvParameters){
    system_state_t *sys = system_get();

    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (xSemaphoreTake(sys_state_sema, portMAX_DELAY) == pdTRUE) {
            
            // Xử lý logic chạy liên tiếp (Sequential)
            if(sys->op_mode == SYS_MODE_SEQUENTIAL && sys->is_system_running){
                if(sys->channels[0].state == PUMP_DONE && sys->channels[1].state == PUMP_IDLE){
                    ESP_LOGI(TAG, "Sequential: CH0 done → starting CH1");
                    pump_manager_start_channel(1);
                }
                if(sys->channels[0].state == PUMP_DONE && sys->channels[1].state == PUMP_DONE){
                    ESP_LOGI(TAG, "Sequential: all done");
                    sys->is_system_running = false;
                }
            }
            
            // Xử lý logic chạy song song (Simultaneous)
            if(sys->op_mode == SYS_MODE_SIMULTANEOUS && sys->is_system_running){
                if(sys->channels[0].state == PUMP_DONE && sys->channels[1].state == PUMP_DONE){
                    ESP_LOGI(TAG, "Simultaneous done");
                    sys->is_system_running = false;
                }
            }

            // Xử lý chạy độc lập (Independent)
            if(sys->op_mode == SYS_MODE_INDEPENDENT && sys->is_system_running){
                uint8_t selct_ch = sys->selected_channel;
                if(sys->channels[selct_ch].state == PUMP_DONE){
                    ESP_LOGI(TAG, "CH %d: DONE", selct_ch);
                    sys->is_system_running = false;
                }
            }

            // Xử lý homing hoàn tất
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
            
            xSemaphoreGive(sys_state_sema);
        }
    }
}


static void log_task(void *pvParameters) {
    system_state_t *sys = system_get();

    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

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

static void sensor_task(void *pvParameters) {
    system_state_t *sys = system_get();
    const TickType_t period = pdMS_TO_TICKS(20);
    TickType_t last_wake = xTaskGetTickCount();
    static float prev_degree_measure_ch0;
    static float prev_degree_measure_ch1;

    while(1) {
        float temp_angle0 = 0.0f;
        float temp_angle1 = 0.0f;

        as5600_process_multi_turn(&as5600_ch0, &as5600_logic_ch0,
                                    &temp_angle0,
                                    sys->channels[0].is_running);

        as5600_process_multi_turn(&as5600_ch1, &as5600_logic_ch1,
                                  &temp_angle1,
                                  sys->channels[1].is_running);

        // Tính toán lưu lượng & thể tích định kỳ 1 giây (50 * 20ms = 1000ms)
        static uint32_t calc_counter = 0;
        calc_counter++;
        if (calc_counter >= 50) {
            calc_counter = 0;
            
            pump_manager_update_channel_stats(&sys->channels[0], temp_angle0, &prev_degree_measure_ch0, 1.0f);
            pump_manager_update_channel_stats(&sys->channels[1], temp_angle1, &prev_degree_measure_ch1, 1.0f);
            
            for(int i = 0; i < MAX_CHANNEL; i++){
                if(sys->channels[i].algorithm == ALGO_TRAP_PID &&
                   sys->channels[i].is_running){
                    motors[i].pid_update_ready = true;
                }
            }

            if (log_task_handle != NULL) {
                xTaskNotifyGive(log_task_handle);
            }
        }

        vTaskDelayUntil(&last_wake, period);
    }
}

static void uart_cmd_task(void *pvParameters) {
    char buf[256];
    int  pos = 0;

    while (1) {
        uint8_t c;
        // Đọc từng byte từ UART0
        if (uart_read_bytes(CMD_UART_PORT, &c, 1, pdMS_TO_TICKS(10)) <= 0)
            continue;
        //Kiểm tra ký tự nhận được    
        if (c == '\n') {
            buf[pos] = '\0'; // Gán ký tự kết thúc chuỗi tại vị trí cuối cùng
            if (pos > 0) 
                handle_command(buf); //Xử lý chuỗi JSON vừa nhân được
            pos = 0; // Gán lại vị trí về 0 để chuẩn bị nhân lệnh mới

        } else if (c == '\r') { // Bỏ qua ký tự xuống dòng
        } else if (pos < 255) { // Nếu chưa đầy thì nhận tiếp ký tự
            buf[pos] = c;
            pos++;
        } else { // Nếu đã đầy thì reset    
            ESP_LOGW("UART_CMD", "Buffer overflow, reset frame");
            pos = 0;
        }
    }
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
    ch->flow_actual = delta_ml * 3600.0f ;
    *prev_angle = display_angle;
}

static void handle_command(const char *json_str) {
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE("UART_CMD", "JSON Parse Error: %s", json_str);
        return;
    }

    cJSON *jcmd = cJSON_GetObjectItem(root, "cmd");
    if (jcmd == NULL || jcmd->valuestring == NULL) {
        ESP_LOGW("UART_CMD", "Thiếu field 'cmd'");
        cJSON_Delete(root);
        return;
    }

    const char *cmd = jcmd->valuestring;
    system_state_t *sys = system_get();

    // 1. Phân tích trước các tham số ra các biến cục bộ (NGOÀI MUTEX)
    float flow = 0.0f, vol = 0.0f;
    float ch0_flow = 0.0f, ch0_vol = 0.0f;
    float ch1_flow = 0.0f, ch1_vol = 0.0f;
    int ch = -1;
    sys_op_mode_t target_mode = SYS_MODE_INDEPENDENT;
    bool is_start = false, is_stop = false, is_home = false;

    if (strcmp(cmd, "START") == 0) {
        is_start = true;
        cJSON *jmode = cJSON_GetObjectItem(root, "mode");
        const char *mode = (jmode && jmode->valuestring) ? jmode->valuestring : "";

        if (strcmp(mode, "SIMUL") == 0) {
            cJSON *jflow = cJSON_GetObjectItem(root, "flow");
            cJSON *jvol  = cJSON_GetObjectItem(root, "vol");
            if (jflow == NULL || jvol == NULL) {
                ESP_LOGW("UART_CMD", "SIMUL: thiếu flow hoặc vol");
                cJSON_Delete(root); return;
            }
            flow = (float)jflow->valuedouble;
            vol  = (float)jvol->valuedouble;
            target_mode = SYS_MODE_SIMULTANEOUS;

        } else if (strcmp(mode, "SEQ") == 0) {
            cJSON *jc0f = cJSON_GetObjectItem(root, "ch0_flow");
            cJSON *jc0v = cJSON_GetObjectItem(root, "ch0_vol");
            cJSON *jc1f = cJSON_GetObjectItem(root, "ch1_flow");
            cJSON *jc1v = cJSON_GetObjectItem(root, "ch1_vol");
            if (jc0f == NULL || jc0v == NULL || jc1f == NULL || jc1v == NULL) {
                ESP_LOGW("UART_CMD", "SEQ: thiếu field");
                cJSON_Delete(root); return;
            }
            ch0_flow = (float)jc0f->valuedouble;
            ch0_vol  = (float)jc0v->valuedouble;
            ch1_flow = (float)jc1f->valuedouble;
            ch1_vol  = (float)jc1v->valuedouble;
            target_mode = SYS_MODE_SEQUENTIAL;

        } else if (strcmp(mode, "INDEP") == 0 || strcmp(mode, "") == 0)  {
            cJSON *jch   = cJSON_GetObjectItem(root, "ch");
            cJSON *jflow = cJSON_GetObjectItem(root, "flow");
            cJSON *jvol  = cJSON_GetObjectItem(root, "vol");
            if (jch == NULL|| jflow == NULL || jvol ==NULL) {
                ESP_LOGW("UART_CMD", "INDEP: thiếu field");
                cJSON_Delete(root); return;
            }
            ch = jch->valueint;
            if (ch < 0 || ch >= MAX_CHANNEL) {
                ESP_LOGW("UART_CMD", "ch không hợp lệ: %d", ch);
                cJSON_Delete(root); return;
            }
            flow = (float)jflow->valuedouble;
            vol  = (float)jvol->valuedouble;
            target_mode = SYS_MODE_INDEPENDENT;
        }

    } else if (strcmp(cmd, "STOP") == 0) {
        is_stop = true;
        cJSON *jch = cJSON_GetObjectItem(root, "ch");
        if (jch) {
            ch = jch->valueint;
        } else {
            ch = -1; // Dừng toàn bộ
        }

    } else if (strcmp(cmd, "HOME") == 0) {
        is_home = true;
        cJSON *jch = cJSON_GetObjectItem(root, "ch");
        if (jch) {
            ch = jch->valueint;
        } else {
            ch = -1; // Home toàn bộ
        }
    }

    // Khóa mutex để gán và kích hoạt motor
    if (xSemaphoreTake(sys_state_sema, portMAX_DELAY) == pdTRUE) {
        if (is_start) {
            if (target_mode == SYS_MODE_SIMULTANEOUS) {
                sys->channels[0].flow_setpoint = flow;
                sys->channels[0].volume_target = vol;
                sys->channels[1].flow_setpoint = flow;
                sys->channels[1].volume_target = vol;
                sys->op_mode = SYS_MODE_SIMULTANEOUS;
            } else if (target_mode == SYS_MODE_SEQUENTIAL) {
                sys->channels[0].flow_setpoint = ch0_flow;
                sys->channels[0].volume_target = ch0_vol;
                sys->channels[1].flow_setpoint = ch1_flow;
                sys->channels[1].volume_target = ch1_vol;
                sys->op_mode = SYS_MODE_SEQUENTIAL;
            } else if (target_mode == SYS_MODE_INDEPENDENT) {
                sys->channels[ch].flow_setpoint = flow;
                sys->channels[ch].volume_target = vol;
                sys->op_mode = SYS_MODE_INDEPENDENT;
                sys->selected_channel = ch;
            }
            pump_manager_system_start();

        } else if (is_stop) {
            if (ch == -1) {
                pump_manager_system_stop();
            } else {
                if (ch >= 0 && ch < MAX_CHANNEL) {
                    motor_stop(&motors[ch]);
                    sys->channels[ch].is_running = false;
                }
            }

        } else if (is_home) {
            sys->op_mode = SYS_MODE_HOMING;
            sys->is_system_running = true;
            if (ch != -1) {
                if (ch >= 0 && ch < MAX_CHANNEL) {
                    sys->selected_channel = ch;
                    pump_manager_home_channel(ch);
                }
            } else {
                pump_manager_home_all();
            }
        }
        
        xSemaphoreGive(sys_state_sema); 
    } else {
        ESP_LOGE("UART_CMD", "Failed to acquire sys_state_sema in handle_command!");
    }

    cJSON_Delete(root);
}
static void pump_manager_start_channel(uint8_t channel_id){
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
            pump_manager_start_channel(sys->selected_channel);
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
            pump_manager_home_channel(sys->selected_channel);
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

static void pump_manager_home_all(void){

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