# SPDX-License-Identifier: GPL-3.0-only
add_library(ws2_blend SHARED module.cpp)
target_link_libraries(ws2_blend PRIVATE wirescape_sdk2)
add_executable(abi2_blend_tests tests.cpp ../../tests2/allocation_watch.cpp)
target_link_libraries(abi2_blend_tests PRIVATE wirescape_sdk2)
add_test(NAME abi2_blend_tests COMMAND abi2_blend_tests)
