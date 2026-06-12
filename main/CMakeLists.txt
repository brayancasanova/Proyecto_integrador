set(srcs "main.c"
	     "connect_wifi.c")
idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "."
                    REQUIRES esp32-camera nvs_flash esp_http_server esp_wifi esp_timer)