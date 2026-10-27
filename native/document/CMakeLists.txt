# Optional QtCore document tooling; the engine remains independent of Qt.
find_package(Qt6 6.5 QUIET COMPONENTS Core)
if(NOT TARGET Qt6::Core)
  message(STATUS "Qt Core unavailable: document inspection tests disabled")
  return()
endif()
add_library(wirescape_document STATIC document_reader.cpp patch_document.cpp)
target_include_directories(wirescape_document PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wirescape_document PUBLIC Qt6::Core)
add_executable(document_reader_tests ../tests/document_reader_tests.cpp)
target_link_libraries(document_reader_tests PRIVATE wirescape_document)
add_test(NAME document_reader_tests COMMAND document_reader_tests "${PROJECT_SOURCE_DIR}/tests/fixtures/legacy_documents")
set_tests_properties(document_reader_tests PROPERTIES TIMEOUT 15
  ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>\;$ENV{PATH}")
add_executable(patch_document_tests ../tests/patch_document_tests.cpp)
target_link_libraries(patch_document_tests PRIVATE wirescape_document)
add_test(NAME patch_document_tests COMMAND patch_document_tests "${PROJECT_SOURCE_DIR}/tests/fixtures/legacy_documents")
set_tests_properties(patch_document_tests PROPERTIES TIMEOUT 15
  ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>\;$ENV{PATH}")
