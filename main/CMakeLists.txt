idf_component_register(SRCS "led_driver.c" "animation.c" "http_rest.c" "local_time.c" "main.c" "mqtt.c" "spi.c" "sprite.c" "tasks.c" "ui.c" "view.c" "wifi.c"
                    INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
