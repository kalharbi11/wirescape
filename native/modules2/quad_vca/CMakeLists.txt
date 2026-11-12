# SPDX-License-Identifier: GPL-3.0-only
add_library(ws2_quad_vca SHARED module.cpp)
target_link_libraries(ws2_quad_vca PRIVATE wirescape_sdk2)
add_executable(abi2_quad_vca_tests tests.cpp ../../tests2/allocation_watch.cpp)
target_link_libraries(abi2_quad_vca_tests PRIVATE wirescape_engine2)
add_test(NAME abi2_quad_vca_tests COMMAND abi2_quad_vca_tests)
