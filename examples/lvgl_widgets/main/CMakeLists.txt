hifw_component_register(
    SRCS
        src/main.c
        src/serial_encoder.c
    REQUIRES lvgl
    COMPILE_DEFINITIONS _POSIX_C_SOURCE=200809L
    COMPILE_OPTIONS -Wall -Wextra)
