function(set_vyroda_compiler_flags target)

  message(STATUS "Setting compiler flags for ${target}.")

  set(DEBUG_OPTIONS
    -g
    -Og
    -Werror
    -fno-inline
    -fno-limit-debug-info
    -fno-inline-functions
    -fno-omit-frame-pointer)

  set(RELEASE_OPTIONS
    -O3
    -Werror
    #-ffast-math
    -march=native
    -fvisibility=hidden
    -DNDEBUG)

  set(RELWITHDEBINFO_OPTIONS
    -g
    -O3
    -Werror
    -fno-limit-debug-info
    #-ffast-math
    -march=native
    -fvisibility=hidden
    -fno-omit-frame-pointer)

  target_compile_options(${target} PRIVATE
    $<$<CONFIG:DEBUG>:${DEBUG_OPTIONS}>
    $<$<CONFIG:RELEASE>:${RELEASE_OPTIONS}>
    $<$<CONFIG:RELWITHDEBINFO>:${RELWITHDEBINFO_OPTIONS}>
    -Weverything
    -fno-strict-aliasing
    -Wno-c++98-compat
    -Wno-c++98-compat-pedantic
    -Wno-c++20-compat
    -Wno-old-style-cast
    -Wno-padded
    -Wno-documentation
    -Wno-documentation-unknown-command
    -Wno-decls-in-multiple-modules
    -Wno-unsafe-buffer-usage
    -Wno-covered-switch-default
    -Wno-deprecated-declarations
    -Wno-thread-safety-negative)

endfunction()