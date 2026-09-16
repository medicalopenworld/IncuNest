cmake_minimum_required(VERSION 3.16)

# Se apunta al COMPONENTE CONCRETO, no a components/ entero.
#
# EXTRA_COMPONENT_DIRS con el directorio completo mete en el build todos los
# componentes que haya dentro — incluido `thingsboard`, que arrastra `mqtt`, y
# con el media pila de red. Un test del sistema de ficheros no tiene por que
# compilar el cliente de ThingsBoard.
# Desde que la plataforma se partio en cuatro (2026-09-16), un test del sistema
# de ficheros solo necesita DOS componentes: el de almacenamiento y el nucleo
# del que depende. Antes tenia que pedir `incunest_platform` entero y con el se
# compilaban mDNS, WiFi, HTTP y mbedTLS para probar un LittleFS.
set(EXTRA_COMPONENT_DIRS
  "../../../components/incunest_plat_storage"
  "../../../components/incunest_plat_core"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(plat_fs_test)
