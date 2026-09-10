# Proyecto minimo cuyo unico trabajo es COMPILAR components/platform y shared/
# contra la ESP-IDF instalada. No se flashea: es el gate barato que detecta que
# una API de IDF ha cambiado bajo nuestros pies, sin tener que construir el
# firmware entero de una placa.
#
#   idf.py -C Firmware/tools/platform_smoke build
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS
  "${CMAKE_CURRENT_LIST_DIR}/../../components/incunest_platform"
  "${CMAKE_CURRENT_LIST_DIR}/../../shared"
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(platform_smoke)
