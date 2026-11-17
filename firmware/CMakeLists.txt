cmake_minimum_required(VERSION 3.16)

# El motor de la mascota vive en ../core y se comparte con el simulador de PC.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../core")

# (1.0.09) la versión sale de main/version.h (el único lugar donde se cambia): queda en la descripción del programa
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/main/version.h" _fw_ver REGEX "^#define FW_VERSION ")
string(REGEX REPLACE "^#define FW_VERSION \"([^\"]+)\".*" "\\1" PROJECT_VER "${_fw_ver}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# (v89 subía LV_INV_BUF_SIZE a 96; v91 lo vuelve a 32: el adaptador de pantalla de Espressif arma en la PILA de su tarea
# (8 KB) una lista de LV_INV_BUF_SIZE*4+4 rectángulos de 16 bytes: con 96 eran 6,2 KB, se desbordaba la pila y la placa
# entraba en pánico al dibujar muchas cosas chicas a la vez, como las gotitas del chapoteo.)
project(mascota)

# (1.0.09) la red, la conexión segura y el Bluetooth se compilan para ocupar poco (-Os): no necesitan velocidad y así el
# programa cabe en ota_1. El juego, LVGL y el dibujo siguen con -O2.
foreach(_c mbedtls lwip wpa_supplicant bt esp_netif esp_http_server esp_http_client esp_https_ota tcp_transport esp-tls)
    idf_component_get_property(_lib ${_c} COMPONENT_LIB)
    if(_lib)
        target_compile_options(${_lib} PRIVATE -Os)
    endif()
endforeach()
# El motor del juego (core) y la lógica de la interfaz (ui.c) también: con el Wi-Fi el programa se pasaba de ota_1. Lo que
# dibuja (LVGL, art.c, la pantalla) sigue rápido. Si en la placa la interfaz se notara más lenta, quitar la línea de ui.c.
idf_component_get_property(_core core COMPONENT_LIB)
target_compile_options(${_core} PRIVATE -Os)
idf_component_get_property(_main main COMPONENT_LIB)
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/main/ui.c" TARGET_DIRECTORY ${_main} PROPERTIES COMPILE_OPTIONS "-Os")
foreach(_t mbedtls mbedcrypto mbedx509) # (las librerías de verdad de mbedTLS)
    if(TARGET ${_t})
        target_compile_options(${_t} PRIVATE -Os)
    endif()
endforeach()
