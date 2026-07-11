
function(set_vyroda_definitions target)

  message(STATUS "Setting compiler definitions for ${target}.")

  target_compile_definitions(${target}
    PRIVATE
      VK_NO_PROTOTYPES
      GLM_FORCE_DEPTH_ZERO_TO_ONE
      GLM_ENABLE_EXPERIMENTAL)

  if(WIN32)
    target_compile_definitions(${target} PRIVATE
        VOLK_STATIC_DEFINES
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN)
  endif()

  target_compile_definitions(${target} PRIVATE
    $<$<CONFIG:DEBUG>:VYR_DEBUG_BUILD>
    $<$<CONFIG:RELWITHDEBINFO>:VYR_DEBUG_BUILD>)

endfunction()