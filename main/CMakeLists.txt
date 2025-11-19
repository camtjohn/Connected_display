idf_component_register(
	SRCS 
	"led_driver.c"
	"animation.c"
	"event_system.c"
	"http_rest.c"
	"local_time.c"
	"main.c"
	"mqtt.c"
	"spi.c"
	"sprite.c"
	"tasks.c"
	"ui.c"
	"wifi.c"
	"ota.c"
	"Views/view.c"
	"Views/menu.c"
	"Views/conway.c"
	"Views/weather.c"
	"Views/etchsketch.c"
	EMBED_TXTFILES "TLS_Keys/device002.crt" "TLS_Keys/device002.key" "TLS_Keys/ca.crt"
                    
	INCLUDE_DIRS
	"Include"
	"Views/Include"
	)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
