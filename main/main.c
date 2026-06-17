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

    // 2. Khởi tạo phần cứng Màn hình SPI/LVGL
    tft_init();

    // 3. Khởi tạo quản lý bơm (AS5600, I2C, PCF8574)
    pump_manager_init();   // bên trong có pcf8574_init(bus_ch0, 0x20)
    
    // 4. Khởi tạo Driver Nút bấm (Polling Mode)
    button_init();         // sau pump_manager_init vì cần PCF8574 sẵn sàng

    // 5. Tạo các task chạy song song bảo đảm thứ tự ưu tiên
    xTaskCreate(tft_lvgl_task,  "lvgl",       8192, NULL,              2, NULL);
    xTaskCreate(button_task,    "button",     8192, (void*)system_get(), 7, NULL);
}
