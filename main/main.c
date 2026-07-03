#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "tmc2209.h"
#include "tft.h"
#include "button.h"
#include "pump_manager.h"

void app_main(void)
{
    // 1. Khởi tạo Domain state đầu tiên
    system_state_init();

    // // 2. Khởi tạo phần cứng Màn hình SPI/LVGL
    tft_init();

    // 3. Khởi tạo quản lý bơm (AS5600, I2C, PCF8574)
    if(pump_manager_init() != ESP_OK){
        ESP_LOGE(TAG, "Failed to initialize pump manager");
        return;
    }   
    
    // // 4. Khởi tạo Driver Nút bấm (Polling Mode)
    // button_init();         

    // //Khởi tạo task màn hình và task nút nhấn
    xTaskCreate(tft_lvgl_task,  "lvgl",       8192, NULL,              2, NULL);
    xTaskCreate(button_task,    "button",     8192, (void*)system_get(), 7, NULL);
}
