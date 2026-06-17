idf_component_register(
    SRCS 
        "mqtt/mqtt.c"
        "storage/storage.c"
    INCLUDE_DIRS 
        "." 
        "mqtt"
    REQUIRES 
        nvs_flash 
        esp_wifi 
        esp_event 
        log           # Thay cho esp_log
        mqtt          # Thay cho mqtt_client
        json          # Thay cho cJSON
        driver        # Bao gồm cả GPIO, UART...
)