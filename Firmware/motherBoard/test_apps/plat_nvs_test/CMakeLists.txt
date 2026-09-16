cmake_minimum_required(VERSION 3.16)

# Solo almacenamiento y su nucleo. Desde que la capa se partio (d56c987) un
# test de NVS no arrastra ni red ni perifericos.
set(EXTRA_COMPONENT_DIRS
  "../../../components/incunest_plat_storage"
  "../../../components/incunest_plat_core"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(plat_nvs_test)
