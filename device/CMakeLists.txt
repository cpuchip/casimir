# casim's on-chip bench (ESP-IDF v5.4, ESP32-S3). Build through scripts/device.sh, which writes the cases from the
# host CLI and runs idf.py in Espressif's pinned image. Casimir comes in as the ESP-IDF component at the repository's
# root (CAS 011, D5); ESP-IDF names a component after its directory, so the root is linked in as build/components/casim.
cmake_minimum_required(VERSION 3.16)
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
file(MAKE_DIRECTORY "${REPO}/build/components")
if(NOT EXISTS "${REPO}/build/components/casim")
    file(CREATE_LINK "${REPO}" "${REPO}/build/components/casim" SYMBOLIC)
endif()
set(EXTRA_COMPONENT_DIRS "${REPO}/build/components/casim")
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(casim_bench)
