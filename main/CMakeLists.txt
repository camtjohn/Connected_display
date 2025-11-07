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
	"Views/view.c"
	"Views/menu.c"
	"Views/conway.c"
	"Views/weather.c"
	"Views/etchsketch.c"
	EMBED_TXTFILES "device002.crt" "device002.key" "ca.crt"
                    
	INCLUDE_DIRS
	"Include"
	"Views/Include"
	"TLS_Keys"
	)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
