#ifndef VEC3_H_INCLUDED
#define VEC3_H_INCLUDED

#include "kingfisher_core.h"

typedef union vec3 {
  struct { f32 x, y, z; };
  struct { f32 r, g, b; };
  f32 e[3];
} vec3;

f32 vec3_dot(vec3 a, vec3 b);

inline vec3 vec3_sub(vec3 a, vec3 b) {
  return (vec3){ a.x - b.x, a.y - b.y, a.z - b.z, };
}

inline vec3 vec3_add(vec3 a, vec3 b) {
  return (vec3){ a.x + b.x, a.y + b.y, a.z + b.z, };
}

inline vec3 vec3_mul(vec3 a, vec3 b) {
  return (vec3){ a.x * b.x, a.y * b.y, a.z * b.z, };
}

inline vec3 vec3_smul(f32 s, vec3 a) {
  return (vec3){ s * a.x, s * a.y, s * a.z, };
}

inline vec3 vec3_max(vec3 a, vec3 b) {
  return (vec3){
    max(a.x, b.x),
    max(a.y, b.y),
    max(a.z, b.z),
  };
}

inline vec3 vec3_min(vec3 a, vec3 b) {
  return (vec3){
    min(a.x, b.x),
    min(a.y, b.y),
    min(a.z, b.z),
  };
}

inline f32 vec3_magnitude(vec3 a) {
  return sqrtf(vec3_dot(a, a));
}

inline vec3 vec3_normalized(vec3 a) {
  f32 m = 1.0f / vec3_magnitude(a);
  return vec3_mul(a, (vec3){ m, m, m, });
}

inline f32 vec3_dot(vec3 a, vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline vec3 vec3_cross(vec3 a, vec3 b) {
  return (vec3){
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x,
  };
}

inline vec3 pitch_yaw_to_vec3(f32 pitch, f32 yaw) {
  f32 cos_pitch = cosf(pitch);

  return (vec3){
    cosf(yaw) * cos_pitch,
    sinf(pitch),
    sinf(yaw) * cos_pitch,
  };
}

// Assumes that up is (0, 1, 0)
// `n` must be normalized.
inline void vec3_to_pitch_yaw(vec3 n, f32 *pitch, f32 *yaw) {
  *pitch = asinf(n.y);
  *yaw = atan2f(n.z, n.x);
}

#endif // VEC3_H_INCLUDED
