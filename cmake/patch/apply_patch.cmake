if(NOT DEFINED PATCH_FILE)
  message(FATAL_ERROR "PATCH_FILE must be defined")
endif()

execute_process(
  COMMAND git apply --ignore-whitespace --reverse --check "${PATCH_FILE}"
  RESULT_VARIABLE already_applied
  OUTPUT_QUIET
  ERROR_QUIET
)

if(NOT already_applied EQUAL 0)
  message(STATUS "Applying patch: ${PATCH_FILE}")
  execute_process(
    COMMAND git apply --ignore-whitespace "${PATCH_FILE}"
    RESULT_VARIABLE res
  )
  if(NOT res EQUAL 0)
    message(FATAL_ERROR "Failed to apply patch: ${PATCH_FILE}")
  endif()
else()
  message(STATUS "Patch already applied: ${PATCH_FILE}")
endif()
