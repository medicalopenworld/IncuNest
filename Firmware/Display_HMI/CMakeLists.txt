# Display HMI — proyecto de ESP-IDF.
#
# El arbol de fuentes NO se ha movido: src/ e include/ siguen donde estaban
# para que el historial de git de 36k lineas se mantenga legible (git log
# --follow, git blame). Es main/CMakeLists.txt quien los recoge.
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
  "${CMAKE_CURRENT_LIST_DIR}/../components/incunest_platform"
  "${CMAKE_CURRENT_LIST_DIR}/../shared"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(display_hmi)
