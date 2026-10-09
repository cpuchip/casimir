# casim's on-chip bench, on Casimir's ESP-IDF component (the repository's root, device/CMakeLists.txt): the
# library and Intel's decimal library built as any other S3 project builds them (CAS 011, D5).
set(CASES_C "${CMAKE_CURRENT_LIST_DIR}/generated/cases.c")

idf_component_register(
    SRCS bench_main.c "${CASES_C}"
    INCLUDE_DIRS .
    REQUIRES casim esp_timer)

if(NOT EXISTS "${CASES_C}")
    message(FATAL_ERROR "no ${CASES_C}: run scripts/device.sh")
endif()
