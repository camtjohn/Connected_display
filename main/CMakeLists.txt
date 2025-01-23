idf_component_register(SRCS "ui.c" "tasks.c" "mqtt.c" "http_rest.c" "wifi.c" "view.c" "main.c" "led_driver.c" "sprite.c" "spi.c" "ui.c"
                    INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
