# Tests Unity de host de la motherBoard y de shared/, sin PlatformIO ni ESP-IDF.
#
# Sustituye a `pio test -e native` (decision del 2026-09-11: CMake + Unity en
# host). Compila los MISMOS ficheros que listaba el build_src_filter del
# [env:native] de platformio.ini, con los mismos flags (-std=c++17,
# -DNATIVE_TEST) y las mismas cabeceras, y un ejecutable por suite: cada
# test_*.cpp trae su propio main() con UNITY_BEGIN/RUN_TEST, como antes.
#
#   cmake -S Firmware/tools/host_tests -B Firmware/tools/host_tests/build
#   cmake --build Firmware/tools/host_tests/build
#   ctest --test-dir Firmware/tools/host_tests/build --output-on-failure
#
# O el atajo: Firmware/tools/host_tests/run_host_tests.ps1
#
# Lo que NO entra, igual que antes: sensor_source.cpp, sensorboard_comm.cpp y
# todo lo que toca hardware. La logica pura vive en los sb_*.cpp y en
# modules/control, que es lo que se prueba.
cmake_minimum_required(VERSION 3.16)
project(incunest_host_tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(FW_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(MB_ROOT "${FW_ROOT}/motherBoard")
set(SHARED_ROOT "${FW_ROOT}/shared")

# ---- ArduinoJson (cabeceras) ----------------------------------------------
# La usan wifi_dwell.cpp, sb_json_codec.cpp y sb_telemetry.cpp. Se toma la
# copia que el gestor de componentes de IDF deja en managed_components/ si el
# proyecto ya se ha configurado alguna vez (misma version pineada, 6.21.5); si
# no, se descarga esa version exacta.
set(ARDUINOJSON_LOCAL "${MB_ROOT}/managed_components/bblanchon__arduinojson/src")
if(EXISTS "${ARDUINOJSON_LOCAL}/ArduinoJson.h")
  set(ARDUINOJSON_INCLUDE "${ARDUINOJSON_LOCAL}")
  message(STATUS "ArduinoJson: ${ARDUINOJSON_LOCAL}")
else()
  include(FetchContent)
  FetchContent_Declare(arduinojson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG v6.21.5
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(arduinojson)
  set(ARDUINOJSON_INCLUDE "${arduinojson_SOURCE_DIR}/src")
  message(STATUS "ArduinoJson: descargada v6.21.5")
endif()

# ---- Unity (vendorizada, v2.6.1, la misma que traia PlatformIO) -----------
add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC unity)
# Los tests comparan floats y algun double; PlatformIO compilaba Unity con
# soporte de double.
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE UNITY_SUPPORT_64)

# ---- Codigo bajo prueba: exactamente el build_src_filter de [env:native] --
set(MB_SOURCES
  modules/control/alarm_machine.cpp
  modules/control/alarm_window.cpp
  modules/control/alarm_history.cpp
  modules/control/alarm_test.cpp
  modules/control/pid_wrapper.cpp
  modules/control/fan_guard.cpp
  modules/factory_test/ftest_summary.cpp
  modules/baby_profile/baby_profile_core.cpp
  modules/baby_profile/baby_profile_protocol.cpp
  modules/util/civil_time.cpp
  modules/util/tz_source.cpp
  modules/util/wifi_dwell.cpp
  modules/util/ip_geoloc.cpp
  modules/baby_profile/baby_cloud.cpp
  modules/sensorboard_comm/sb_crc16.cpp
  modules/sensorboard_comm/sb_frame_parser.cpp
  modules/sensorboard_comm/sb_json_codec.cpp
  modules/sensorboard_comm/sb_link_state.cpp
  modules/sensorboard_comm/sb_door_state.cpp
  modules/sensorboard_comm/sb_env_fusion.cpp
  modules/sensorboard_comm/sb_telemetry.cpp
)
list(TRANSFORM MB_SOURCES PREPEND "${MB_ROOT}/src/")
file(GLOB SHARED_SOURCES "${SHARED_ROOT}/src/*.cpp")

add_library(under_test STATIC ${MB_SOURCES} ${SHARED_SOURCES})
target_include_directories(under_test PUBLIC
  "${MB_ROOT}/src"
  "${MB_ROOT}/include"
  "${MB_ROOT}/include/config"
  "${SHARED_ROOT}/include"
  "${ARDUINOJSON_INCLUDE}")
target_compile_definitions(under_test PUBLIC NATIVE_TEST)
target_link_libraries(under_test PUBLIC unity)

# ---- Una suite por directorio test_*/ -------------------------------------
enable_testing()
file(GLOB SUITE_DIRS LIST_DIRECTORIES true "${MB_ROOT}/test/test_*")
foreach(dir ${SUITE_DIRS})
  if(NOT IS_DIRECTORY "${dir}")
    continue()
  endif()
  get_filename_component(suite "${dir}" NAME)
  file(GLOB suite_sources "${dir}/*.cpp" "${dir}/*.c")
  if(NOT suite_sources)
    continue()
  endif()
  add_executable(${suite} ${suite_sources})
  target_link_libraries(${suite} PRIVATE under_test unity)
  target_include_directories(${suite} PRIVATE "${dir}")
  add_test(NAME ${suite} COMMAND ${suite})
endforeach()
