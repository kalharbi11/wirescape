# SPDX-License-Identifier: GPL-3.0-only
add_library(ws2_reverb SHARED module.cpp)
target_link_libraries(ws2_reverb PRIVATE wirescape_sdk2)
add_executable(abi2_reverb_tests tests.cpp ../../tests2/allocation_watch.cpp)
target_link_libraries(abi2_reverb_tests PRIVATE wirescape_sdk2)
add_test(NAME abi2_reverb_tests COMMAND abi2_reverb_tests)
