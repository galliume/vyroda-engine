##################### Vyroda-Engine Project ##############
cmake_minimum_required(VERSION 3.28)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4")
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "3.29")
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "50702d73-207d-4767-8898-bf83236e7681")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "07d64865-edd6-429a-9139-2d67c4b3a7bc")
endif()

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})
set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING "Build type" FORCE)

include(cmake_checks)

project(Vyroda-Engine
  VERSION 0.2.1
  DESCRIPTION "Game Engine with a vulkan renderer"
  HOMEPAGE_URL "https://codeberg.org/galliume/vyroda-engine"
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
  
add_executable(${PROJECT_NAME} src/Vyroda.cpp vyroda.rc)

include(programs)

message(STATUS "Generated with config types: ${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "CMAKE_CXX_STANDARD: ${CMAKE_CXX_STANDARD}")
message(STATUS "CMAKE_CXX_STANDARD_REQUIRED: ${CMAKE_CXX_STANDARD_REQUIRED}")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

configure_file(VyrodaConfig.h.in VyrodaConfig.h)

#include(optims)
include(fetchDeps)
include(packages)
include(common_deps)
include(compiler_definitions)
include(compiler_flags)
include(compiler_links)
include(target_properties)
include(subdirs)
include(install)
include(custom_targets)
include(graphVizOptions)

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE
    VyrodaLib
    EditorManagers)

set_vyroda_definitions(${PROJECT_NAME})
set_vyroda_links(${PROJECT_NAME})
set_vyroda_compiler_flags(${PROJECT_NAME})
set_vyroda_target_properties(${PROJECT_NAME})
