add_library(GUI STATIC)

target_sources(GUI
  PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES
      FILES
        Window.cppm
  PRIVATE
    Window.cpp)

target_link_libraries(
  GUI

  PRIVATE
    Core
    EngineCommon

  PUBLIC
    glfw
    stb)

set_vyroda_definitions(GUI)
set_vyroda_links(GUI)
set_vyroda_compiler_flags(GUI)