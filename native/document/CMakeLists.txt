# SPDX-License-Identifier: GPL-3.0-only
include(FetchContent)
FetchContent_Declare(nlohmann_json
  URL https://codeload.github.com/nlohmann/json/tar.gz/9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03
  URL_HASH SHA256=0dbc5e40a01ff142e7e68c03e85247a4dcede2f592d12d3677dee3664d17975a)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)
add_library(wirescape_runtime_document STATIC runtime_patch.cpp)
target_include_directories(wirescape_runtime_document PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wirescape_runtime_document PUBLIC wirescape_app_engine PRIVATE nlohmann_json::nlohmann_json)


add_executable(runtime_patch_tests runtime_patch_tests.cpp ../tests2/allocation_watch.cpp)
target_link_libraries(runtime_patch_tests PRIVATE wirescape_runtime_document nlohmann_json::nlohmann_json)
add_test(NAME runtime_patch_tests COMMAND runtime_patch_tests "${PROJECT_SOURCE_DIR}/examples/Six_Essentials_Demo.json")
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
add_test(NAME offline_play COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/cli/play_tests.py $<TARGET_FILE:wirescape_cli> "${PROJECT_SOURCE_DIR}/examples/Six_Essentials_Demo.json")
set_tests_properties(offline_play PROPERTIES TIMEOUT 45)

# Optional QtCore document tooling; the engine remains independent of Qt.
find_package(Qt6 6.5 QUIET COMPONENTS Core)
if(NOT TARGET Qt6::Core)
  message(STATUS "Qt Core unavailable: document inspection tests disabled")
  return()
endif()
add_library(wirescape_document STATIC document_reader.cpp patch_document.cpp app_patch.cpp)
target_include_directories(wirescape_document PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wirescape_document PUBLIC Qt6::Core wirescape_app_engine wirescape_runtime_document)
add_executable(document_reader_tests ../tests/document_reader_tests.cpp)
target_link_libraries(document_reader_tests PRIVATE wirescape_document)
add_test(NAME document_reader_tests COMMAND document_reader_tests "${PROJECT_SOURCE_DIR}/tests/fixtures/legacy_documents")
set_tests_properties(document_reader_tests PROPERTIES TIMEOUT 15
  ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>\;$ENV{PATH}")
add_executable(patch_document_tests ../tests/patch_document_tests.cpp)
target_link_libraries(patch_document_tests PRIVATE wirescape_document)
add_test(NAME patch_document_tests COMMAND patch_document_tests "${PROJECT_SOURCE_DIR}/tests/fixtures/legacy_documents"
  "${PROJECT_SOURCE_DIR}/../docs/specs/patch_v2_draft.example.json")
set_tests_properties(patch_document_tests PROPERTIES TIMEOUT 15
  ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>\;$ENV{PATH}")
add_executable(patch_render_tests ../tests/patch_render_tests.cpp)
target_include_directories(patch_render_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests2)
target_link_libraries(patch_render_tests PRIVATE wirescape_document wirescape_engine ${CMAKE_DL_LIBS})
add_dependencies(patch_render_tests ws_noise ws_clock ws_function_envelope ws_slew ws_vcf ws_ladder ws_vca ws_output)
add_test(NAME patch_render_tests COMMAND patch_render_tests $<TARGET_FILE_DIR:ws_noise>
  "${PROJECT_SOURCE_DIR}/modules" "${PROJECT_SOURCE_DIR}/examples/Six_Essentials_Demo.json")
set_tests_properties(patch_render_tests PROPERTIES TIMEOUT 30
  ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>\;$ENV{PATH}")
add_executable(app_patch_tests app_patch_tests.cpp ../tests2/allocation_watch.cpp)
target_link_libraries(app_patch_tests PRIVATE wirescape_document)
add_test(NAME app_patch_tests COMMAND app_patch_tests $<TARGET_FILE_DIR:ws_noise> "${PROJECT_SOURCE_DIR}/examples/Six_Essentials_Demo.json")
