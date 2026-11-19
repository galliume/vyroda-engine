function(set_vyroda_links target)

  if(WIN32)
    set(LINK_OPTIONS
      -fexperimental-library)
    list(APPEND EXTRA_LIBS
      Ws2_32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32)
 else()
  set(LINK_OPTIONS
    -pthread
    -fexperimental-library)
 endif()

  target_link_options(${target} PRIVATE
    $<$<CONFIG:DEBUG>:${LINK_OPTIONS}>
    $<$<CONFIG:RELEASE>:${LINK_OPTIONS}>
    $<$<CONFIG:RELWITHDEBINFO>:${LINK_OPTIONS}>)

  message(STATUS "EXTRA_LIBS: ${EXTRA_LIBS}")
  target_link_libraries(${target} PRIVATE ${EXTRA_LIBS})

endfunction()