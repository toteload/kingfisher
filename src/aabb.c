#include "toteload.h"
#include "aabb.h"

f32 aabb_intersect(
  Aabb const *bounds,
  vec3s ray_origin,
  vec3s reciprocal_ray_dir
) {
  f32 t0 = -FLT_MAX;
  f32 t1 = FLT_MAX;

  for (u32 i = 0; i < 3; i++) {
    f32 t_near = (bounds->lo.raw[i] - ray_origin.raw[i]) * reciprocal_ray_dir.raw[i];
    f32 t_far = (bounds->hi.raw[i] - ray_origin.raw[i]) * reciprocal_ray_dir.raw[i];

    if (t_near > t_far) {
      Swap(f32, t_near, t_far);
    }

    t0 = Max(t0, t_near);
    t1 = Min(t1, t_far);

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

