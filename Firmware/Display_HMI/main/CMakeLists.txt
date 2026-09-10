# Recoge el arbol de src/ sin moverlo de sitio.
#
# Se usa GLOB_RECURSE aun sabiendo que ESP-IDF prefiere listas explicitas: son
# ~100 ficheros que se anaden y quitan a menudo, y una lista a mano se
# desincroniza en silencio (un fichero nuevo que no entra al build no da error,
# simplemente no existe). Sin CONFIGURE_DEPENDS a proposito: ESP-IDF evalua
# este fichero tambien en modo script para extraer los REQUIRES, y ahi CMake
# rechaza esa opcion. El precio es que al ANADIR o BORRAR un fichero hay que
# correr `idf.py reconfigure` (editarlo no, eso ninja ya lo ve).
file(GLOB_RECURSE HMI_SRCS
  "${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../src/*.c"
)

# AudioManager quedo fuera del build en la migracion a Arduino 3.x
# (platformio.ini: build_src_filter = +<*> -<tasks/AudioManager.cpp>). El porte
# NO lo reactiva: seria meter una funcionalidad nueva dentro de un cambio de
# toolchain. Queda pendiente y anotado.
list(FILTER HMI_SRCS EXCLUDE REGEX "/tasks/AudioManager\.cpp$")

idf_component_register(
  SRCS ${HMI_SRCS} "app_main.cpp"
  INCLUDE_DIRS
    "."
    "${CMAKE_CURRENT_LIST_DIR}/../include"
    "${CMAKE_CURRENT_LIST_DIR}/../include/config"
    "${CMAKE_CURRENT_LIST_DIR}/../include/protocol"
    "${CMAKE_CURRENT_LIST_DIR}/../include/tasks"
    "${CMAKE_CURRENT_LIST_DIR}/../include/ui"
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
    esp_lcd
    esp_wifi
    esp_netif
    esp_event
    esp_http_server
    esp_https_ota
    app_update
    esp_timer
    nvs_flash
    spiffs
    mqtt
    thingsboard
    espressif__mdns
)

# Estandar de C++: gnu++2b, el mismo con el que compilaba pioarduino.
#
# ESP-IDF 6 compila por defecto en gnu++26, y eso NO es neutral para este
# codigo: C++26 convierte en ERROR las operaciones de bits entre enums de
# distinto tipo, que es un patron que LVGL usa por todas partes (22 errores
# solo en src/ui). Fijar el estandar que ya se usaba mantiene el porte
# centrado en quitar Arduino, en vez de arrastrar de paso una migracion de
# lenguaje sobre 36k lineas. Subir a C++26 es un trabajo aparte y deliberado.
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++2b>)

# PlatformIO no compilaba con -Werror; ESP-IDF si. Mantener -Werror aqui
# convertiria en errores avisos preexistentes (inicializadores de struct
# incompletos, etc.) que no tienen nada que ver con el porte. Se rebajan a
# aviso SOLO para las fuentes del proyecto: los componentes siguen con la
# politica estricta de IDF. Limpiarlos es una tarea propia, no de este cambio.
target_compile_options(${COMPONENT_LIB} PRIVATE
  -Wno-error=missing-field-initializers
  -Wno-error=deprecated-enum-enum-conversion
)
