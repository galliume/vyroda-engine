add_library(EditorManagers STATIC)

target_sources(EditorManagers
  PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES
      FILES
        EditorManager.cppm
        LevelManager.cppm
        ExplorerManager.cppm

  PRIVATE
    EditorManager.cpp
    LevelManager.cpp
    ExplorerManager.cpp)

target_link_libraries(
  EditorManagers 

  PRIVATE
    Components
    EngineManagers
    VyrodaLib
    EngineCommon

  PUBLIC
    glfw
    tcl
    tk)

set_vyroda_definitions(EditorManagers)
set_vyroda_links(EditorManagers)
set_vyroda_compiler_flags(EditorManagers)
set_vyroda_target_properties(EditorManagers)