# casim's on-chip bench (ESP-IDF v5.4, ESP32-S3). Build through scripts/device.sh, which writes the
# cases from the host CLI, cross-builds the decimal library in the pinned core export and runs
# idf.py in Espressif's pinned image.
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(casim_bench)
