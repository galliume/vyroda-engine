set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

message(NOTICE "Fetching meshoptimizer from https://github.com/zeux/meshoptimizer ...")

FetchContent_Declare(
  fetch_meshoptimizer
  GIT_REPOSITORY https://github.com/zeux/meshoptimizer
  GIT_TAG ${MESHOPTIMIZER_TAG}
  GIT_SHALLOW ${FETCH_GIT_SHALLOW}
  GIT_PROGRESS ${FETCH_GIT_PROGRESS}
  UPDATE_DISCONNECTED ${FETCH_UPDATE_DISCONNECTED}
  SYSTEM)

FetchContent_MakeAvailable(fetch_meshoptimizer)
