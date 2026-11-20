add_executable(vyroda-cooker
  main.cpp)

target_link_libraries(vyroda-cooker
  PRIVATE
    Core)

set_vyroda_definitions(vyroda-cooker)
set_vyroda_links(vyroda-cooker)
set_vyroda_compiler_flags(vyroda-cooker)
set_vyroda_target_properties(vyroda-cooker)

add_custom_command(TARGET vyroda-cooker POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:vyroda-cooker> $<TARGET_RUNTIME_DLLS:vyroda-cooker>
  COMMAND_EXPAND_LISTS)
