#pragma once

#include <stdint.h>
#include "esp_lcd_types.h"
#include "lvgl.h"
#include "domain.h"
#include "io_config.h" 
#include "driver/gpio.h"  
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


// ============================================================
//  GPIO Màn hình ILI9341 (SPI)
// ============================================================

// ============================================================
//  Thông số màn hình - Landscape 320x240
// ============================================================
#define LCD_H_RES           320
#define LCD_V_RES           240
#define LCD_BIT_PER_PIXEL   16
#define LCD_DRAW_BUFF_LINES 10

// ============================================================
//  Màu sắc giao diện
// ============================================================
#define COLOR_BG            lv_color_hex(0x0D1117)  // Đen rất tối, gần đen hoàn toàn
#define COLOR_PANEL         lv_color_hex(0x161B22)  // Đen hơi xanh đậm
#define COLOR_BORDER        lv_color_hex(0x30363D)  // Xám tối
#define COLOR_ACCENT        lv_color_hex(0x58A6FF)  // Xanh dương nhạt (như GitHub)
#define COLOR_GREEN         lv_color_hex(0x3FB950)  // Xanh lá
#define COLOR_YELLOW        lv_color_hex(0xD29922)  // Vàng cam
#define COLOR_RED           lv_color_hex(0xF85149)  // Đỏ
#define COLOR_TEXT_PRIMARY  lv_color_hex(0xE6EDF3)  // Trắng hơi xanh
#define COLOR_TEXT_MUTED    lv_color_hex(0x8B949E)  // Xám
#define COLOR_HIGHLIGHT     lv_color_hex(0x1F6FEB)  // Xanh dương đậm

// ============================================================
//  Hằng số UI
// ============================================================
#define MENU_ITEM_COUNT     4       // INDEPENDENT, SIMULTANEOUS, SEQUENTIAL, HOMING
#define FLOW_STEP           0.01f   // Bước chỉnh flow (ml/h)
#define VOL_STEP            0.1f    // Bước chỉnh volume (ml)

// ============================================================
//  KHỞI TẠO & TASK
// ============================================================

/**
 * @brief Khởi tạo SPI bus, ILI9341 driver, LVGL, flush callback.
 *        Gọi một lần duy nhất trong app_main.
 */
void tft_init(void);

/**
 * @brief Task LVGL timer handler.
 *        Tạo bằng: xTaskCreate(tft_lvgl_task, "lvgl", 8192, NULL, 5, NULL)
 */
void tft_lvgl_task(void *pvParameters);
bool tft_lock(TickType_t timeout);
void tft_unlock(void);

// ============================================================
//  ĐIỀU HƯỚNG MÀN HÌNH
// ============================================================
void tft_show_menu(void);
void tft_show_setting(void);
void tft_show_run(void);

// ============================================================
//  CẬP NHẬT DỮ LIỆU  (gọi từ log_task mỗi giây)
// ============================================================
void tft_update_run_screen(const system_state_t *sys);
void tft_update_setting_screen(const system_state_t *sys);

/**
 * @brief Xử lý nút nhấn trên màn hình MENU.
 *        UP/DOWN: di chuyển cursor.
 *        SELECT: chọn mode → chuyển sang RUN.
 *        RIGHT: chuyển sang SETTING.
 *
 * @param pin  GPIO pin (BTN_*_PIN)
 */
void nav_menu(system_event_t evt);

/**
 * @brief Xử lý nút nhấn trên màn hình SETTING.
 *        UP/DOWN: tăng/giảm giá trị ô đang chọn.
 *        LEFT/RIGHT: di chuyển giữa các ô.
 *        SELECT: quay về MENU.
 *
 * @param pin  GPIO pin (BTN_*_PIN)
 */
void nav_setting(system_event_t evt);

/**
 * @brief Xử lý nút nhấn trên màn hình RUN.
 *        SELECT: toggle Start/Stop.
 *        LEFT: chuyển sang SETTING.
 *        RIGHT: chuyển sang MENU.
 *
 * @param pin  GPIO pin (BTN_*_PIN)
 */
void nav_run(system_event_t evt);
void nav_select_channel(system_event_t evt);
void tft_show_select_channel(void);
void tft_test_fill_red(void);