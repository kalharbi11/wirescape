# SPDX-License-Identifier: GPL-3.0-only
# Accept repeat configuration and either checkout line-ending convention.
file(GLOB patches "${PATCH_DIR}/*.patch")
list(SORT patches)
foreach(patch IN LISTS patches)
  file(READ "${patch}" text)
  string(REPLACE "\r\n" "\n" text "${text}")
  get_filename_component(name "${patch}" NAME)
  set(normalized "${SOURCE_DIR}/.wirescape-${name}")
  file(WRITE "${normalized}" "${text}")
  execute_process(COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace --reverse --check "${normalized}"
    WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE applied OUTPUT_QUIET ERROR_QUIET)
  if(NOT applied EQUAL 0)
    execute_process(COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace "${normalized}"
      WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
      message(FATAL_ERROR "Pinned Macro Oscillator patch failed: ${name}")
    endif()
  endif()
endforeach()
