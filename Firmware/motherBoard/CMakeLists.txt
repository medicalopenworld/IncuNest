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
  "${CMAKE_CURRENT_LIST_DIR}/../components/incunest_sensors"
)

# incunest_afe4490 es un SUBMODULO de git, no codigo de este repo. En un clon
# sin --recursive el directorio existe pero esta vacio, e IDF falla mucho mas
# tarde con un "component not found" que no dice como arreglarlo.
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/../components/incunest_afe4490/CMakeLists.txt")
  message(FATAL_ERROR
    "El submodulo incunest_afe4490 esta vacio.\n"
    "Desde la raiz del repo:\n"
    "    git submodule update --init --recursive")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Instrumentacion de tiempos de la libreria del AFE4490 (INCUNEST_TIMING_STATS),
# apagada por defecto. Se enciende asi:
#
#     set INCUNEST_PPG_TIMING=1 && idf.py build flash
#
# Emite una trama "$TIMING,..." cada 5 s (2500 muestras a 500 Hz) con media y
# maximo de hr1/hr2/hr3/spo2, del CICLO y de los computos asincronos, mas la
# pila libre. Desde v0.92 lleva ademas spi_mean y spi_max: esa es la medida que
# plat_spi.h deja pendiente de banco sobre el coste por byte de
# spi_device_polling_transmit().
#
# ESTO VIVIA EN EL CMakeLists DEL COMPONENTE, que ahora es un submodulo y no se
# puede editar. Y NO ES UN DEFINE CUALQUIERA: el #if INCUNEST_TIMING_STATS ANADE
# MIEMBROS a la clase INCUNEST_AFE4490, o sea cambia su sizeof. La cabecera la
# incluyen SPO2.h, DriveUpload.h y PpgSnapshot.h; si el define llegase a unas
# unidades de traduccion y a otras no, cada lado veria una clase de distinto
# tamano y el resultado seria corrupcion de memoria silenciosa, no un error de
# enlazado. idf_build_set_property alcanza a TODO el build, que es justo la
# garantia que hace falta.
#
# VA AQUI Y NO MAS ARRIBA porque idf_build_set_property solo existe despues de
# incluir project.cmake, y tiene que ejecutarse antes de project(), que es
# cuando se procesan los componentes. Moverlo antes del include() de arriba
# rompe la configuracion con "Unknown CMake command idf_build_set_property".
#
# NO DEJARLO PUESTO EN PRODUCCION: cuesta dos lecturas de esp_timer por muestra
# a 500 Hz y mete una trama de log cada 5 s.
if(DEFINED ENV{INCUNEST_PPG_TIMING})
  message(WARNING "INCUNEST_PPG_TIMING: instrumentacion de tiempos del AFE4490 "
                  "ACTIVADA. Binario de diagnostico, no de produccion.")
  idf_build_set_property(COMPILE_DEFINITIONS "INCUNEST_TIMING_STATS=1" APPEND)
endif()

project(motherboard)
