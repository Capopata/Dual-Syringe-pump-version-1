#ifndef PCF8574_H
#define PCF8574_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t pcf8574_init(i2c_master_bus_handle_t bus_handle, uint8_t addr);
esp_err_t pcf8574_write(uint8_t data);
esp_err_t pcf8574_read(uint8_t *data);
bool      pcf8574_read_pin(uint8_t pin);
bool      pcf8574_is_present(void);

// Các hàm phục vụ quét địa chỉ động và tự sửa lỗi
esp_err_t pcf8574_reinit(uint8_t new_addr);
esp_err_t pcf8574_probe_and_reinit(void);

#ifdef __cplusplus
}
#endif

#endif