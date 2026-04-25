#pragma once

#include "esp_err.h"

// --- Cấu hình WiFi ---
#define WIFI_SSID      "UET-Wifi-Office-Free 2.4Ghz"
#define WIFI_PASS      ""

// --- Cấu hình MQTT ---
#define MQTT_BROKER    "mqtt://broker.hivemq.com:1883"
#define MQTT_TOPIC     "uet/mechanics/data"

// Prototype hàm khởi tạo duy nhất
void network_init(void);