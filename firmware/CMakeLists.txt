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
        # (1.0.55) un componente sin código (el Bluetooth apagado) es una biblioteca INTERFACE: no se le pueden poner opciones
        get_target_property(_tipo ${_lib} TYPE)
        if(NOT _tipo STREQUAL "INTERFACE_LIBRARY")
            target_compile_options(${_lib} PRIVATE -Os)
        endif()
    endif()
endforeach()
# (1.0.10) el juego (core, ui.c, art.c), LVGL y la pantalla siguen con -O2, igual de rápidos que antes: el espacio para el
# Wi-Fi salió de igualar las dos ranuras (partitions.csv), no de achicar el juego.
foreach(_t mbedtls mbedcrypto mbedx509) # (las librerías de verdad de mbedTLS)
    if(TARGET ${_t})
        target_compile_options(${_t} PRIVATE -Os)
    endif()
endforeach()
