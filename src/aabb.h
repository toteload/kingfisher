#ifndef AABB_H_INCLUDED
#define AABB_H_INCLUDED

#include "kingfisher_core.h"
#include "vec3.h"

typedef struct Aabb {
  vec3 lo, hi;
} Aabb;

#define AABB_EMPTY ((Aabb){.lo={INFINITY,INFINITY,INFINITY},.hi={-INFINITY,-INFINITY,-INFINITY}})

#endif // AABB_H_INCLUDED
