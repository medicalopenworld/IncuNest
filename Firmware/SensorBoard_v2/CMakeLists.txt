cmake_minimum_required(VERSION 3.16)
# Build mínimo: solo main y sus dependencias transitivas (evita compilar
# componentes IDF no usados, p. ej. esp_lcd, que además dispara un ICE de GCC).
#
# espcoredump va explícito porque nadie lo requiere: no es dependencia de main
# ni transitiva de esp_system, así que con `set(COMPONENTS main)` a secas queda
# fuera del build. Y quedarse fuera no da ningún error — sus símbolos Kconfig
# dejan de existir, y entonces CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH en
# sdkconfig.defaults se descarta en silencio (confgen ignora símbolos
# desconocidos): compila, arranca, y no hay volcado en el panic. Si algún día
# se quita de esta lista, comprobar que `grep COREDUMP sdkconfig` sigue
# devolviendo algo.
set(COMPONENTS main espcoredump)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(SensorBoard)
