cmake_minimum_required(VERSION 3.16)

# Set IDF_COMPONENT_MANAGER to use idf_component.yml
set(IDF_COMPONENT_MANAGER 1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(incunest_hmi VERSION 0.1.0)
