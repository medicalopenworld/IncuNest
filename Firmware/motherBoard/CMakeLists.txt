# motherBoard — proyecto de ESP-IDF.
#
# Igual que en el HMI, src/ e include/ NO se mueven: los recoge
# main/CMakeLists.txt donde estan, para no romper el historial de git.
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
  "${CMAKE_CURRENT_LIST_DIR}/../components/incunest_platform"
  "${CMAKE_CURRENT_LIST_DIR}/../shared"
  "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard"
  "${CMAKE_CURRENT_LIST_DIR}/../components/arduino_pid"
  "${CMAKE_CURRENT_LIST_DIR}/../components/incunest_afe4490"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(motherboard)
