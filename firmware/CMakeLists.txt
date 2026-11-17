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
