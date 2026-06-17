#include "button.h"
#include "pcf8574.h"

static const char *TAG = "BUTTON";

#define PCF_BTN_SELECT  1
#define PCF_BTN_UP      0
#define PCF_BTN_DOWN    4
#define PCF_BTN_LEFT    3
#define PCF_BTN_RIGHT   2

void button_init(void) {
    ESP_LOGI(TAG, "Button initialized in Polling Mode (No ISR, No Queue)");
}

static bool pcf_pin_to_event(uint8_t pin, system_event_t *evt) {
    switch (pin) {
        case PCF_BTN_SELECT: *evt = EVENT_SELECT; return true;
        case PCF_BTN_UP:     *evt = EVENT_UP;     return true;
        case PCF_BTN_DOWN:   *evt = EVENT_DOWN;   return true;
        case PCF_BTN_LEFT:   *evt = EVENT_LEFT;   return true;
        case PCF_BTN_RIGHT:  *evt = EVENT_RIGHT;  return true;
        default: return false;
    }
}

static const char *event_name(system_event_t evt) {
    switch (evt) {
        case EVENT_SELECT: return "SELECT";
        case EVENT_UP:     return "UP";
        case EVENT_DOWN:   return "DOWN";
        case EVENT_LEFT:   return "LEFT";
        case EVENT_RIGHT:  return "RIGHT";
        default:           return "UNKNOWN";
    }
}

void button_task(void *arg) {
    system_state_t *sys = (system_state_t *)arg;
    ESP_LOGI(TAG, "Button task started (POLLING MODE - 40ms ticks)");

    uint8_t prev_state = 0xFF; // HIGH = nhả nút, LOW = nhấn nút
    int consecutive_failures = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(40)); // Tần số quét 40ms cực kỳ tiết kiệm điện và chống nhiễu tuyệt đối

        uint8_t pcf_state = 0xFF;
        if (pcf8574_read(&pcf_state) != ESP_OK) {
            consecutive_failures++;
            
            // TỰ SỬA LỖI (Self-Healing): Nếu xảy ra 3 lần lỗi liên tiếp, thực hiện quét động I2C
            // để tìm và đăng ký lại PCF8574 với địa chỉ chính xác trên bus!
            if (consecutive_failures == 3) {
                ESP_LOGW(TAG, "PCF8574 read failed 3 times consecutively. Attempting dynamic auto-scan & recovery...");
                if (pcf8574_probe_and_reinit() == ESP_OK) {
                    ESP_LOGI(TAG, "Dynamic auto-recovery SUCCEEDED! Resuming button operations.");
                    consecutive_failures = 0;
                    continue;
                }
            }

            if (consecutive_failures < 5) {
                ESP_LOGE(TAG, "PCF8574 read FAILED — check I2C wiring! (attempt %d/5)", consecutive_failures);
                vTaskDelay(pdMS_TO_TICKS(1000));
            } else {
                if (consecutive_failures == 5) {
                    ESP_LOGE(TAG, "PCF8574 read FAILED consecutively! Throttling warning to every 5 seconds.");
                }
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
            continue;
        }
        consecutive_failures = 0;

        // Phát hiện sườn xuống (nhả sang nhấn): Bit nào ở prev_state là 1 (nhả) mà ở pcf_state là 0 (nhấn)
        uint8_t pressed_mask = prev_state & (~pcf_state);
        prev_state = pcf_state;

        if (pressed_mask == 0) {
            continue;
        }

        for (uint8_t pin = 0; pin < 8; pin++) {
            if (pressed_mask & (1 << pin)) {
                system_event_t evt;
                if (!pcf_pin_to_event(pin, &evt)) continue;

                ESP_LOGI(TAG, ">>> Button pressed: pin=%d event=%s screen=%d",
                         pin, event_name(evt), sys->ui.screen);

                if (tft_lock(pdMS_TO_TICKS(100))) {
                    switch (sys->ui.screen) {
                        case UI_MENU:           nav_menu(evt);           break;
                        case UI_SETTING:        nav_setting(evt);        break;
                        case UI_RUN:            nav_run(evt);            break;
                        case UI_SELECT_CHANNEL: nav_select_channel(evt); break;
                    }
                    tft_unlock();
                } else {
                    ESP_LOGE(TAG, "Failed to lock LVGL for navigation!");
                }

                // Debounce sau khi xử lý sự kiện nhấn để tránh hiện tượng double click ngoài ý muốn
                vTaskDelay(pdMS_TO_TICKS(200));

                // Cập nhật lại prev_state sau khi kết thúc debounce
                uint8_t current_state = 0xFF;
                if (pcf8574_read(&current_state) == ESP_OK) {
                    prev_state = current_state;
                }
                break;
            }
        }
    }
}