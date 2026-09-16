# motherBoard — proyecto de ESP-IDF.
#
# Igual que en el HMI, src/ e include/ NO se mueven: los recoge
# main/CMakeLists.txt donde estan, para no romper el historial de git.
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
  # incunest_platform es solo el AGREGADOR (sin fuentes): requiere los cuatro
  # de abajo para que las 157 inclusiones de platform/*.h sigan resolviendo
  # sin tocar el firmware. En codigo nuevo, pedir el componente concreto.
  "${CMAKE_CURRENT_LIST_DIR}/../components/incunest_platform"
  "${CMAKE_CURRENT_LIST_DIR}/../components/incunest_plat_core"
  "${CMAKE_CURRENT_LIST_DIR}/../components/incunest_plat_hw"
  "${CMAKE_CURRENT_LIST_DIR}/../components/incunest_plat_storage"
  "${CMAKE_CURRENT_LIST_DIR}/../components/incunest_plat_net"
  "${CMAKE_CURRENT_LIST_DIR}/../shared"
  "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard"
  "${CMAKE_CURRENT_LIST_DIR}/../components/arduino_pid"
  "${CMAKE_CURRENT_LIST_DIR}/../components/incunest_afe4490"
  "${CMAKE_CURRENT_LIST_DIR}/../components/incunest_sensors"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(motherboard)
