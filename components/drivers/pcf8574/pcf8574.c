#include "pcf8574.h"
#include "freertos/FreeRTOS.h" 
#include "esp_log.h"

static const char *TAG = "PCF8574";
static i2c_master_bus_handle_t s_bus_handle = NULL;
static i2c_master_dev_handle_t s_dev = NULL;

esp_err_t pcf8574_init(i2c_master_bus_handle_t bus_handle, uint8_t addr)
{
    s_bus_handle = bus_handle; // Lưu lại bus handle để quét động tự sửa lỗi khi cần thiết
    uint8_t detected_addr = 0;

    // 1. Thử probe địa chỉ mặc định trước (Dùng timeout 150ms trực tiếp, không qua pdMS_TO_TICKS)
    esp_err_t probe_err = i2c_master_probe(bus_handle, addr, 150);
    if (probe_err == ESP_OK) {
        detected_addr = addr;
        ESP_LOGI(TAG, "PCF8574 found at default address: 0x%02X", addr);
    } else {
        ESP_LOGW(TAG, "PCF8574 NOT found at default address 0x%02X. Scanning other possible addresses...", addr);

        // 2. Quét các địa chỉ có thể có của PCF8574 (0x20 - 0x27) và PCF8574A (0x38 - 0x3F)
        const uint8_t possible_addrs[] = {
            0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, // PCF8574
            0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F  // PCF8574A
        };

        for (size_t i = 0; i < sizeof(possible_addrs) / sizeof(possible_addrs[0]); i++) {
            uint8_t a = possible_addrs[i];
            if (a == addr) continue;

            // Sử dụng timeout 100ms thực tế
            if (i2c_master_probe(bus_handle, a, 100) == ESP_OK) {
                detected_addr = a;
                ESP_LOGI(TAG, ">>> PCF8574 auto-detected at address: 0x%02X", a);
                break;
            }
        }
    }

    // 3. Dự phòng (Fallback): Nếu không quét được bất kỳ chip nào (ví dụ do kẹt bus lúc boot),
    // chúng ta vẫn đăng ký thiết bị tại địa chỉ mặc định để tự phục hồi khi bus I2C ổn định trở lại!
    if (detected_addr == 0) {
        ESP_LOGW(TAG, "No PCF8574/PCF8574A chip detected during boot scan. Fallback: Registering to default address 0x%02X.", addr);
        detected_addr = addr;

        ESP_LOGI(TAG, "Active devices on bus_ch0 during fallback scan:");
        bool found_any = false;
        for (uint8_t a = 0x08; a < 0x78; a++) {
            if (i2c_master_probe(bus_handle, a, 100) == ESP_OK) {
                ESP_LOGI(TAG, " - Device found at 0x%02X %s", a, (a == 0x36) ? "(AS5600)" : "");
                found_any = true;
            }
        }
        if (!found_any) {
            ESP_LOGE(TAG, " - No devices found on I2C bus. Check SCL/SDA pull-up resistors and power!");
        }
    }

    // 4. Đăng ký thiết bị với địa chỉ tìm được lên bus I2C
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = detected_addr,
        .scl_speed_hz    = 100000,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add PCF8574 to bus: %s", esp_err_to_name(ret));
        s_dev = NULL;
        return ret;
    }

    // Tất cả pin HIGH = input mode
    return pcf8574_write(0xFF);
}

bool pcf8574_is_present(void)
{
    return (s_dev != NULL);
}

esp_err_t pcf8574_write(uint8_t data)
{
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    // Dùng timeout 150ms trực tiếp
    return i2c_master_transmit(s_dev, &data, 1, 150);
}

esp_err_t pcf8574_read(uint8_t *data)
{
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    // Dùng timeout 150ms trực tiếp
    return i2c_master_receive(s_dev, data, 1, 150);
}

bool pcf8574_read_pin(uint8_t pin)
{
    uint8_t data = 0xFF;
    if (pcf8574_read(&data) != ESP_OK) return false;
    return !(data & (1 << pin)); // active LOW
}

esp_err_t pcf8574_reinit(uint8_t new_addr)
{
    if (!s_bus_handle) return ESP_ERR_INVALID_STATE;

    if (s_dev) {
        // Gỡ bỏ thiết bị cũ ra khỏi Bus I2C để tránh rò rỉ bộ nhớ hoặc xung đột handle
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }

    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = new_addr,
        .scl_speed_hz    = 100000,
    };

    esp_err_t ret = i2c_master_bus_add_device(s_bus_handle, &cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to re-register PCF8574 at address 0x%02X: %s", new_addr, esp_err_to_name(ret));
        s_dev = NULL;
        return ret;
    }

    ESP_LOGI(TAG, ">>> PCF8574 successfully re-registered at address: 0x%02X", new_addr);
    return pcf8574_write(0xFF);
}

esp_err_t pcf8574_probe_and_reinit(void)
{
    if (!s_bus_handle) return ESP_ERR_INVALID_STATE;

    const uint8_t possible_addrs[] = {
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, // PCF8574
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F  // PCF8574A
    };

    ESP_LOGI(TAG, "Dynamic Scan: Checking all potential PCF8574/PCF8574A addresses...");
    for (size_t i = 0; i < sizeof(possible_addrs) / sizeof(possible_addrs[0]); i++) {
        uint8_t a = possible_addrs[i];
        
        // Thử xem thiết bị có phản hồi ACK không (dùng timeout 150ms trực tiếp)
        if (i2c_master_probe(s_bus_handle, a, 150) == ESP_OK) {
            ESP_LOGI(TAG, ">>> Dynamic Scan SUCCESS! Active PCF8574 chip found at address: 0x%02X", a);
            return pcf8574_reinit(a);
        }
    }

    ESP_LOGW(TAG, "Dynamic Scan: No PCF8574/PCF8574A devices detected on bus ch0.");
    return ESP_ERR_NOT_FOUND;
}