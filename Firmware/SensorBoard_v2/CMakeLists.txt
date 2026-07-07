cmake_minimum_required(VERSION 3.16)
# Build mínimo: solo main y sus dependencias transitivas (evita compilar
# componentes IDF no usados, p. ej. esp_lcd, que además dispara un ICE de GCC).
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(SensorBoard)
