message(NOTICE "Fetching Tcl/TK ...")

if (WIN32)
set(TCL_TCLSH
    "${CMAKE_SYSROOT}/bin/tclsh91.exe"
    CACHE FILEPATH "Tcl shell executable" FORCE)

set(TCL_INCLUDE_PATH
    "${CMAKE_SYSROOT}/include/tcl9.0"
    CACHE PATH "Tcl include directory" FORCE)

set(TCL_LIBRARY
    "${CMAKE_SYSROOT}/lib/libtcl91.dll.a"
    CACHE FILEPATH "Tcl library" FORCE)

set(TK_INCLUDE_PATH
    "${CMAKE_SYSROOT}/include/tcl9.0"
    CACHE PATH "Tk include directory" FORCE)

set(TK_LIBRARY
    "${CMAKE_SYSROOT}/lib/libtcl9tk91.dll.a"
    CACHE FILEPATH "Tk library" FORCE)

set(TK_WISH
    "${CMAKE_SYSROOT}/bin/wish90.exe"
    CACHE FILEPATH "Tk wish executable" FORCE)
endif()

find_package(TCL REQUIRED)

message(STATUS "Found Tcl version: ${TCL_VERSION}")
message(STATUS "TCL_INCLUDE_PATH = ${TCL_INCLUDE_PATH}")
message(STATUS "TCL_LIBRARY = ${TCL_LIBRARY}")
message(STATUS "TCL_TCLSH = ${TCL_TCLSH}")
message(STATUS "TK_INCLUDE_PATH = ${TK_INCLUDE_PATH}")
message(STATUS "TK_LIBRARY = ${TK_LIBRARY}")

#todo: add flag to activate or not the tcl/tk editor

add_library(tcl INTERFACE)

target_include_directories(tcl
  SYSTEM INTERFACE ${TCL_INCLUDE_PATH})

target_link_libraries(tcl INTERFACE ${TCL_LIBRARY})

add_library(tk INTERFACE)

target_include_directories(tk
  SYSTEM INTERFACE ${TK_INCLUDE_PATH})

target_link_libraries(tk INTERFACE ${TK_LIBRARY})