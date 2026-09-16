# shared/ como componente de ESP-IDF.
#
# Este directorio ya era portable: sus .cpp solo usan la libreria estandar
# (verificado — ni Arduino.h ni String). Lo unico atado al framework era
# include/fw_guarded_updater.h, que hereda de IUpdater del SDK de ThingsBoard;
# esa cabecera no la compila nadie desde aqui, la incluye quien la usa, asi que
# no entra en SRCS y el componente no arrastra la dependencia.
#
# El mismo directorio lo siguen consumiendo los tests de host, que no pasan por
# CMake de IDF.
idf_component_register(
  SRCS
    "src/alarm_audio_pattern.cpp"
    "src/alarm_policy.cpp"
    "src/alarm_text.cpp"
    "src/civil_time.cpp"
    "src/factory_test.cpp"
    "src/fw_image_tag.cpp"
    "src/nte_table.cpp"
  INCLUDE_DIRS "include"
)
