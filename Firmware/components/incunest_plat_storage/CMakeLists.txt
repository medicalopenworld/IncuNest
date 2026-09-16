# Almacenamiento: LittleFS con forma de la API FS de Arduino, y NVS con forma
# de Preferences.
#
# AQUI VIVEN LOS DATOS DE PACIENTE: los perfiles de bebe y el historico de
# pesos. Dos contratos que no se pueden romper sin dejar sin datos a una unidad
# ya desplegada:
#   - FsFile::name() devuelve el NOMBRE PELADO, no la ruta (d324f9e).
#   - putFloat/putDouble guardan BLOB, como hacia Arduino.
# Los fija motherBoard/test_apps/plat_fs_test, que corre en la placa.
#
# Su lista de REQUIRES no menciona red por ninguna parte, que era justo el
# problema de tenerlo todo en un solo componente.
idf_component_register(
  SRCS
    "src/plat_fs.cpp"
    "src/plat_nvs.cpp"
  INCLUDE_DIRS "include"
  REQUIRES incunest_plat_core nvs_flash joltwallet__littlefs vfs spi_flash
)
