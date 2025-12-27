#include "aabb.h"

f32 aabb_intersect(
  Aabb const *bounds,
  vec3 ray_origin,
  vec3 reciprocal_ray_dir
) {
  f32 t0 = -FLT_MAX;
  f32 t1 = FLT_MAX;

  for (u32 i = 0; i < 3; i++) {
    f32 t_near = (bounds->lo.e[i] - ray_origin.e[i]) * reciprocal_ray_dir.e[i];
    f32 t_far = (bounds->hi.e[i] - ray_origin.e[i]) * reciprocal_ray_dir.e[i];

    if (t_near > t_far) {
      Swap(f32, t_near, t_far);
    }

    t0 = max(t0, t_near);
    t1 = min(t1, t_far);

    if (t0 > t1) {
      return F32_NO_HIT;
    }
  }

  if (t0 < 0.0f) {
    if (t1 < 0.0f) {
      return F32_NO_HIT;
    } else {
      return t1;
    }
  }

  return t0;
}

