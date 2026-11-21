idf_component_register(
    SRCS "main.c" "services.c" "ui.c" "art.c" "radio.c" "ota.c"
    INCLUDE_DIRS "."
    REQUIRES core waveshare__esp32_s3_touch_amoled_1_75c bt nvs_flash esp_driver_gpio esp_driver_i2c esp_driver_tsens esp_driver_usb_serial_jtag esp_timer spiffs esp_partition espcoredump esp_pm
             esp_wifi esp_netif esp_event esp_http_server esp_http_client esp_https_ota app_update mbedtls
)

# Hora de compilación como respaldo si el RTC todavía no tiene hora.
string(TIMESTAMP BUILD_TS "%s" UTC)
target_compile_definitions(${COMPONENT_LIB} PRIVATE BUILD_UNIX_TIME=${BUILD_TS}u)

# Los avisos del compilador no detienen la compilación (el proyecto se prueba aparte en el simulador).
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error -Wno-error=format-truncation -Wno-format-truncation)

# v89: no repintar cuando un estilo se pone con el mismo valor que ya tenía, ni repintar la pantalla entera al reordenar un objeto (ver los __wrap_ en ui.c)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=lv_obj_set_local_style_prop" "-Wl,--wrap=lv_obj_move_to_index")
