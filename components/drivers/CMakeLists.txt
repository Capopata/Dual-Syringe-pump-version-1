idf_component_register(
    SRCS
        "as5600/as5600.c"
        "tft/tft.c"
        "tmc2209/tmc2209.c"
        "button/button.c"
    INCLUDE_DIRS
        "."
        "as5600"
        "tft"
        "tmc2209"
        "button"
    REQUIRES driver freertos esp_driver_gpio esp_driver_rmt esp_timer domain
)