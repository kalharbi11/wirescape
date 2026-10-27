# SPDX-License-Identifier: GPL-3.0-only
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
add_test(NAME capture_offline_selftests COMMAND ${Python3_EXECUTABLE}
  ${PROJECT_SOURCE_DIR}/../tools/capture/test_capture.py
  --engine $<TARGET_FILE:wirescape_engine> --modules $<TARGET_FILE_DIR:ws_vco>)
set_tests_properties(capture_offline_selftests PROPERTIES TIMEOUT 60)
# Compile the manual runner, but NEVER execute it in CTest or CI.
if(WIN32)
  add_executable(wirescape_capture capture.cpp)
  target_compile_definitions(wirescape_capture PRIVATE _WIN32_WINNT=0x0A00)
  target_link_libraries(wirescape_capture PRIVATE ole32 uuid mmdevapi)
endif()
