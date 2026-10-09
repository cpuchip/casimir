# The WebAssembly module (emscripten; README.md): casim.js and casim.wasm, an ES module whose factory
# wasm/casim.mjs wraps. Included by CMakeLists.txt under emcmake.
add_executable(casim_wasm wasm/casim_js.c)
target_link_libraries(casim_wasm PRIVATE casim)
set_target_properties(casim_wasm PROPERTIES OUTPUT_NAME casim SUFFIX ".js" C_STANDARD 11)
target_compile_options(casim_wasm PRIVATE -Wall -Wextra -Werror)
target_link_options(casim_wasm PRIVATE
    -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createCasimirModule -sENVIRONMENT=web,worker,node
    -sEXPORTED_FUNCTIONS=_casimjs_run,_casimjs_out,_casimjs_version -sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString
    -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=4194304 -sALLOW_MEMORY_GROWTH=0 -sFILESYSTEM=0)
add_custom_command(TARGET casim_wasm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/wasm/casim.mjs" "$<TARGET_FILE_DIR:casim_wasm>/casim.mjs")
