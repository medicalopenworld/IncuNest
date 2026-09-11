# Recoge el arbol de src/ sin moverlo de sitio.
#
# Sin CONFIGURE_DEPENDS: ESP-IDF evalua este fichero tambien en modo script
# para extraer los REQUIRES, y ahi CMake rechaza esa opcion. Al ANADIR o BORRAR
# un fichero hay que correr `idf.py reconfigure`.
file(GLOB_RECURSE MB_SRCS
  "${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../src/*.c"
)

# hal_hw16.cpp estaba excluido del build en los dos entornos de platformio.ini
# (build_src_filter = +<*> -<hal/hal_hw16.cpp>). Se mantiene fuera.
list(FILTER MB_SRCS EXCLUDE REGEX "/hal/hal_hw16\\.cpp$")

idf_component_register(
  SRCS ${MB_SRCS} "app_main.cpp"
  INCLUDE_DIRS
    "."
    "${CMAKE_CURRENT_LIST_DIR}/../include"
    "${CMAKE_CURRENT_LIST_DIR}/../include/config"
    "${CMAKE_CURRENT_LIST_DIR}/../include/protocol"
    "${CMAKE_CURRENT_LIST_DIR}/../include/tasks"
    "${CMAKE_CURRENT_LIST_DIR}/../src"
    "${CMAKE_CURRENT_LIST_DIR}/../src/drivers"
  REQUIRES
    incunest_platform
    shared
    esp_driver_gpio
    esp_driver_uart
    esp_driver_i2c
    esp_driver_spi
    esp_driver_ledc
    esp_adc
    esp_wifi
    esp_netif
    esp_event
    esp_http_server
    esp_https_ota
    app_update
    esp_timer
    nvs_flash
    usb
    bt
    mqtt
    thingsboard
    arduino_pid
    incunest_afe4490
    incunest_sensors
    espressif__mdns
    espressif__esp_modem
    joltwallet__littlefs
)

# HW_NUM sale ahora de Kconfig (menuconfig) en vez de los dos entornos de
# platformio.ini, que solo se diferenciaban en este define. board.h aborta la
# compilacion si no esta puesto.
target_compile_definitions(${COMPONENT_LIB} PUBLIC HW_NUM=${CONFIG_INCUNEST_HW_NUM})

# Variante de taller (antes los entornos *_factory de platformio.ini). Ver la
# ayuda de INCUNEST_FTEST_SIM_ACT_ENABLED en Kconfig.projbuild: ese binario no
# se distribuye.
if(CONFIG_INCUNEST_FTEST_SIM_ACT_ENABLED)
  target_compile_definitions(${COMPONENT_LIB} PUBLIC FTEST_SIM_ACT_ENABLED=1)
endif()

# Reproduce el reparto de optimizacion que habia con PlatformIO: la IDF va a
# -Os (CONFIG_COMPILER_OPTIMIZATION_SIZE en sdkconfig.defaults, igual que el
# framework de Arduino precompilado) y SOLO las fuentes del proyecto van a
# -O0 -g3, que es lo que pedia build_flags. Sin esto, el porte cambiaria en
# silencio el nivel de optimizacion del codigo de control.
target_compile_options(${COMPONENT_LIB} PRIVATE -O0 -g3)

# Estandar de C++: gnu++17, el que usaba arduino-esp32 2.0.16 en esta placa.
# ESP-IDF 6 va por defecto a gnu++26, que cambia reglas del lenguaje (entre
# otras, convierte en error las operaciones de bits entre enums distintos).
# Modernizar el estandar es un trabajo aparte del de quitar Arduino.
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>)

# PlatformIO no compilaba con -Werror y ESP-IDF si. Se rebajan a aviso SOLO
# para las fuentes del proyecto, para que el porte no se mezcle con la
# limpieza de avisos preexistentes.
target_compile_options(${COMPONENT_LIB} PRIVATE
  -Wno-error=missing-field-initializers
  -Wno-error=deprecated-enum-enum-conversion
  -Wno-error=format-truncation
  -Wno-error=format=
  -Wno-error=attributes
)
