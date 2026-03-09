add_library(Network STATIC)

target_sources(Network
  PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES
      FILES
        Socket.cppm
        Server.cppm
  PRIVATE 
    Socket.cpp
    Server.cpp)


target_link_libraries(
  Network

  PRIVATE
    Core)

set_vyroda_definitions(Network)
set_vyroda_links(Network)
set_vyroda_compiler_flags(Network)
set_vyroda_target_properties(Network)