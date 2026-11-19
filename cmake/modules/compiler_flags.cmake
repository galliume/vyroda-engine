function(set_vyroda_compiler_flags target)

  message(STATUS "Setting compiler flags for ${target}.")

  # TODO: clean flags

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
    -march=native
    -fvisibility=hidden
    -DNDEBUG
    -Wno-reserved-module-identifier
    -Wno-reserved-identifier)

  set(RELWITHDEBINFO_OPTIONS
    -g
    -O3
    -Werror
    -fno-limit-debug-info
    -march=native
    -fvisibility=hidden)

  target_compile_options(${target} PRIVATE
    $<$<CONFIG:DEBUG>:${DEBUG_OPTIONS}>
    $<$<CONFIG:RELEASE>:${RELEASE_OPTIONS}>
    $<$<CONFIG:RELWITHDEBINFO>:${RELWITHDEBINFO_OPTIONS}>
    $<$<COMPILE_LANGUAGE:CXX>:-fexperimental-library>
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
    -Wno-thread-safety-negative
    -Wno-enum-enum-conversion #ktx error
    -Wno-reserved-identifier
    -Wno-lifetime-safety-intra-tu-constructor-suggestions
    -Wno-lifetime-safety-intra-tu-suggestions
    -Wno-lifetime-safety-cross-tu-constructor-suggestions
    -Wno-lifetime-safety-cross-tu-suggestions
    -Wno-lifetime-safety-invalidation
    -Wno-pass-failed)

endfunction()