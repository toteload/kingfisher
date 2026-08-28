#pragma warning(push)
#pragma warning(disable: 4324)
#include <embree4/rtcore.h>
#pragma warning(pop)

#include <SDL3/SDL.h>

#include "bvh.h"

Aabb aabb_from_RTCBounds(struct RTCBounds const *bounds) {
  return (Aabb){
    .lo = {{ bounds->lower_x, bounds->lower_y, bounds->lower_z, }},
    .hi = {{ bounds->upper_x, bounds->upper_y, bounds->upper_z, }},
  };
}

Aabb aabb_from_triangle(Triangle const *triangle) {
  return (Aabb){
    .lo = glms_vec3_minv(triangle->v0, glms_vec3_minv(triangle->v1, triangle->v2)),
    .hi = glms_vec3_maxv(triangle->v0, glms_vec3_maxv(triangle->v1, triangle->v2)),
  };
}

// Callback to create a node
void *embree_callback_create_node(
  RTCThreadLocalAllocator allocator,
  u32 child_count,
  void *user
) {
  BvhBuildNode *node = rtcThreadLocalAlloc(allocator, sizeof(BvhBuildNode), 16);
  Aabb *bounds = rtcThreadLocalAlloc(allocator, child_count * sizeof(Aabb), 16);
  BvhBuildNode **children = rtcThreadLocalAlloc(allocator, child_count * sizeof(BvhBuildNode*), 16);
  
  *node = (BvhBuildNode){
    .bounds = bounds,
    .children = children,
    .count = child_count,
    .prims = NULL,
  };

  return node;
}

// Callback to set the pointer to all children
void embree_callback_set_node_children(void *pnode, void **pchildren, u32 child_count, void *user) {
  BvhBuildNode *node = pnode;
  BvhBuildNode **children = (BvhBuildNode**)pchildren;

  for (u32 i = 0; i < child_count; i++) {
    node->children[i] = children[i];
  }
}

// Callback to set the bounds of all children
void embree_callback_set_node_bounds(void *pnode, struct RTCBounds const **bounds, u32 child_count, void *user) {
  BvhBuildNode *node = pnode;

  for (u32 i = 0; i < child_count; i++) {
    node->bounds[i] = aabb_from_RTCBounds(bounds[i]);
  }
}

void *embree_callback_create_leaf(RTCThreadLocalAllocator allocator, struct RTCBuildPrimitive const *build_prims, size_t prim_count, void *user) {
  BvhBuildNode *node = rtcThreadLocalAlloc(allocator, sizeof(BvhBuildNode), 16);
  u64 *prims = rtcThreadLocalAlloc(allocator, prim_count * sizeof(u64), 8);

  for (u64 i = 0; i < prim_count; i++) {
    prims[i] = (((u64)build_prims[i].geomID) << 32) | ((u64)build_prims[i].primID);
  }

  *node = (BvhBuildNode){
    .count = (u32)prim_count,
    .prims = prims,
    .bounds = NULL,
    .children = NULL,
  };

  return node;
}

void embree_callback_split_primitive(
  struct RTCBuildPrimitive const *prim,
  u32 dim,
  f32 position,
  struct RTCBounds *left,
  struct RTCBounds *right,
  void *user
) {
  *left = (struct RTCBounds){
    .lower_x = prim->lower_x,
    .lower_y = prim->lower_y,
    .lower_z = prim->lower_z,
    .upper_x = prim->upper_x,
    .upper_y = prim->upper_y,
    .upper_z = prim->upper_z,
  };

  *right = (struct RTCBounds){
    .lower_x = prim->lower_x,
    .lower_y = prim->lower_y,
    .lower_z = prim->lower_z,
    .upper_x = prim->upper_x,
    .upper_y = prim->upper_y,
    .upper_z = prim->upper_z,
  };

  switch (dim) {
    case 0: {
      left->upper_x = position;
      right->lower_x = position;
    } break;
    case 1: {
      left->upper_y = position;
      right->lower_y = position;
    } break;
    case 2: {
      left->upper_z = position;
      right->lower_z = position;
    } break;
  }
}

EmbreeBvh embree_bvh_build(Triangle const *triangles, u64 triangle_count) {
  RTCDevice dev = rtcNewDevice("");
  if(!dev) {
    return (EmbreeBvh){
      .root = NULL,
    };
  }

  u64 prim_count = triangle_count;

  // Embree needs extra memory to build the BVH.
  u64 prim_array_capacity = 2 * prim_count;

  struct RTCBuildPrimitive *prims = malloc(2 * triangle_count * sizeof(struct RTCBuildPrimitive));
  for (u32 i = 0; i < triangle_count; i++) {
    Aabb bounds = aabb_from_triangle(&triangles[i]);
    prims[i] = (struct RTCBuildPrimitive){
      .lower_x = bounds.lo.x,
      .lower_y = bounds.lo.y,
      .lower_z = bounds.lo.z,
      .upper_x = bounds.hi.x,
      .upper_y = bounds.hi.y,
      .upper_z = bounds.hi.z,
      .geomID = 0,
      .primID = i,
    };
  }

  RTCBVH bvh = rtcNewBVH(dev);

  struct RTCBuildArguments args = (struct RTCBuildArguments){
    .byteSize = sizeof(struct RTCBuildArguments),

    .buildQuality = RTC_BUILD_QUALITY_HIGH,
    .buildFlags = RTC_BUILD_FLAG_NONE,

    .maxBranchingFactor = 2,
    .maxDepth = 64,
    .sahBlockSize = 1,
    .minLeafSize = 1,
    .maxLeafSize = 4,

    .traversalCost = 1.0f,
    .intersectionCost = 1.0f,

    .bvh = bvh,
    .primitives = prims,
    .primitiveCount = prim_count,
    .primitiveArrayCapacity = prim_array_capacity,

    .createNode      = embree_callback_create_node,
    .setNodeChildren = embree_callback_set_node_children,
    .setNodeBounds   = embree_callback_set_node_bounds,
    .createLeaf      = embree_callback_create_leaf,
    .splitPrimitive  = embree_callback_split_primitive,

    .buildProgress = NULL,
    .userPtr = NULL,
  };

  BvhBuildNode *root = rtcBuildBVH(&args);

  free(prims);

  return (EmbreeBvh){
    .root = root,
    .bvh = bvh,
  };
}

void embree_free_bvh(EmbreeBvh *bvh) {
  rtcReleaseBVH(bvh->bvh);
}

void embree_bvh_intersect(
  EmbreeBvh const *bvh,
  Ray const *ray,
  Triangle const *triangles,
  HitRecord *record
) {
  BvhBuildNode *stack[64] = { bvh->root };
  u32 top = 1;

  HitRecord rec = {
    .t = F32_NO_HIT,
  };

  vec3s reciprocal_ray_dir = vec3_reciprocal(ray->dir);

  while (top) {
    BvhBuildNode *node = stack[--top];

    bool is_leaf = node->prims != NULL;

    if (is_leaf) {
      for (u32 i = 0; i < node->count; i++) {
        u64 prim_idx = node->prims[i];
        Triangle const *tri = &triangles[prim_idx];

        TriangleHit hit;
        bool has_hit = ray_triangle_intersect(ray, tri, &hit);
        if (!has_hit) {
          continue;
        }

        if (hit.t < rec.t) {
          rec = (HitRecord){
            .t = hit.t,
            .u = hit.u,
            .v = hit.v,
            .idx = prim_idx,
          };
        }
      }
    } else {
      for (u32 i = 0; i < node->count; i++) {
        Aabb *bound = &node->bounds[i];

        f32 t = aabb_intersect(bound, ray->origin, reciprocal_ray_dir);
        if (t == F32_NO_HIT) {
          continue;
        }

        stack[top++] = node->children[i];
      }
    }
  }

  *record = rec;
}

typedef struct BvhBuildContext {
  u64 node_offset;
  u64 prim_offset;
} BvhBuildContext;

void bvh_build_node(
  BvhBuildContext *ctx,
  Bvh *bvh,
  BvhBuildNode const *node,
  u32 self
) {
  u32 is_leaf = node->prims != NULL;

  if (is_leaf) {
    u32 offset = (u32)ctx->prim_offset;
    u32 count = node->count;

    bvh->offset[self] = offset;
    bvh->meta[self] = (u8)count;

    memcpy(bvh->prims + offset, node->prims, count * sizeof(u64));

    ctx->prim_offset += count;
  } else {
    u32 offset = (u32)ctx->node_offset;
    u32 count = node->count;

    bvh->offset[self] = offset;
    bvh->meta[self] = 0x80 | ((u8)count);

    memcpy(bvh->bounds + offset, node->bounds, count * sizeof(Aabb));

    ctx->node_offset += count;

    for (u32 i = 0; i < count; i++) {
      bvh_build_node(ctx, bvh, node->children[i], offset + i);
    }
  }
}

u64 bvh_node_and_prim_count(BvhBuildNode const *node, u64 *prim_count) {
  u32 is_leaf = node->prims != NULL;

  if (is_leaf) {
    *prim_count += node->count;
    return 1;
  }

  u64 count = 1;
  for (u32 i = 0; i < node->count; i++) {
    count += bvh_node_and_prim_count(node->children[i], prim_count);
  }

  return count;
}

void bvh_build_from_embree_bvh(EmbreeBvh const *embree_bvh, Bvh *bvh) {
  u64 prim_count = 0;
  u64 node_count = bvh_node_and_prim_count(embree_bvh->root, &prim_count);

  *bvh = (Bvh){
    .node_count = node_count,
    .offset = malloc(node_count * sizeof(u32)),
    .meta = malloc(node_count * sizeof(u8)),
    .bounds = malloc(node_count * sizeof(Aabb)),
    .prim_count = prim_count,
    .prims = malloc(prim_count * sizeof(u64)),
  };

  BvhBuildContext ctx = {
    .node_offset = 1,
    .prim_offset = 0,
  };

  bvh_build_node(&ctx, bvh, embree_bvh->root, 0);

  SDL_assert_always(ctx.node_offset == node_count);
  SDL_assert_always(ctx.prim_offset == prim_count);
}

void bvh_intersect(
  Bvh const *bvh,
  Ray const *ray,
  Triangle const *triangles,
  HitRecord *record
) {
  u32 stack[64];
  u32 top = 0;

  {
    u32 offset = bvh->offset[0];
    u32 count = (bvh->meta[0] & 0x7f);
    for (u32 i = 0; i < count; i++) {
      stack[top++] = offset + i;
    }
  }

  HitRecord rec = {
    .t = F32_NO_HIT,
  };

  vec3s reciprocal_ray_dir = vec3_reciprocal(ray->dir);

  while (top) {
    u32 index = stack[--top];

    bool is_leaf = (bvh->meta[index] & 0x80) == 0;

    u32 count = bvh->meta[index] & 0x7f;
    u32 offset = bvh->offset[index];

    if (is_leaf) {
      for (u32 i = 0; i < count; i++) {
        u64 prim_idx = bvh->prims[offset + i];
        Triangle const *tri = &triangles[prim_idx];

        TriangleHit hit;
        bool has_hit = ray_triangle_intersect(ray, tri, &hit);
        if (!has_hit) {
          continue;
        }

        if (hit.t < rec.t) {
          rec = (HitRecord){
            .t = hit.t,
            .u = hit.u,
            .v = hit.v,
            .idx = prim_idx,
          };
        }
      }
    } else {
      for (u32 i = 0; i < count; i++) {
        Aabb *bound = &bvh->bounds[offset + i];

        f32 t = aabb_intersect(bound, ray->origin, reciprocal_ray_dir);
        if (t == F32_NO_HIT) {
          continue;
        }

        stack[top++] = offset + i;
      }
    }
  }

  *record = rec;
}
