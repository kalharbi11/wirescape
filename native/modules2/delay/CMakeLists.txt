# SPDX-License-Identifier: GPL-3.0-only
add_library(ws2_delay SHARED module.cpp)
target_link_libraries(ws2_delay PRIVATE wirescape_sdk2)
add_executable(abi2_delay_tests tests.cpp ../../tests2/allocation_watch.cpp)
target_link_libraries(abi2_delay_tests PRIVATE wirescape_engine2)
add_test(NAME abi2_delay_tests COMMAND abi2_delay_tests)
