# SPDX-License-Identifier: GPL-3.0-only
include(FetchContent)
find_package(Git REQUIRED)
FetchContent_Declare(macro_eurorack
  URL https://codeload.github.com/pichenettes/eurorack/tar.gz/f4cfa7697095a28d292bff758beb51deb49b5a86
  URL_HASH SHA256=7bade44c4b3839c7e505a128e68896cf0b039849b8bc824a41e0051e1dfb016e
  PATCH_COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=<SOURCE_DIR>
    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
    -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/patches/unsigned-pow2-shift.patch
    -P ${CMAKE_CURRENT_SOURCE_DIR}/patches/apply.cmake
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(macro_stmlib
  URL https://codeload.github.com/pichenettes/stmlib/tar.gz/e3bd7c9cc00e4364166f9905c0509b6ffd0535ec
  URL_HASH SHA256=eff5d6642a93498996661e0dd4b5a1de4b98093f25a009166910726d1262b5d2
  SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/upstream/stmlib
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(macro_eurorack macro_stmlib)
file(GLOB_RECURSE macro_sources CONFIGURE_DEPENDS ${macro_eurorack_SOURCE_DIR}/plaits/dsp/*.cc)
add_library(macro_upstream STATIC ${macro_sources} ${macro_eurorack_SOURCE_DIR}/plaits/resources.cc ${macro_stmlib_SOURCE_DIR}/dsp/units.cc)
set_target_properties(macro_upstream PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(macro_upstream PUBLIC ${macro_eurorack_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/upstream ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(macro_upstream PUBLIC TEST $<$<CXX_COMPILER_ID:MSVC>:_USE_MATH_DEFINES>)
target_compile_features(macro_upstream PUBLIC cxx_std_17)
# Storage adapter plus the single planner-authorized unsigned exponent shift.
if(MSVC)
  target_compile_options(macro_upstream PUBLIC /FI${CMAKE_CURRENT_SOURCE_DIR}/rng_scope.h)
else()
  target_compile_options(macro_upstream PUBLIC -include ${CMAKE_CURRENT_SOURCE_DIR}/rng_scope.h)
endif()
add_library(ws2_macro_oscillator SHARED module.cpp)
target_link_libraries(ws2_macro_oscillator PRIVATE wirescape_sdk2 macro_upstream)
add_executable(abi2_macro_oscillator_tests tests.cpp ../../tests2/allocation_watch.cpp)
target_link_libraries(abi2_macro_oscillator_tests PRIVATE wirescape_engine2 macro_upstream Threads::Threads)
add_test(NAME abi2_macro_oscillator_tests COMMAND abi2_macro_oscillator_tests)
