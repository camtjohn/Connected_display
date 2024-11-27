idf_component_register(SRCS "tasks.c" "mqtt.c" "http_rest.c" "wifi.c" "view.c" "main.c" "led_display.c" "spi.c"
                    INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
