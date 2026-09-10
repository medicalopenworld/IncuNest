# Display HMI — proyecto de ESP-IDF.
#
# El arbol de fuentes NO se ha movido: src/ e include/ siguen donde estaban
# para que el historial de git de 36k lineas se mantenga legible (git log
# --follow, git blame). Es main/CMakeLists.txt quien los recoge.
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
  "${CMAKE_CURRENT_LIST_DIR}/../components/incunest_platform"
  "${CMAKE_CURRENT_LIST_DIR}/../shared"
  "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard"
)

# LVGL tiene que usar EL lv_conf.h DEL PROYECTO, no su Kconfig.
#
# El componente de LVGL para ESP-IDF, si le dejas, se configura por menuconfig
# (CONFIG_LV_*) y se salta include/config/lv_conf.h. Eso aqui seria un cambio
# de comportamiento encubierto: ese fichero lleva la profundidad de color, el
# tamano del pool de LVGL y el juego de fuentes con el que esta cuadrada toda
# la UI. LV_KCONFIG_IGNORE apaga el camino de Kconfig y LV_CONF_PATH apunta al
# fichero de siempre, que asi sigue siendo la unica fuente de verdad.
# (idf_build_set_property solo existe DESPUES de incluir project.cmake, y
# tiene que ir ANTES de project(), que es cuando se procesan los componentes.)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# lv_conf.h saca el tick de LVGL de platform/plat_time.h, asi que el propio
# componente de LVGL tiene que poder encontrar esa cabecera. Va como include
# global porque LVGL no declara (ni debe declarar) una dependencia de un
# componente nuestro.
idf_build_set_property(INCLUDE_DIRECTORIES
  "${CMAKE_CURRENT_LIST_DIR}/../components/incunest_platform/include" APPEND)

idf_build_set_property(COMPILE_DEFINITIONS "LV_KCONFIG_IGNORE" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS
  "LV_CONF_PATH=${CMAKE_CURRENT_LIST_DIR}/include/config/lv_conf.h" APPEND)

project(display_hmi)
