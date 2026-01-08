#ifndef AABB_H_INCLUDED
#define AABB_H_INCLUDED

#include "kingfisher_core.h"

typedef struct Aabb {
  vec3s lo, hi;
} Aabb;

#define AABB_EMPTY ((Aabb){.lo={FLT_MAX,FLT_MAX,FLT_MAX},.hi={-FLT_MAX,-FLT_MAX,-FLT_MAX}})

f32 aabb_intersect(Aabb const *bounds, vec3s ray_origin, vec3s reciprocal_ray_dir);

#endif // AABB_H_INCLUDED
