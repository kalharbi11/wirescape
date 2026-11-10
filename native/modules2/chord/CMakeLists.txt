# SPDX-License-Identifier: GPL-3.0-only
add_library(ws2_chord SHARED module.cpp)
target_link_libraries(ws2_chord PRIVATE wirescape_sdk2)
add_executable(abi2_chord_tests tests.cpp ../../tests2/allocation_watch.cpp)
target_link_libraries(abi2_chord_tests PRIVATE wirescape_engine2 wirescape_voice2)
add_test(NAME abi2_chord_tests COMMAND abi2_chord_tests)
