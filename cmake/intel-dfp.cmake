# Intel's Decimal Floating-Point Math Library for CMake builds (CAS 011, D3): the tarball fetched from Intel's
# distribution at netlib, checked against the same SHA-256 as scripts/fetch-intel-dfp.sh, unpacked into the build
# tree with the same const patch, and the sources Casimir links (third_party/intel-dfp/sources.txt) listed with the
# flags Intel's own makefile gives them. BSD-3-Clause; the notice is third_party/intel-dfp/LICENSE.
#
# casim_intel_dfp(<sources var> <include dir var> <arch define>)
#   CASIM_INTEL_TARBALL (cache): a local copy of the tarball to use instead of the download, checked the same way.

set(CASIM_INTEL_DFP_LIST_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(CASIM_INTEL_URL "https://netlib.org/misc/intel/IntelRDFPMathLib20U5.tar.gz")
set(CASIM_INTEL_SHA256 "85dafd70f0fe2a8da218ade4233fca9d3228b0b04cd6d8527f7499926be01037")
set(CASIM_INTEL_TARBALL "" CACHE FILEPATH "A local IntelRDFPMathLib20U5.tar.gz (checked against the pinned SHA-256)")

function(casim_intel_dfp sources_var include_var arch)
    set(dir "${CMAKE_BINARY_DIR}/intel-dfp")
    set(tgz "${dir}/IntelRDFPMathLib20U5.tar.gz")
    set(lib "${dir}/IntelRDFPMathLib20U5/LIBRARY")
    if(NOT EXISTS "${lib}/src/bid128_sin.c")
        file(MAKE_DIRECTORY "${dir}")
        if(CASIM_INTEL_TARBALL)
            configure_file("${CASIM_INTEL_TARBALL}" "${tgz}" COPYONLY)
        elseif(NOT EXISTS "${tgz}")
            message(STATUS "Casimir: fetching ${CASIM_INTEL_URL}")
            file(DOWNLOAD "${CASIM_INTEL_URL}" "${tgz}" EXPECTED_HASH SHA256=${CASIM_INTEL_SHA256} STATUS st)
            list(GET st 0 code)
            if(NOT code EQUAL 0)
                file(REMOVE "${tgz}")
                message(FATAL_ERROR "Casimir: could not fetch Intel's library: ${st}")
            endif()
        endif()
        file(SHA256 "${tgz}" got)
        if(NOT got STREQUAL CASIM_INTEL_SHA256)
            message(FATAL_ERROR "Casimir: ${tgz} has SHA-256 ${got}, not the pinned ${CASIM_INTEL_SHA256}")
        endif()
        execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf "${tgz}" WORKING_DIRECTORY "${dir}" RESULT_VARIABLE rc)
        if(NOT rc EQUAL 0)
            message(FATAL_ERROR "Casimir: could not unpack ${tgz}")
        endif()
        # The trig tables (295 KB each in sin, cos and tan) are only read but not declared const: made const, so a
        # microcontroller keeps them in flash (scripts/fetch-intel-dfp.sh does the same).
        foreach(f sin cos tan)
            file(READ "${lib}/src/bid128_${f}.c" text)
            string(REPLACE "\nstatic BID_UINT384 bid_decimal128_moduli[] =" "\nstatic const BID_UINT384 bid_decimal128_moduli[] ="
                   patched "${text}")
            if(patched STREQUAL text)
                message(FATAL_ERROR "Casimir: bid128_${f}.c: the moduli table was not found for the const patch")
            endif()
            file(WRITE "${lib}/src/bid128_${f}.c" "${patched}")
        endforeach()
    endif()

    file(STRINGS "${CASIM_INTEL_DFP_LIST_DIR}/../third_party/intel-dfp/sources.txt" names REGEX "^[^#]")
    set(srcs "")
    foreach(n ${names})
        set(f "${lib}/${n}")
        list(APPEND srcs "${f}")
        # Intel's makefile: two families, each with its own defines; the architecture by the pointer size
        if(n MATCHES "^float128/")
            set_source_files_properties("${f}" PROPERTIES COMPILE_DEFINITIONS "USE_NATIVE_QUAD_TYPE=0;${arch};linux;cc")
        else()
            set_source_files_properties("${f}" PROPERTIES COMPILE_DEFINITIONS
                "USE_COMPILER_F128_TYPE=0;USE_COMPILER_F80_TYPE=0;LINUX;${arch};DECIMAL_CALL_BY_REFERENCE=0;DECIMAL_GLOBAL_ROUNDING=0;DECIMAL_GLOBAL_EXCEPTION_FLAGS=0")
        endif()
    endforeach()
    set(${sources_var} "${srcs}" PARENT_SCOPE)
    set(${include_var} "${lib}/src" PARENT_SCOPE)
endfunction()
