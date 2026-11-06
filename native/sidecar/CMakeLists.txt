# SPDX-License-Identifier: GPL-3.0-only
execute_process(COMMAND git rev-parse --short=12 HEAD WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE engine_sha OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
add_executable(wirescape_engine_sidecar main.cpp protocol.cpp session.cpp devices.cpp)
set_target_properties(wirescape_engine_sidecar PROPERTIES OUTPUT_NAME wirescape-engine)
target_compile_definitions(wirescape_engine_sidecar PRIVATE WIRESCAPE_ENGINE_SHA="${engine_sha}")
target_link_libraries(wirescape_engine_sidecar PRIVATE wirescape_runtime_document wirescape_audio_service
  wirescape_midi_host nlohmann_json::nlohmann_json Threads::Threads)
add_test(NAME abi2_sidecar_protocol_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/protocol_tests.py
  $<TARGET_FILE:wirescape_engine_sidecar> ${PROJECT_SOURCE_DIR}/examples)
set_tests_properties(abi2_sidecar_protocol_tests PROPERTIES TIMEOUT 90)
add_executable(sidecar_protocol_unit_tests protocol_unit_tests.cpp protocol.cpp devices.cpp ../tests2/allocation_watch.cpp)
target_link_libraries(sidecar_protocol_unit_tests PRIVATE wirescape_runtime_document wirescape_audio_service
  wirescape_midi_host nlohmann_json::nlohmann_json Threads::Threads)
add_test(NAME abi2_sidecar_unit_tests COMMAND sidecar_protocol_unit_tests)
set_tests_properties(abi2_sidecar_unit_tests PROPERTIES TIMEOUT 30)
