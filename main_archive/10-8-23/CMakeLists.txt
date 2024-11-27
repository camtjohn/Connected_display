idf_component_register(SRCS "main.c" "array_tools.c" "adc_tools.c" "mqtt.c" "wifi.c"
                    INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
