message(STATUS "Fetching dependancies.")

set(FETCHCONTENT_QUIET OFF)

set(ASSIMP_TAG v6.0.6) #https://github.com/assimp/assimp/tags
set(FREETYPE_TAG VER-2-15-0) #https://github.com/freetype/freetype/tags
set(FMT_TAG 12.2.1) #https://github.com/fmtlib/fmt/tags
set(GLFW_TAG 3.5.1) #https://github.com/glfw/glfw/tags
set(GLM_TAG 1.0.3) #https://github.com/g-truc/glm/tags
set(KTX_TAG v4.4.2) #https://github.com/KhronosGroup/KTX-Software/tags
set(LUA_TAG v5.5.1) #https://github.com/lua/lua/tags
set(MINIAUDIO_TAG  0.11.26) #https://github.com/mackron/miniaudio/tags
set(NH_JSON_TAG v3.12.0) #https://github.com/nlohmann/json/tags
set(STB_TAG master) #https://github.com/nothings/stb
set(SQLITE_TAG 3540100) #https://www.sqlite.org/download.html https://github.com/sqlite/sqlite/tags
set(VOLK_TAG vulkan-sdk-1.4.363.0) #https://github.com/zeux/volk/tags
#set(WxWidget_TAG v3.3.0)
set(FETCH_GIT_SHALLOW TRUE)
set(FETCH_GIT_PROGRESS TRUE)
set(FETCH_UPDATE_DISCONNECTED TRUE)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

include(FetchContent)

include(deps/FetchAssimp)
include(deps/FetchFreeType)
include(deps/FetchGLFW)
include(deps/FetchGLM)
include(deps/FetchFMT)
include(deps/FetchKTX)
include(deps/FetchLua)
include(deps/FetchMiniAudio)
include(deps/FetchNlohmannJson)
include(deps/FetchSTB)
include(deps/FetchSQlite)
include(deps/FetchTclTk)
include(deps/FetchVolk)
#include(deps/FetchWxWidgets)

function(silence_target_warnings tgt)
  if(TARGET ${tgt})
    get_target_property(target_type ${tgt} TYPE)
    if(target_type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY|EXECUTABLE)$")
      target_compile_options(${tgt} PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/w>
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>
        $<$<CXX_COMPILER_ID:MSVC>:/w>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
      )
    endif()
  endif()
endfunction()

function(silence_dependency_warnings dir)
  get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
  foreach(subdir ${subdirs})
    silence_dependency_warnings("${subdir}")
  endforeach()
  get_property(targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(tgt ${targets})
    if(NOT tgt STREQUAL "${PROJECT_NAME}")
      silence_target_warnings(${tgt})
    endif()
  endforeach()
endfunction()

silence_dependency_warnings(${CMAKE_CURRENT_SOURCE_DIR})

message(STATUS "dependancies OK.")