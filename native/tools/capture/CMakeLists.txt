# SPDX-License-Identifier: GPL-3.0-only
find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)
add_test(NAME capture_offline_selftests COMMAND ${Python3_EXECUTABLE}
  ${PROJECT_SOURCE_DIR}/../tools/capture/test_capture.py
  --engine $<TARGET_FILE:wirescape_engine> --modules $<TARGET_FILE_DIR:ws_vco>)
set_tests_properties(capture_offline_selftests PROPERTIES TIMEOUT 60)
