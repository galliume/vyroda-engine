add_library(VyrodaLib STATIC)

target_sources(VyrodaLib
  PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES
      BASE_DIRS .
      FILES
        Application.cppm
  PRIVATE
    Application.cpp)

target_include_directories(VyrodaLib PRIVATE ${CMAKE_BINARY_DIR})

target_link_libraries(
  VyrodaLib

  PRIVATE
    miniaudio
    EngineManagers
    EngineCommon

  PUBLIC
    tcl
    tk)

set_vyroda_definitions(VyrodaLib)
set_vyroda_links(VyrodaLib)
set_vyroda_compiler_flags(VyrodaLib)
