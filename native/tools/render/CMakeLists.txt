# SPDX-License-Identifier: GPL-3.0-only
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
add_executable(wirescape_render main.cpp)
target_link_libraries(wirescape_render PRIVATE wirescape_runtime_document nlohmann_json::nlohmann_json)
foreach(case_name adsr-notes frequency-step blend-selection)
  string(REPLACE "-" "_" test_name "${case_name}")
  add_test(NAME render_evidence_${test_name}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/evidence.py
      --render $<TARGET_FILE:wirescape_render>
      --case ${CMAKE_CURRENT_SOURCE_DIR}/cases/${case_name}.json
      --out ${CMAKE_BINARY_DIR}/render_evidence/${case_name}
      --compiler "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
      --configuration "$<CONFIG>")
  set_tests_properties(render_evidence_${test_name} PROPERTIES TIMEOUT 90)
endforeach()
add_test(NAME render_evidence_negative COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/evidence.py --negative)
set_tests_properties(render_evidence_negative PROPERTIES TIMEOUT 30)
