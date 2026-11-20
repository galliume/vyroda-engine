message(STATUS "Setting compiler targets.")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${PROJECT_NAME}> $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
  COMMAND_EXPAND_LISTS)

add_custom_target(
  CompilShaders ${CMAKE_CURRENT_SOURCE_DIR}/bin/shaders_compil.sh
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_target(cook_assets
  COMMAND $<TARGET_FILE:vyroda-cooker> --root "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS vyroda-cooker
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Cooking .vmesh and .ktx2 assets...")
