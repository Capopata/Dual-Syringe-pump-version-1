#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "domain.h"
#include "pump_manager.h"

const char *TAG = "MAIN";

void app_main(void)
{
    // 1. Khởi tạo Domain state đầu tiên
    system_state_init();

    esp_err_t result = pump_manager_init();

    // 2. Khởi tạo quản lý bơm (AS5600, I2C)
    if(result != ESP_OK){
        ESP_LOGE(TAG, "Failed to initialize pump manager!");
        return ESP_FAIL;
    }   

    ESP_LOGI(TAG,"Initialization successful!" );
}
