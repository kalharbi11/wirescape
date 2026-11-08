# SPDX-License-Identifier: GPL-3.0-only
add_library(ws2_resonator SHARED module.cpp)
target_link_libraries(ws2_resonator PRIVATE wirescape_sdk2)
add_executable(abi2_resonator_tests tests.cpp ../../tests2/allocation_watch.cpp)
target_link_libraries(abi2_resonator_tests PRIVATE wirescape_sdk2)
add_test(NAME abi2_resonator_tests COMMAND abi2_resonator_tests)
