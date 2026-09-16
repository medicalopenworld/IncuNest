cmake_minimum_required(VERSION 3.16)

# Se apunta al COMPONENTE CONCRETO, no a components/ entero.
#
# EXTRA_COMPONENT_DIRS con el directorio completo mete en el build todos los
# componentes que haya dentro — incluido `thingsboard`, que arrastra `mqtt`, y
# con el media pila de red. Un test del sistema de ficheros no tiene por que
# compilar el cliente de ThingsBoard.
set(EXTRA_COMPONENT_DIRS "../../../components/incunest_platform")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(plat_fs_test)
