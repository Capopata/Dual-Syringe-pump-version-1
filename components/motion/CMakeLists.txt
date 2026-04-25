idf_component_register(
    SRCS
        "PID/pid.c"
        "trapezoidal_profile/trapezoidal_profile.c"
    INCLUDE_DIRS
        "."
        "PID"
        "trapezoidal_profile"
    REQUIRES driver freertos esp_driver_gpio 
)