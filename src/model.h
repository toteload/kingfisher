#ifndef MODEL_H
#define MODEL_H

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstatic-in-inline"
#include <cglm/struct.h>
#pragma clang diagnostic pop

#include "toteload.h"

typedef union {
  vec3s p[3];
  struct {
    vec3s v0, v1, v2;
  };
} Triangle;

bool read_obj_triangles(char const *filename, Triangle **triangles, u64 *triangle_count);
bool read_fbx_triangles(char const *filename, Triangle **triangles, u64 *triangle_count);

#endif // MODEL_H
