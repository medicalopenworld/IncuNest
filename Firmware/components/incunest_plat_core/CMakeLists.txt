# NUCLEO de la capa de plataforma: lo que no toca hardware ni red.
#
# String, Print, millis()/delay(), helpers numericos y tipos. Es la base del
# arbol: todas las demas capas dependen de esta, y esta no depende de ninguna.
#
# Su lista de REQUIRES es deliberadamente CORTA. Ese es el sentido de haber
# partido el componente: antes todo esto vivia junto a WiFi, HTTP y mbedTLS en
# un unico `incunest_platform` con 21 REQUIRES, asi que un test del sistema de
# ficheros compilaba media pila de red.
idf_component_register(
  SRCS
    "src/plat_string.cpp"
    "src/plat_print.cpp"
    "src/plat_time.c"
    "src/plat_num.c"
    "src/plat_esp.cpp"
  INCLUDE_DIRS "include"
  # spi_flash: plat_esp.cpp usa esp_flash.h para el tamano de flash real.
  REQUIRES esp_timer esp_system spi_flash
)
