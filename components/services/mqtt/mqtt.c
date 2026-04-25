#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "mqtt.h"

static const char *TAG = "NETWORK_MODULE";

// Quản lý trạng thái kết nối
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static esp_mqtt_client_handle_t mqtt_client;

// --- Task Gửi Dữ Liệu ---
void mqtt_publisher_task(void *pvParameters) {
    float pos = 0.0, vel = 1.2, acc = 0.5; // Các biến giả lập

    while (1) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "pos", pos);
        cJSON_AddNumberToObject(root, "vel", vel);
        cJSON_AddNumberToObject(root, "acc", acc);
        // Có thể thêm đến 10 biến tùy ý ở đây

        char *json_string = cJSON_PrintUnformatted(root);
        
        if (mqtt_client != NULL) {
            esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, json_string, 0, 1, 0);
            ESP_LOGI(TAG, "Sent: %s", json_string);
        }

        cJSON_Delete(root);
        free(json_string);

        // Giả lập cập nhật dữ liệu
        pos += 0.1;
        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
}

// --- Handler Sự Kiện WiFi & IP ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Trying to connect to WiFi...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "The IP address has been obtained!");
    }
}

// --- Handler Sự Kiện MQTT ---
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    //esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected!");
            xTaskCreate(mqtt_publisher_task, "mqtt_pub_task", 4096, NULL, 5, NULL);
            //ESP_LOGI(TAG, "setup() running on core %u", xPortGetCoreID());
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected!");
            break;
        default:
            break;
    }
}

// --- Hàm Khởi Tạo Toàn Bộ Hệ Thống Mạng ---
void network_init(void) {
    s_wifi_event_group = xEventGroupCreate();

    // Khởi tạo "Card mạng ảo" ở tầng trung gian (Network interface) giao tiếp giữa hardware wifi và TCP/IP
    esp_netif_create_default_wifi_sta();
    
    // 2. Cấu hình WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    // 3. Chờ có WiFi rồi mới Start MQTT
    ESP_LOGI(TAG, "Waitting WiFi...");
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    // 4. Khởi tạo MQTT
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}