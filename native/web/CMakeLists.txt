# SPDX-License-Identifier: GPL-3.0-only
# Standalone build of the browser demo engine. Native: a test executable.
# Emscripten (emcmake): a standalone wirescape.wasm for the AudioWorklet.
# Only Qt-free, device-free sources; no PortAudio, libremidi or sockets.
cmake_minimum_required(VERSION 3.20)
project(WirescapeWeb LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(FetchContent)
find_package(Git REQUIRED)
set(native ${CMAKE_CURRENT_SOURCE_DIR}/..)
# Same pinned revisions as native/document and native/modules2/macro_oscillator.
FetchContent_Declare(nlohmann_json
  URL https://codeload.github.com/nlohmann/json/tar.gz/9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03
  URL_HASH SHA256=0dbc5e40a01ff142e7e68c03e85247a4dcede2f592d12d3677dee3664d17975a)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(macro_dir ${native}/modules2/macro_oscillator)
FetchContent_Declare(macro_eurorack
  URL https://codeload.github.com/pichenettes/eurorack/tar.gz/f4cfa7697095a28d292bff758beb51deb49b5a86
  URL_HASH SHA256=7bade44c4b3839c7e505a128e68896cf0b039849b8bc824a41e0051e1dfb016e
  PATCH_COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=<SOURCE_DIR> -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
    -DPATCH_DIR=${macro_dir}/patches -P ${macro_dir}/patches/apply.cmake
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(macro_stmlib
  URL https://codeload.github.com/pichenettes/stmlib/tar.gz/e3bd7c9cc00e4364166f9905c0509b6ffd0535ec
  URL_HASH SHA256=eff5d6642a93498996661e0dd4b5a1de4b98093f25a009166910726d1262b5d2
  SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/upstream/stmlib
  PATCH_COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=<SOURCE_DIR> -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
    -DPATCH_FILE=${macro_dir}/patches/stmlib-msvc-inline.patch -P ${macro_dir}/patches/apply.cmake
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nlohmann_json macro_eurorack macro_stmlib)
file(GLOB_RECURSE macro_sources CONFIGURE_DEPENDS ${macro_eurorack_SOURCE_DIR}/plaits/dsp/*.cc)
add_library(web_macro STATIC ${macro_sources} ${macro_eurorack_SOURCE_DIR}/plaits/resources.cc
  ${macro_stmlib_SOURCE_DIR}/dsp/units.cc)
target_include_directories(web_macro PUBLIC ${macro_eurorack_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/upstream ${macro_dir})
target_compile_definitions(web_macro PUBLIC TEST)
target_compile_options(web_macro PUBLIC -include ${macro_dir}/rng_scope.hpp)

add_library(web_core STATIC ${native}/engine2/engine2.cpp ${native}/control/app_engine.cpp
  ${native}/document/runtime_patch.cpp ${native}/src/engine.cpp ${native}/voice/allocator.cpp)
target_include_directories(web_core PUBLIC ${native}/include ${native}/sdk ${native}/engine2 ${native}/control
  ${native}/document ${native}/voice)
target_compile_definitions(web_core PUBLIC WS_ENGINE_BUILD)
target_link_libraries(web_core PUBLIC web_macro nlohmann_json::nlohmann_json ${CMAKE_DL_LIBS})

if(EMSCRIPTEN)
  # Standalone module (no JS glue): the worklet instantiates it with a few WASI stubs.
  add_executable(wirescape_web web_engine.cpp)
  target_link_libraries(wirescape_web PRIVATE web_core)
  set_target_properties(wirescape_web PROPERTIES OUTPUT_NAME wirescape SUFFIX .wasm)
  set(ws_exports _wsw_init,_wsw_catalogue,_wsw_command,_wsw_telemetry,_wsw_render,_wsw_right,_wsw_peak,_wsw_alloc,_wsw_free)
  target_link_options(wirescape_web PRIVATE -sSTANDALONE_WASM=1 --no-entry -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=33554432 -sSTACK_SIZE=1048576 -sEXPORTED_FUNCTIONS=${ws_exports} -sERROR_ON_UNDEFINED_SYMBOLS=0)
  # Native wasm exceptions: no JS glue needed (Chrome 95+, Firefox 100+, Safari 15.2+).
  target_compile_options(web_core PUBLIC -fwasm-exceptions -O3)
  target_compile_options(web_macro PUBLIC -fwasm-exceptions -O3)
  target_link_options(wirescape_web PRIVATE -fwasm-exceptions -O3)
else()
  enable_testing()
  add_executable(web_engine_tests web_engine_tests.cpp web_engine.cpp)
  target_link_libraries(web_engine_tests PRIVATE web_core)
  add_test(NAME web_engine_tests COMMAND web_engine_tests ${native}/examples)
endif()
