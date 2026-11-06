# SPDX-License-Identifier: GPL-3.0-only
add_library(ws2_spectral SHARED module.cpp)
target_link_libraries(ws2_spectral PRIVATE wirescape_sdk2)
add_executable(abi2_spectral_tests tests.cpp ../../tests2/allocation_watch.cpp)
target_link_libraries(abi2_spectral_tests PRIVATE wirescape_sdk2)
add_test(NAME abi2_spectral_tests COMMAND abi2_spectral_tests)
