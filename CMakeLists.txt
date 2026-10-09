# Casimir with CMake: the library on any gcc or clang host, an ESP-IDF component, or WebAssembly through emscripten
# (README.md). Intel's decimal library is fetched by URL and SHA-256 at configure time (cmake/intel-dfp.cmake).
#
#   cmake -B build-cmake && cmake --build build-cmake && ctest --test-dir build-cmake
#
# As a subproject (add_subdirectory), link the target casim; its header is casim.h. In an ESP-IDF project, list this
# repository as a component (idf_component.yml) and REQUIRES casim.
cmake_minimum_required(VERSION 3.16)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/intel-dfp.cmake")

# CAS 004 rule 3: no frame in casim larger than 1 KB (gcc and clang spell it differently; called once the
# compiler is known)
macro(casim_frame_flags)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(casim_frame -Wframe-larger-than=1024 -Werror=frame-larger-than)
    else()
        set(casim_frame -Werror=frame-larger-than=1024)
    endif()
endmacro()

# ---- ESP-IDF: a component ----
if(ESP_PLATFORM)
    if(CMAKE_BUILD_EARLY_EXPANSION)
        idf_component_register(INCLUDE_DIRS src)
        return()
    endif()
    casim_intel_dfp(CASIM_INTEL_SOURCES CASIM_INTEL_INCLUDE ia32)
    idf_component_register(
        SRCS src/casim.c third_party/abn/abn_intel.c ${CASIM_INTEL_SOURCES}
        INCLUDE_DIRS src
        PRIV_INCLUDE_DIRS third_party/abn)
    target_include_directories(${COMPONENT_LIB} SYSTEM PRIVATE "${CASIM_INTEL_INCLUDE}")
    set_source_files_properties(third_party/abn/abn_intel.c PROPERTIES COMPILE_DEFINITIONS
        "DECIMAL_CALL_BY_REFERENCE=0;DECIMAL_GLOBAL_ROUNDING=0;DECIMAL_GLOBAL_EXCEPTION_FLAGS=0")
    # Intel's sources as the firmware builds them for the S3: its warnings are not ours, and the rounding-mode calls
    # are compiled out (cmake/fenv-nearest.h says why)
    set_property(SOURCE ${CASIM_INTEL_SOURCES} APPEND PROPERTY COMPILE_OPTIONS
        -w -include "${CMAKE_CURRENT_LIST_DIR}/cmake/fenv-nearest.h")
    casim_frame_flags()
    set_property(SOURCE src/casim.c APPEND PROPERTY COMPILE_OPTIONS ${casim_frame})
    return()
endif()

# ---- everywhere else ----
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/src/casim.h" casim_version_line REGEX "^#define CASIM_VERSION \"")
string(REGEX REPLACE "^#define CASIM_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" casim_version "${casim_version_line}")
project(casimir VERSION ${casim_version} LANGUAGES C)
casim_frame_flags()

set(casim_top OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(casim_top ON)
endif()
option(CASIM_BUILD_TOOLS "the command line, the C example and the golden test" ${casim_top})
option(CASIM_WERROR "casim's own sources at -Werror" ${casim_top})

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(casim_arch efi2)        # Intel's 64-bit configuration
else()
    set(casim_arch ia32)        # 32-bit: the ESP32's and wasm32's
endif()
casim_intel_dfp(CASIM_INTEL_SOURCES CASIM_INTEL_INCLUDE ${casim_arch})

add_library(casim STATIC src/casim.c third_party/abn/abn_intel.c ${CASIM_INTEL_SOURCES})
target_include_directories(casim PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>" "$<INSTALL_INTERFACE:include>"
                           PRIVATE third_party/abn)
target_include_directories(casim SYSTEM PRIVATE "${CASIM_INTEL_INCLUDE}")
set_target_properties(casim PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
set_source_files_properties(third_party/abn/abn_intel.c PROPERTIES COMPILE_DEFINITIONS
    "DECIMAL_CALL_BY_REFERENCE=0;DECIMAL_GLOBAL_ROUNDING=0;DECIMAL_GLOBAL_EXCEPTION_FLAGS=0")
set_property(SOURCE ${CASIM_INTEL_SOURCES} APPEND PROPERTY COMPILE_OPTIONS -w)
set(casim_warn -Wall -Wextra -Wpedantic)
if(CASIM_WERROR)
    list(APPEND casim_warn -Werror)
endif()
set_property(SOURCE src/casim.c third_party/abn/abn_intel.c APPEND PROPERTY COMPILE_OPTIONS ${casim_warn})
set_property(SOURCE src/casim.c APPEND PROPERTY COMPILE_OPTIONS ${casim_frame})
if(NOT EMSCRIPTEN AND NOT WIN32)
    target_link_libraries(casim PUBLIC m)       # Intel's library calls sqrt and the rounding-mode functions
endif()

include(GNUInstallDirs)
install(TARGETS casim EXPORT casim ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES src/casim.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(CASIM_BUILD_TOOLS AND NOT EMSCRIPTEN)
    add_executable(casim_cli cli/casim_cli.c)
    set_target_properties(casim_cli PROPERTIES OUTPUT_NAME casim)
    add_executable(casim_hello examples/hello.c)
    add_executable(golden_check tests/golden_check.c)
    foreach(t casim_cli casim_hello golden_check)
        target_link_libraries(${t} PRIVATE casim)
        set_target_properties(${t} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
        target_compile_options(${t} PRIVATE ${casim_warn})
    endforeach()
    enable_testing()
    add_test(NAME golden COMMAND golden_check "${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/corpus.txt")
    add_test(NAME hello COMMAND casim_hello)
    set_tests_properties(hello PROPERTIES PASS_REGULAR_EXPRESSION "Casimir ${casim_version}")
endif()

if(EMSCRIPTEN)
    include("${CMAKE_CURRENT_LIST_DIR}/wasm/wasm.cmake")
endif()
