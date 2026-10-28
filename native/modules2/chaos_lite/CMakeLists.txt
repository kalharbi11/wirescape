# SPDX-License-Identifier: GPL-3.0-only
add_library(ws2_chaos_lite SHARED module.cpp)
target_link_libraries(ws2_chaos_lite PRIVATE wirescape_sdk2)
add_executable(abi2_chaos_lite_tests tests.cpp ../../tests2/allocation_watch.cpp)
target_link_libraries(abi2_chaos_lite_tests PRIVATE wirescape_engine2)
add_test(NAME abi2_chaos_lite_tests COMMAND abi2_chaos_lite_tests)
