add_library(Components STATIC)

target_sources(Components
  PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES
      FILES
        Components.cppm
        Entity.cppm
        EntityNode.cppm
        Mesh.cppm
        Texture.cppm
  PRIVATE
    Entity.cpp
    Mesh.cpp)

target_link_libraries(
  Components

  PRIVATE
    Core
    Utils
    EngineCommon

  PUBLIC
    freetype)

set_vyroda_definitions(Components)
set_vyroda_links(Components)
set_vyroda_compiler_flags(Components)
set_vyroda_target_properties(Components)