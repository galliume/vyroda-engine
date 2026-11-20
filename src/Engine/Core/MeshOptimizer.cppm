module;

#include <meshoptimizer.h>

export module Engine.Core.MeshOptimizer;

export
{
  using ::meshopt_optimizeVertexCache;
  using ::meshopt_optimizeOverdraw;
  using ::meshopt_optimizeVertexFetch;
  using ::meshopt_optimizeVertexFetchRemap;
  using ::meshopt_remapIndexBuffer;
  using ::meshopt_remapVertexBuffer;
  using ::meshopt_generateVertexRemap;
  using ::meshopt_simplify;
  using ::meshopt_buildMeshlets;
  using ::meshopt_buildMeshletsBound;
  using ::meshopt_computeMeshletBounds;
  using ::meshopt_Meshlet;
  using ::meshopt_Bounds;
}
