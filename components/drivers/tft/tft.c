#include "tft.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "pump_manager.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "TFT";
static const char *TAG_NAV = "NAV";

// ============================================================
//  Biến nội bộ - phần cứng & LVGL
// ============================================================
static esp_lcd_panel_handle_t s_panel        = NULL;
static lv_disp_drv_t          s_disp_drv;
static lv_disp_draw_buf_t     s_draw_buf;
static lv_color_t             s_buf1[LCD_H_RES * LCD_DRAW_BUFF_LINES];
static lv_color_t             s_buf2[LCD_H_RES * LCD_DRAW_BUFF_LINES];
static SemaphoreHandle_t      s_lvgl_mux    = NULL;

ui_screen_t g_current_screen = UI_MENU;

// ============================================================
//  Màn hình LVGL
// ============================================================
static lv_obj_t *s_scr_menu    = NULL;
static lv_obj_t *s_scr_setting = NULL;
static lv_obj_t *s_scr_run     = NULL;
static lv_obj_t *s_scr_select  = NULL;
static lv_obj_t *s_ch_cards[2] = {NULL, NULL};

// ============================================================
//  Widget RUN
// ============================================================
typedef struct {
    lv_obj_t *state_dot;
    lv_obj_t *state_label;
    lv_obj_t *flow_sp_label;
    lv_obj_t *flow_act_label;
    lv_obj_t *vol_target_label;
    lv_obj_t *vol_infused_label;
    lv_obj_t *vol_bar;
    lv_obj_t *time_label;
} ch_widget_t;

static ch_widget_t  s_ch[MAX_CHANNEL];
static lv_obj_t    *s_mode_label_run    = NULL;
static lv_obj_t    *s_sys_status_label  = NULL;

// ============================================================
//  Widget SETTING
// ============================================================
typedef struct {
    lv_obj_t *flow_val;
    lv_obj_t *vol_val;
    lv_obj_t *cursor;
} setting_widget_t;

static setting_widget_t s_sw[MAX_CHANNEL];

// ============================================================
//  Widget MENU
// ============================================================
static lv_obj_t *s_menu_rows[MENU_ITEM_COUNT];
static lv_obj_t *s_menu_cursor = NULL;

// ============================================================
//  Trạng thái điều hướng nội bộ
// ============================================================
static int8_t s_menu_row    = 0;          // con trỏ menu (0..3)
// Setting: 4 ô theo thứ tự flow_ch0 → vol_ch0 → flow_ch1 → vol_ch1
static int8_t s_setting_row = 0;          // 0..3

// ============================================================
//  LVGL flush callback
// ============================================================
static void _flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    static int count = 0;
    if (count < 5) {
        ESP_LOGI("TFT", "flush #%d: x=%d..%d y=%d..%d", 
                 count, area->x1, area->x2, area->y1, area->y2);
        count++;
    }

    // Vô hiệu hóa hoán đổi byte thủ công vì CONFIG_LV_COLOR_16_SWAP=y đã hoán đổi sẵn trong LVGL
    esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t)drv->user_data,
                              area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1,
                              color_p);
    lv_disp_flush_ready(drv);
}

// ============================================================
//  Helper: tạo panel nền
// ============================================================
static lv_obj_t *_panel(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_color(p, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(p, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(p, 1, 0);
    lv_obj_set_style_radius(p, 4, 0);
    lv_obj_set_style_pad_all(p, 6, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

// ============================================================
//  Helper: tạo label
// ============================================================
static lv_obj_t *_label(lv_obj_t *parent, const char *text,
                         lv_color_t color, int x, int y)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

// ============================================================
//  Helper: chuỗi state / màu
// ============================================================
static const char *_state_str(pump_state_t s)
{
    switch (s) {
        case PUMP_IDLE:   return "IDLE";
        case PUMP_RUN:    return "RUN";
        case PUMP_PAUSED: return "PAUSED";
        case PUMP_DONE:   return "DONE";
        case PUMP_HOMING: return "HOMING";
        case PUMP_ERROR:  return "ERROR";
        default:          return "???";
    }
}

static lv_color_t _state_color(pump_state_t s)
{
    switch (s) {
        case PUMP_RUN:    return COLOR_GREEN;
        case PUMP_DONE:   return COLOR_YELLOW;
        case PUMP_ERROR:  return COLOR_RED;
        case PUMP_HOMING: return COLOR_ACCENT;
        default:          return COLOR_TEXT_MUTED;
    }
}

static const char *_mode_str(sys_op_mode_t m)
{
    switch (m) {
        case SYS_MODE_INDEPENDENT:  return "INDEPENDENT";
        case SYS_MODE_SIMULTANEOUS: return "SIMULTANEOUS";
        case SYS_MODE_SEQUENTIAL:   return "SEQUENTIAL";
        case SYS_MODE_HOMING:       return "HOMING";
        default:                    return "???";
    }
}

// ============================================================
//  Tạo màn hình MENU
// ============================================================
static void _create_menu(void)
{
    s_scr_menu = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr_menu, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_scr_menu, LV_OPA_COVER, 0);

    // Tiêu đề
    lv_obj_t *title = _label(s_scr_menu, "PUMP SYSTEM - SELECT MODE",
                              COLOR_ACCENT, 10, 6);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    // Hint
    lv_obj_t *hint = _label(s_scr_menu,
        "UP/DOWN: Di chuyen   SELECT: Chon mode   RIGHT: Setting",
        COLOR_TEXT_MUTED, 6, 222);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);

    const char *names[MENU_ITEM_COUNT] = {
        "  INDEPENDENT   - Cac kenh doc lap",
        "  SIMULTANEOUS  - Chay dong thoi",
        "  SEQUENTIAL    - Chay lien tiep",
        "  HOMING        - Ve goc ban dau",
    };

    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        lv_obj_t *row = _panel(s_scr_menu, 10, 28 + i * 46, 300, 40);
        lv_obj_t *lbl = _label(row, names[i], COLOR_TEXT_PRIMARY, 4, 10);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        s_menu_rows[i] = row;
    }

    // Cursor viền highlight
    s_menu_cursor = lv_obj_create(s_scr_menu);
    lv_obj_set_size(s_menu_cursor, 304, 44);
    lv_obj_set_style_bg_opa(s_menu_cursor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(s_menu_cursor, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(s_menu_cursor, 2, 0);
    lv_obj_set_style_radius(s_menu_cursor, 4, 0);
    lv_obj_clear_flag(s_menu_cursor, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_menu_cursor, 8, 26 + s_menu_row * 46);
}
// static void _create_menu(void)
// {
//     s_scr_menu = lv_obj_create(NULL);
//     // Đổi màu nền thành ĐỎ để test
//     lv_obj_set_style_bg_color(s_scr_menu, lv_color_hex(0xFF0000), 0);
//     lv_obj_set_style_bg_opa(s_scr_menu, LV_OPA_COVER, 0);

//     lv_obj_t *t = lv_label_create(s_scr_menu);
//     lv_label_set_text(t, "HELLO");
//     lv_obj_set_style_text_color(t, lv_color_hex(0xFFFFFF), 0);
//     lv_obj_set_pos(t, 10, 10);
// }
// ============================================================
//  Tạo màn hình SETTING
// ============================================================
static void _create_setting(void)
{
    s_scr_setting = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr_setting, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_scr_setting, LV_OPA_COVER, 0);

    lv_obj_t *title = _label(s_scr_setting, "SETTING - PUMP PARAMETERS",
                              COLOR_ACCENT, 10, 6);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    lv_obj_t *hint = _label(s_scr_setting,
        "UP/DOWN: +/-   LEFT/RIGHT: Chuyen o   SELECT: Quay Menu",
        COLOR_TEXT_MUTED, 4, 222);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);

    system_state_t *sys = system_get();
    const int px[2] = {5, 162};
    char buf[16];

    for (int ch = 0; ch < MAX_CHANNEL; ch++) {
        lv_obj_t *panel = _panel(s_scr_setting, px[ch], 26, 152, 190);

        // Tên channel
        snprintf(buf, sizeof(buf), "CHANNEL %d", ch);
        lv_obj_t *ch_lbl = _label(panel, buf, COLOR_ACCENT, 4, 0);
        lv_obj_set_style_text_font(ch_lbl, &lv_font_montserrat_12, 0);

        // --- Flow Setpoint ---
        _label(panel, "Flow Setpoint (ml/h)", COLOR_TEXT_MUTED, 4, 20);
        snprintf(buf, sizeof(buf), "%.3f", sys->channels[ch].flow_setpoint);
        lv_obj_t *fv = lv_label_create(panel);
        lv_label_set_text(fv, buf);
        lv_obj_set_style_text_color(fv, COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(fv, &lv_font_montserrat_14, 0);
        lv_obj_set_pos(fv, 4, 36);
        s_sw[ch].flow_val = fv;

        // Divider
        lv_obj_t *div = lv_obj_create(panel);
        lv_obj_set_pos(div, 0, 60);
        lv_obj_set_size(div, 138, 1);
        lv_obj_set_style_bg_color(div, COLOR_BORDER, 0);
        lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);

        // --- Volume Target ---
        _label(panel, "Volume Target (ml)", COLOR_TEXT_MUTED, 4, 68);
        snprintf(buf, sizeof(buf), "%.2f", sys->channels[ch].volume_target);
        lv_obj_t *vv = lv_label_create(panel);
        lv_label_set_text(vv, buf);
        lv_obj_set_style_text_color(vv, COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(vv, &lv_font_montserrat_14, 0);
        lv_obj_set_pos(vv, 4, 84);
        s_sw[ch].vol_val = vv;

        // Cursor box (di chuyển theo ô đang chọn)
        lv_obj_t *cur = lv_obj_create(panel);
        lv_obj_set_size(cur, 140, 28);
        lv_obj_set_style_bg_opa(cur, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(cur, COLOR_HIGHLIGHT, 0);
        lv_obj_set_style_border_width(cur, 2, 0);
        lv_obj_set_style_radius(cur, 3, 0);
        lv_obj_set_pos(cur, 0, 32);     // mặc định ở hàng flow
        lv_obj_clear_flag(cur, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_border_opa(cur,
            (ch == 0) ? LV_OPA_COVER : LV_OPA_TRANSP, 0);  // ch0 hiện mặc định
        s_sw[ch].cursor = cur;
    }
}

// ============================================================
//  Tạo màn hình RUN
// ============================================================
static void _create_run(void)
{
    s_scr_run = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr_run, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_scr_run, LV_OPA_COVER, 0);

    // Header bar
    lv_obj_t *hdr = lv_obj_create(s_scr_run);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_size(hdr, 320, 24);
    lv_obj_set_style_bg_color(hdr, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    system_state_t *sys = system_get();
    char buf[40];

    snprintf(buf, sizeof(buf), "MODE: %s", _mode_str(sys->op_mode));
    s_mode_label_run = _label(hdr, buf, COLOR_ACCENT, 6, 4);
    lv_obj_set_style_text_font(s_mode_label_run, &lv_font_montserrat_12, 0);

    s_sys_status_label = _label(hdr,
        sys->is_system_running ? "RUNNING" : "STOPPED",
        sys->is_system_running ? COLOR_GREEN : COLOR_RED,
        230, 4);
    lv_obj_set_style_text_font(s_sys_status_label, &lv_font_montserrat_12, 0);

    // Hint
    lv_obj_t *hint = _label(s_scr_run,
        "SELECT: Start/Stop   LEFT: Setting   RIGHT: Menu",
        COLOR_TEXT_MUTED, 6, 222);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);

    // 2 channel card
    const int px[2] = {4, 162};
    const int pw = 154, ph = 192, py = 26;

    for (int ch = 0; ch < MAX_CHANNEL; ch++) {
        lv_obj_t *card = _panel(s_scr_run, px[ch], py, pw, ph);
        ch_widget_t *w = &s_ch[ch];

        snprintf(buf, sizeof(buf), "CHANNEL %d", ch);
        _label(card, buf, COLOR_ACCENT, 4, 0);

        // Status dot
        lv_obj_t *dot = lv_obj_create(card);
        lv_obj_set_pos(dot, pw - 26, 2);
        lv_obj_set_size(dot, 12, 12);
        lv_obj_set_style_bg_color(dot, COLOR_TEXT_MUTED, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        w->state_dot = dot;

        // State text
        lv_obj_t *st = lv_label_create(card);
        lv_label_set_text(st, "IDLE");
        lv_obj_set_style_text_color(st, COLOR_TEXT_MUTED, 0);
        lv_obj_set_style_text_font(st, &lv_font_montserrat_12, 0);
        lv_obj_set_pos(st, 4, 16);
        w->state_label = st;

        // Divider
        lv_obj_t *div = lv_obj_create(card);
        lv_obj_set_pos(div, 0, 32); lv_obj_set_size(div, pw - 14, 1);
        lv_obj_set_style_bg_color(div, COLOR_BORDER, 0);
        lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);

        _label(card, "Flow SP",   COLOR_TEXT_MUTED, 4, 38);
        lv_obj_t *fsp = lv_label_create(card);
        lv_label_set_text(fsp, "0.000 ml/h");
        lv_obj_set_style_text_color(fsp, COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(fsp, &lv_font_montserrat_12, 0);
        lv_obj_set_pos(fsp, 4, 50);
        w->flow_sp_label = fsp;

        _label(card, "Flow Act",  COLOR_TEXT_MUTED, 4, 66);
        lv_obj_t *fact = lv_label_create(card);
        lv_label_set_text(fact, "0.000 ml/h");
        lv_obj_set_style_text_color(fact, COLOR_GREEN, 0);
        lv_obj_set_style_text_font(fact, &lv_font_montserrat_12, 0);
        lv_obj_set_pos(fact, 4, 78);
        w->flow_act_label = fact;

        lv_obj_t *div2 = lv_obj_create(card);
        lv_obj_set_pos(div2, 0, 95); lv_obj_set_size(div2, pw - 14, 1);
        lv_obj_set_style_bg_color(div2, COLOR_BORDER, 0);
        lv_obj_clear_flag(div2, LV_OBJ_FLAG_SCROLLABLE);

        _label(card, "Vol Target",  COLOR_TEXT_MUTED, 4, 101);
        lv_obj_t *vt = lv_label_create(card);
        lv_label_set_text(vt, "0.00 ml");
        lv_obj_set_style_text_color(vt, COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(vt, &lv_font_montserrat_12, 0);
        lv_obj_set_pos(vt, 4, 113);
        w->vol_target_label = vt;

        _label(card, "Vol Infused", COLOR_TEXT_MUTED, 4, 129);
        lv_obj_t *vi = lv_label_create(card);
        lv_label_set_text(vi, "0.0000 ml");
        lv_obj_set_style_text_color(vi, COLOR_YELLOW, 0);
        lv_obj_set_style_text_font(vi, &lv_font_montserrat_12, 0);
        lv_obj_set_pos(vi, 4, 141);
        w->vol_infused_label = vi;

        // Progress bar
        lv_obj_t *bar = lv_bar_create(card);
        lv_obj_set_pos(bar, 4, 158);
        lv_obj_set_size(bar, pw - 22, 8);
        lv_bar_set_range(bar, 0, 100);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, COLOR_BORDER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, COLOR_GREEN, LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);
        w->vol_bar = bar;

        _label(card, "Time", COLOR_TEXT_MUTED, 4, 172);
        lv_obj_t *tm = lv_label_create(card);
        lv_label_set_text(tm, "0.0 s");
        lv_obj_set_style_text_color(tm, COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(tm, &lv_font_montserrat_12, 0);
        lv_obj_set_pos(tm, 46, 172);
        w->time_label = tm;
    }
}
static bool _flush_ready_cb(esp_lcd_panel_io_handle_t io,
                             esp_lcd_panel_io_event_data_t *edata,
                             void *user_ctx)
{
    lv_disp_drv_t *drv = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(drv);
    return false;
}
// ============================================================
//  Khởi tạo phần cứng
// ============================================================
void tft_init(void)
{
    // 1. Đèn nền
    gpio_config_t bk = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BK_LIGHT,
    };
    gpio_config(&bk);
    gpio_set_level(PIN_NUM_BK_LIGHT, 1);

    // 2. SPI Bus
    spi_bus_config_t buscfg = {
        .sclk_io_num   = PIN_NUM_SCLK,
        .mosi_io_num   = PIN_NUM_MOSI,
        .miso_io_num   = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        // LCD_H_RES=320, LCD_V_RES=240 (landscape)
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 3. Panel IO
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = PIN_NUM_LCD_DC,
        .cs_gpio_num       = PIN_NUM_LCD_CS,
        .pclk_hz           = 20 * 1000 * 1000,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        .trans_queue_depth = 10,

        // .on_color_trans_done      = _flush_ready_cb,  // ← thêm
        // .user_ctx                 = &s_disp_drv,      // ← thêm
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io));

    // 4. ILI9341 driver
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
//        .data_endian    = LCD_RGB_DATA_ENDIAN_BIG,  
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io, &panel_cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));

    // ----------------------------------------------------------------
    // Landscape 320x240:
    //   swap_xy  = true  → hoán đổi trục X/Y (portrait → landscape)
    //   mirror_x = true  → lật ngang cho đúng chiều đọc
    //   mirror_y = false
    // Nếu hình vẫn bị lật/ngược, thử: mirror(false,false) hoặc mirror(true,true)
    // ----------------------------------------------------------------
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    // 5. LVGL
    lv_init();
    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2,
                          LCD_H_RES * LCD_DRAW_BUFF_LINES);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.sw_rotate = 0;
    s_disp_drv.rotated = LV_DISP_ROT_NONE;
//    s_disp_drv.color_swap       = 1; 
    s_disp_drv.offset_x = 0;
    s_disp_drv.offset_y = 0;
    //s_disp_drv.color_format = LCD_COLOR_FMT_RGB565;
    s_disp_drv.hor_res   = LCD_H_RES;   // 320
    s_disp_drv.ver_res   = LCD_V_RES;   // 240
    s_disp_drv.flush_cb  = _flush_cb;
    s_disp_drv.draw_buf  = &s_draw_buf;
    s_disp_drv.user_data = s_panel;
    // s_disp_drv.full_refresh = 1;
    lv_disp_drv_register(&s_disp_drv);

    // 6. Mutex bảo vệ LVGL API (dùng recursive mutex để tránh deadlock khi lồng nhau)
    s_lvgl_mux = xSemaphoreCreateRecursiveMutex();

    ESP_LOGI(TAG, "TFT ILI9341 + LVGL OK (320x240 Landscape)");

    // TEST: fill đỏ toàn màn, bypass LVGL
    static uint16_t test_buf[320 * 10];
    memset(test_buf, 0, sizeof(test_buf));

    // RGB565: đỏ = 0xF800, sau swap byte = 0x00F8
    for (int i = 0; i < 320 * 10; i++) {
        test_buf[i] = 0x00F8;   // đỏ big-endian
    }

    for (int y = 0; y < 240; y += 10) {
        esp_lcd_panel_draw_bitmap(s_panel, 0, y, 320, y + 10, test_buf);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));  // nhìn 2 giây
}

// ============================================================
//  Hiển thị màn hình
// ============================================================
void tft_show_menu(void)
{
    if (!s_scr_menu) _create_menu();
    g_current_screen = UI_MENU;
    ESP_LOGI("TFT", "loading screen %p, active=%p", s_scr_menu, lv_scr_act());
    lv_scr_load(s_scr_menu);
    ESP_LOGI("TFT", "after load active=%p", lv_scr_act());
}

void tft_show_setting(void)
{
    if (!s_scr_setting) _create_setting();
    g_current_screen = UI_SETTING;
    // Reset về ô đầu tiên mỗi lần vào Setting
    s_setting_row = 0;
    tft_update_setting_screen(system_get());
    lv_scr_load(s_scr_setting);
}

void tft_show_run(void)
{
    if (!s_scr_run) _create_run();
    g_current_screen = UI_RUN;
    tft_update_run_screen(system_get());
    lv_scr_load(s_scr_run);
}

static void _create_select_channel(void)
{
    s_scr_select = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr_select, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_scr_select, LV_OPA_COVER, 0);

    // Tiêu đề
    lv_obj_t *title = _label(s_scr_select, "SELECT ACTIVE CHANNEL", COLOR_ACCENT, 10, 6);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    // Hint
    lv_obj_t *hint = _label(s_scr_select,
        "LEFT/RIGHT: Chuyen kenh   SELECT: Xac nhan",
        COLOR_TEXT_MUTED, 10, 222);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);

    const int px[2] = {20, 170};
    const int pw = 130, ph = 160, py = 40;

    for (int ch = 0; ch < 2; ch++) {
        lv_obj_t *card = _panel(s_scr_select, px[ch], py, pw, ph);
        
        char buf[16];
        snprintf(buf, sizeof(buf), "CHANNEL %d", ch);
        lv_obj_t *lbl = _label(card, buf, COLOR_TEXT_PRIMARY, 20, 60);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        
        snprintf(buf, sizeof(buf), "Pump %s", (ch == 0) ? "Left" : "Right");
        lv_obj_t *sub_lbl = _label(card, buf, COLOR_TEXT_MUTED, 35, 85);
        lv_obj_set_style_text_font(sub_lbl, &lv_font_montserrat_10, 0);
        
        s_ch_cards[ch] = card;
    }
}

void tft_update_select_channel_screen(const system_state_t *sys)
{
    if (!s_scr_select) return;
    if (xSemaphoreTakeRecursive(s_lvgl_mux, pdMS_TO_TICKS(20)) != pdTRUE) return;

    uint8_t sel = sys->selected_channel;
    for (int ch = 0; ch < 2; ch++) {
        if (ch == sel) {
            lv_obj_set_style_border_color(s_ch_cards[ch], COLOR_HIGHLIGHT, 0);
            lv_obj_set_style_border_width(s_ch_cards[ch], 3, 0);
            lv_obj_set_style_bg_color(s_ch_cards[ch], COLOR_PANEL, 0);
        } else {
            lv_obj_set_style_border_color(s_ch_cards[ch], COLOR_BORDER, 0);
            lv_obj_set_style_border_width(s_ch_cards[ch], 1, 0);
            lv_obj_set_style_bg_color(s_ch_cards[ch], COLOR_PANEL, 0);
        }
    }

    xSemaphoreGiveRecursive(s_lvgl_mux);
}

void tft_show_select_channel(void)
{
    if (!s_scr_select) _create_select_channel();
    g_current_screen = UI_SELECT_CHANNEL;
    tft_update_select_channel_screen(system_get());
    lv_scr_load(s_scr_select);
}

// ============================================================
//  Cập nhật màn hình RUN  (gọi từ log_task mỗi 1 giây)
// ============================================================
void tft_update_run_screen(const system_state_t *sys)
{
    if (!s_scr_run) return;
    if (xSemaphoreTakeRecursive(s_lvgl_mux, pdMS_TO_TICKS(20)) != pdTRUE) return;

    char buf[32];

    // Header
    snprintf(buf, sizeof(buf), "MODE: %s", _mode_str(sys->op_mode));
    lv_label_set_text(s_mode_label_run, buf);

    lv_label_set_text(s_sys_status_label,
        sys->is_system_running ? "RUNNING" : "STOPPED");
    lv_obj_set_style_text_color(s_sys_status_label,
        sys->is_system_running ? COLOR_GREEN : COLOR_RED, 0);

    // Từng channel
    for (int ch = 0; ch < MAX_CHANNEL; ch++) {
        const pump_channel_t *c = &sys->channels[ch];
        ch_widget_t          *w = &s_ch[ch];

        lv_label_set_text(w->state_label, _state_str(c->state));
        lv_obj_set_style_text_color(w->state_label, _state_color(c->state), 0);
        lv_obj_set_style_bg_color(w->state_dot, _state_color(c->state), 0);

        snprintf(buf, sizeof(buf), "%.3f ml/h", c->flow_setpoint);
        lv_label_set_text(w->flow_sp_label, buf);

        snprintf(buf, sizeof(buf), "%.3f ml/h", c->flow_actual);
        lv_label_set_text(w->flow_act_label, buf);

        snprintf(buf, sizeof(buf), "%.2f ml", c->volume_target);
        lv_label_set_text(w->vol_target_label, buf);

        snprintf(buf, sizeof(buf), "%.4f ml", c->volume_infused);
        lv_label_set_text(w->vol_infused_label, buf);

        int32_t pct = 0;
        if (c->volume_target > 0.0f)
            pct = (int32_t)((c->volume_infused / c->volume_target) * 100.0f);
        if (pct > 100) pct = 100;
        if (pct < 0)   pct = 0;
        lv_bar_set_value(w->vol_bar, pct, LV_ANIM_OFF);

        snprintf(buf, sizeof(buf), "%.1f s", c->time_run);
        lv_label_set_text(w->time_label, buf);
    }

    xSemaphoreGiveRecursive(s_lvgl_mux);
}

// ============================================================
//  Cập nhật màn hình SETTING
// ============================================================
void tft_update_setting_screen(const system_state_t *sys)
{
    if (!s_scr_setting) return;
    if (xSemaphoreTakeRecursive(s_lvgl_mux, pdMS_TO_TICKS(20)) != pdTRUE) return;

    char buf[16];
    for (int ch = 0; ch < MAX_CHANNEL; ch++) {
        snprintf(buf, sizeof(buf), "%.3f", sys->channels[ch].flow_setpoint);
        lv_label_set_text(s_sw[ch].flow_val, buf);

        snprintf(buf, sizeof(buf), "%.2f", sys->channels[ch].volume_target);
        lv_label_set_text(s_sw[ch].vol_val, buf);
    }

    // Di chuyển cursor: s_setting_row 0=flow_ch0, 1=vol_ch0, 2=flow_ch1, 3=vol_ch1
    uint8_t cur_ch  = (s_setting_row < 2) ? 0 : 1;
    uint8_t cur_row = s_setting_row % 2;  // 0=flow, 1=vol

    for (int ch = 0; ch < MAX_CHANNEL; ch++)
        lv_obj_set_style_border_opa(s_sw[ch].cursor, LV_OPA_TRANSP, 0);

    lv_obj_set_style_border_opa(s_sw[cur_ch].cursor, LV_OPA_COVER, 0);
    lv_obj_set_pos(s_sw[cur_ch].cursor, 0, (cur_row == 0) ? 32 : 80);

    xSemaphoreGiveRecursive(s_lvgl_mux);
}
void tft_test_fill_red(void)
{
    uint16_t *buf = heap_caps_malloc(320 * 10 * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) return;

    // Thử đỏ RGB565 = 0xF800
    for (int i = 0; i < 320 * 10; i++) buf[i] = 0xF800;

    for (int y = 0; y < 240; y += 10)
        esp_lcd_panel_draw_bitmap(s_panel, 0, y, 320, y + 10, buf);

    free(buf);
}

// ============================================================
//  Logic điều hướng theo từng màn hình
// ============================================================
// nav_menu, nav_setting giữ logic cũ, chỉ đổi tham số + so sánh
void nav_menu(system_event_t evt) {
    system_state_t *sys = system_get();
    if (evt == EVENT_UP) {
        s_menu_row = (s_menu_row - 1 + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
        lv_obj_set_pos(s_menu_cursor, 8, 26 + s_menu_row * 46);
        ESP_LOGI(TAG_NAV, "[MENU] UP → row=%d", s_menu_row);
    } else if (evt == EVENT_DOWN) {
        s_menu_row = (s_menu_row + 1) % MENU_ITEM_COUNT;
        lv_obj_set_pos(s_menu_cursor, 8, 26 + s_menu_row * 46);
        ESP_LOGI(TAG_NAV, "[MENU] DOWN → row=%d", s_menu_row);
    } else if (evt == EVENT_SELECT) {
        if(xSemaphoreTake(sys_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE){
            if (s_menu_row == 0) {
                sys->op_mode = SYS_MODE_INDEPENDENT;
                g_current_screen = UI_SELECT_CHANNEL;
                ESP_LOGI(TAG_NAV, "[MENU] SELECT (INDEP) → UI_SELECT_CHANNEL");
                tft_show_select_channel();
            } else if (s_menu_row == 1) {
                sys->op_mode = SYS_MODE_SIMULTANEOUS;
                ESP_LOGI(TAG_NAV, "[MENU] SELECT (SIMUL) → tft_show_run");
                tft_show_run();
            } else if (s_menu_row == 2) {
                sys->op_mode = SYS_MODE_SEQUENTIAL;
                ESP_LOGI(TAG_NAV, "[MENU] SELECT (SEQ) → tft_show_run");
                tft_show_run();
            } else if (s_menu_row == 3) {
                sys->op_mode = SYS_MODE_HOMING;
                g_current_screen = UI_SELECT_CHANNEL;
                ESP_LOGI(TAG_NAV, "[MENU] SELECT (HOMING) → UI_SELECT_CHANNEL");
                tft_show_select_channel();
            }
            xSemaphoreGive(sys_state_mutex);
        }
    } else if (evt == EVENT_RIGHT) {
        ESP_LOGI(TAG_NAV, "[MENU] RIGHT → tft_show_setting");
        tft_show_setting();
    }
}

void nav_setting(system_event_t evt) {
    system_state_t *sys = system_get();
    uint8_t cur_ch  = (s_setting_row < 2) ? 0 : 1;
    uint8_t cur_row = s_setting_row % 2;

    if(xSemaphoreTake(sys_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE){
        if (evt == EVENT_UP) {
            if (cur_row == 0) {
                sys->channels[cur_ch].flow_setpoint += FLOW_STEP;
                ESP_LOGI(TAG_NAV, "[SETTING] UP flow ch%d → %.3f", cur_ch, sys->channels[cur_ch].flow_setpoint);
            } else {
                sys->channels[cur_ch].volume_target += VOL_STEP;
                ESP_LOGI(TAG_NAV, "[SETTING] UP vol  ch%d → %.2f", cur_ch, sys->channels[cur_ch].volume_target);
            }
        } else if (evt == EVENT_DOWN) {
            if (cur_row == 0) {
                sys->channels[cur_ch].flow_setpoint -= FLOW_STEP;
                if (sys->channels[cur_ch].flow_setpoint < 0.001f) sys->channels[cur_ch].flow_setpoint = 0.001f;
                ESP_LOGI(TAG_NAV, "[SETTING] DOWN flow ch%d → %.3f", cur_ch, sys->channels[cur_ch].flow_setpoint);
            } else {
                sys->channels[cur_ch].volume_target -= VOL_STEP;
                if (sys->channels[cur_ch].volume_target < 0.1f) sys->channels[cur_ch].volume_target = 0.1f;
                ESP_LOGI(TAG_NAV, "[SETTING] DOWN vol  ch%d → %.2f", cur_ch, sys->channels[cur_ch].volume_target);
            }
        } else if (evt == EVENT_RIGHT) {
            s_setting_row = (s_setting_row + 1) % 4;
            ESP_LOGI(TAG_NAV, "[SETTING] RIGHT → row=%d (ch%d)", s_setting_row, (s_setting_row < 2) ? 0 : 1);
        } else if (evt == EVENT_LEFT) {
            s_setting_row = (s_setting_row + 3) % 4;
            ESP_LOGI(TAG_NAV, "[SETTING] LEFT  → row=%d (ch%d)", s_setting_row, (s_setting_row < 2) ? 0 : 1);
        } else if (evt == EVENT_SELECT) {
            ESP_LOGI(TAG_NAV, "[SETTING] SELECT → tft_show_menu");
            tft_show_menu();
            return;
        }
        xSemaphoreGive(sys_state_mutex);
    }

    tft_update_setting_screen(sys);
}

// nav_run — FIX: lấy mutex trước khi gọi LVGL API
void nav_run(system_event_t evt) {
    system_state_t *sys = system_get();
    if (evt == EVENT_SELECT) {
        if (xSemaphoreTake(sys_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (!sys->is_system_running) {
                ESP_LOGI(TAG_NAV, "[RUN] SELECT → pump_manager_system_start");
                pump_manager_system_start();
            } else {
                ESP_LOGI(TAG_NAV, "[RUN] SELECT → pump_manager_system_stop");
                pump_manager_system_stop();
            }
            xSemaphoreGive(sys_state_mutex);
        } else {
            ESP_LOGE(TAG_NAV, "nav_run: Failed to acquire sys_state_mutex");
        }
        if (xSemaphoreTakeRecursive(s_lvgl_mux, pdMS_TO_TICKS(50)) == pdTRUE) {
            lv_label_set_text(s_sys_status_label,
                sys->is_system_running ? "RUNNING" : "STOPPED");
            lv_obj_set_style_text_color(s_sys_status_label,
                sys->is_system_running ? COLOR_GREEN : COLOR_RED, 0);
            xSemaphoreGiveRecursive(s_lvgl_mux);
        }
    } else if (evt == EVENT_LEFT) {
        ESP_LOGI(TAG_NAV, "[RUN] LEFT → tft_show_setting");
        tft_show_setting();
    } else if (evt == EVENT_RIGHT) {
        ESP_LOGI(TAG_NAV, "[RUN] RIGHT → tft_show_menu");
        tft_show_menu();
    }
}
void nav_select_channel(system_event_t evt) {
    system_state_t *sys = system_get();
    if (xSemaphoreTake(sys_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (evt == EVENT_LEFT || evt == EVENT_RIGHT) {
            sys->selected_channel ^= 1;
            ESP_LOGI(TAG_NAV, "[SEL_CH] LEFT/RIGHT → selected_channel=%d", sys->selected_channel);
            tft_update_select_channel_screen(sys);
        } else if (evt == EVENT_SELECT) {
            if (sys->op_mode == SYS_MODE_HOMING) {
                sys->is_system_running = true;
                pump_manager_home_channel(sys->selected_channel);
            }
            ESP_LOGI(TAG_NAV, "[SEL_CH] SELECT → tft_show_run");
            tft_show_run();
        }
        xSemaphoreGive(sys_state_mutex);
    } else {
        ESP_LOGE(TAG_NAV, "nav_select_channel: Failed to acquire sys_state_mutex");
    }
    // Lưu ý: nhánh EVENT_LEFT trùng với nhánh đầu — xem ghi chú bên dưới
}

// ============================================================
//  LVGL Task (chạy lv_timer_handler liên tục)
// ============================================================
void tft_lvgl_task(void *pvParameters)
{
    ESP_LOGI("TFT", "LVGL task started");

    // Khởi tạo và hiển thị màn hình menu đầu tiên một cách an sau
    if (xSemaphoreTakeRecursive(s_lvgl_mux, portMAX_DELAY) == pdTRUE) {
        tft_show_menu();
        xSemaphoreGiveRecursive(s_lvgl_mux);
    }

    ESP_LOGI("TFT", "Entering loop");
    uint32_t last_update = 0;

    while (1) {
        if (xSemaphoreTakeRecursive(s_lvgl_mux, pdMS_TO_TICKS(10)) == pdTRUE) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            system_state_t *sys = system_get();
            
            // Tự động cập nhật màn hình RUN mỗi 500ms khi đang ở màn hình RUN
            if (g_current_screen == UI_RUN && (now - last_update >= 500)) {
                tft_update_run_screen(sys);
                last_update = now;
            }

            uint32_t delay = lv_timer_handler();
            xSemaphoreGiveRecursive(s_lvgl_mux);
            
            // Giới hạn thời gian ngủ tối đa (capping max delay) để đảm bảo LVGL task 
            // thức dậy xử lý các sự kiện bất đồng bộ từ button_task (như chuyển màn hình)
            if (delay > 30) {
                delay = 30;
            }
            vTaskDelay(pdMS_TO_TICKS(delay < 5 ? 5 : delay));
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

bool tft_lock(TickType_t timeout)
{
    if (!s_lvgl_mux) return false;
    return xSemaphoreTakeRecursive(s_lvgl_mux, timeout) == pdTRUE;
}

void tft_unlock(void)
{
    if (s_lvgl_mux) {
        xSemaphoreGiveRecursive(s_lvgl_mux);
    }
}
