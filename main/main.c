#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"

#include "mqtt.h"
#include "pump_channel.h"
#include "pump_manager.h"
#include "domain.h"
#include "tmc2209.h"

static const char *TAG = "MAIN_APP";

void app_main(void)
{


    /*
// Chỉ cần khởi tạo NVS và Event Loop hệ thống, còn lại network.c lo
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Gọi duy nhất hàm này để chạy mạng
    network_init();*/

    // Create data for 2 channel
 // 1. Khởi tạo hệ thống
    system_state_init();
    system_state_t *sys = system_get();
    
    // Khởi tạo quản lý bơm (Gán con trỏ stats cho motor)
    pump_manager_init();
    
    ESP_LOGI(TAG, "System State Initialized");

    // 2. Chạy Task quản lý logic (Sequential/Simultaneous)
    // Truyền NULL vì task này tự gọi system_get()
    xTaskCreate(pump_manager_task, "pump_mgr_logic", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Configuring Pump Parameters...");

    // 3. Cài đặt thông số tiêm cho Channel 0
    // Với Trapezoidal, các thông số này sẽ được motor_start "dịch" sang phần cứng
    sys->channels[0].volume_target = 0.1f;       // 1 ml
    sys->channels[0].flow_setpoint = 5.0f;      // 0.03 ml/h (Siêu chậm)
    sys->channels[0].acceleration  = 2.0f;       // 2 mm/s^2

    sys->op_mode = SYS_MODE_INDEPENDENT;

    // 4. Bắt đầu hệ thống
    // Hàm này sẽ gọi motor_start -> sync dữ liệu -> tính run_time_total
    pump_manager_system_start();
    ESP_LOGI(TAG, "Starting System...");

    while (1) {
        // Log dồn 1 dòng cho Channel 0
        ESP_LOGI(TAG, "CH0 | Vol: %.4f/%.2f ml | Flow: %.4f ml/h | Time: %.1f/%.1f s | S:%d Stp:%lu",
                 sys->channels[0].volume_infused, sys->channels[0].volume_target,
                 sys->channels[0].flow_actual, sys->channels[0].time_run,
                 sys->channels[0].run_time_total, sys->channels[0].state,
                 sys->channels[0].current_steps);

        // Tương tự cho Channel 1 (nếu bác đang dùng chế độ song song)
        if (sys->op_mode == SYS_MODE_SIMULTANEOUS) {
            ESP_LOGI(TAG, "CH1 | Vol: %.4f/%.2f ml | Flow: %.4f ml/h | Time: %.1f/%.1f s | S:%d Stp:%lu",
                     sys->channels[1].volume_infused, sys->channels[1].volume_target,
                     sys->channels[1].flow_actual, sys->channels[1].time_run,
                     sys->channels[1].run_time_total, sys->channels[1].state,
                     sys->channels[1].current_steps);
        }

        // Kiểm tra kết thúc toàn bộ hệ thống
        if (sys->channels[0].state == PUMP_DONE && sys->channels[1].state == PUMP_DONE) {
            if (sys->is_system_running) {
                ESP_LOGW(TAG, ">>> ALL CHANNELS COMPLETED <<<");
                sys->is_system_running = false;
            }
        }
        // Cập nhật Log mỗi giây để theo dõi cho mượt
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}
