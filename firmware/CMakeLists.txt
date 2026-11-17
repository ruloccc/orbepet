cmake_minimum_required(VERSION 3.16)

# El motor de la mascota vive en ../core y se comparte con el simulador de PC.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../core")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# (v89 subía LV_INV_BUF_SIZE a 96; v91 lo vuelve a 32: el adaptador de pantalla de Espressif arma en la PILA de su tarea
# (8 KB) una lista de LV_INV_BUF_SIZE*4+4 rectángulos de 16 bytes: con 96 eran 6,2 KB, se desbordaba la pila y la placa
# entraba en pánico al dibujar muchas cosas chicas a la vez, como las gotitas del chapoteo.)
project(mascota)
