#ifndef BVH_H_INCLUDED
#define BVH_H_INCLUDED

#include "kingfisher_core.h"
#include "aabb.h"

typedef struct BvhBuildNode {
  // A node is either an internal node or a leaf node.
  // If it is internal, `count` is the number of children.
  // If it is a leaf, `count` is the number of primitives.
  u32 count;
  Aabb *bounds;
  struct BvhBuildNode **children;
  u64 *prims;
} BvhBuildNode;

typedef struct EmbreeBvh {
  BvhBuildNode *root;
  void *bvh; // RTCBVH
} EmbreeBvh;

typedef struct Bvh {
  u64 node_count;

  u32 *index;
  u8 *meta;
  Aabb *bounds;

  u64 prim_count;
  u64 *prims;
} Bvh;

EmbreeBvh embree_bvh_build(Triangle const *triangles, i32 triangle_count);
void embree_bvh_free(EmbreeBvh *bvh);

void embree_bvh_intersect(EmbreeBvh const *bvh, Ray const *ray, Triangle const *triangles, HitRecord *record);

void bvh_build_from_embree_bvh(EmbreeBvh const *embree_bvh, Bvh *bvh);

#endif // BVH_H_INCLUDED
