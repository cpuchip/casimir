# casim and the core's number engine (casim needs only abn), linked with the decimal library
# cross-built for the S3 inside the pinned core export (build/core).
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CORE "${REPO}/build/core")
set(BID_LIB "${CORE}/build/intel-esp32s3/libbid.a")
set(BID_INC "${CORE}/third_party/intel-dfp/esp32s3/IntelRDFPMathLib20U5/LIBRARY/src")
set(CASES_C "${CMAKE_CURRENT_LIST_DIR}/generated/cases.c")

idf_component_register(
    SRCS bench_main.c "${CASES_C}" "${REPO}/src/casim.c" "${CORE}/core/abn_intel.c"
    INCLUDE_DIRS . "${REPO}/src" "${CORE}/core")

if(NOT EXISTS "${CASES_C}")
    message(FATAL_ERROR "no ${CASES_C}: run scripts/device.sh")
endif()
target_include_directories(${COMPONENT_LIB} SYSTEM PRIVATE "${BID_INC}")
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    DECIMAL_CALL_BY_REFERENCE=0 DECIMAL_GLOBAL_ROUNDING=0 DECIMAL_GLOBAL_EXCEPTION_FLAGS=0)
target_link_libraries(${COMPONENT_LIB} PRIVATE "${BID_LIB}")
