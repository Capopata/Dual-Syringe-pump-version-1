idf_component_register(
    SRCS
        "domain.c"
    INCLUDE_DIRS
        "."
    REQUIRES driver freertos esp_driver_gpio
)