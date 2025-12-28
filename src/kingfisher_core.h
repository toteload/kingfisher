#ifndef KINGFISHER_H_INCLUDED
#define KINGFISHER_H_INCLUDED

#include <stdint.h>
#include <math.h>
#include <float.h>
#include <stdlib.h> // _rotl
#include <stdbool.h>

// Basic types
// -

typedef  int8_t i8;
typedef uint8_t u8;
typedef  int16_t i16;
typedef uint16_t u16;
typedef  int32_t i32;
typedef uint32_t u32;
typedef  int64_t i64;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

#include "vec3.h"

#define F32_NO_HIT FLT_MAX

typedef struct Ray {
  vec3 origin;
  f32 min_t;
  vec3 dir;
  f32 max_t;
} Ray;

typedef union Triangle {
  struct {
    vec3 v0;
    vec3 v1;
    vec3 v2;
  };
  vec3 p[3];
} Triangle;

typedef struct HitRecord {
  f32 t;
  f32 u, v;
  u64 idx;
} HitRecord;

typedef struct TriangleHit {
  f32 t, u, v;
} TriangleHit;

// Möller-Trumbore ray-triangle intersection algorithm
inline bool ray_triangle_intersect(
  Ray const *ray,
  Triangle const *tri,
  TriangleHit *hit
) {
  const f32 EPSILON = 0.0000001f;

  // Compute edges from v0
  vec3 edge1 = vec3_sub(tri->v1, tri->v0);
  vec3 edge2 = vec3_sub(tri->v2, tri->v0);

  // Begin calculating determinant - also used to calculate u parameter
  vec3 h = vec3_cross(ray->dir, edge2);
  f32 a = vec3_dot(edge1, h);

  // Ray is parallel to triangle
  if (fabs(a) < EPSILON) {
    *hit = (TriangleHit){ .t = F32_NO_HIT };
    return false;
  }

  f32 f = 1.0f / a;
  vec3 s = vec3_sub(ray->origin, tri->v0);
  f32 u = f * vec3_dot(s, h);

  // Intersection is outside triangle
  if (u < 0.0f || u > 1.0f) {
    *hit = (TriangleHit){ .t = F32_NO_HIT };
    return false;
  }

  vec3 q = vec3_cross(s, edge1);
  f32 v = f * vec3_dot(ray->dir, q);

  // Intersection is outside triangle
  if (v < 0.0f || u + v > 1.0f) {
    *hit = (TriangleHit){ .t = F32_NO_HIT };
    return false;
  }

  // Compute t to find intersection point on ray
  f32 t = f * vec3_dot(edge2, q);

  // Check if intersection is within ray bounds
  if (t < ray->min_t || t > ray->max_t) {
    *hit = (TriangleHit){ .t = F32_NO_HIT };
    return false;
  }

  *hit = (TriangleHit){
    .t = t,
    .u = u,
    .v = v,
  };

  return true;
}

// Common macros
// -

#define PI 3.14159265358979323846f
#define TWO_PI 6.28318530717958647692f

#define Swap(T,a,b) do { T tmp = (a); a = b; b = tmp; } while(0)

#ifndef max
// Windows is being annoying. It defines max and min in stdlib.h and there is no flag you can give
// to prevent it from being defined. And it only defines it you are using C not C++...
#define max(a,b) (((a) < (b)) ? (b) : (a))
#endif

#ifndef min
#define min(a,b) (((a) > (b)) ? (b) : (a))
#endif

#define clamp(lo,hi,t) min(max(lo, t), hi)

inline u32 clz32(u32 x) {
  unsigned long idx;
  u8 res = _BitScanReverse(&idx, x);
  return (res) ? (31 - idx) : 32;
}

inline u32 round_up_to_nearest_power_of_two32(u32 x) {
  return 1 << (32 - clz32(x));
}

// Random
// -

typedef struct Rng {
  u32 s[4];
} Rng;

inline u64 splitmix64_next(u64 x) {
  u64 z = (x += 0x9e3779b97f4a7c15);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
	return z ^ (z >> 31);
}

inline void Rng_seed(Rng *rng, u64 seed) {
  u64 a = splitmix64_next(seed);
  u64 b = splitmix64_next(a);

  rng->s[0] = (u32)(a >> 32);
  rng->s[1] = (u32)a;
  rng->s[2] = (u32)(b >> 32);
  rng->s[3] = (u32)b;
}

inline u32 Rng_u32(Rng *rng) {
  u32 result = rng->s[0] + rng->s[3];
  u32 t = rng->s[1] << 9;

	rng->s[2] ^= rng->s[0];
	rng->s[3] ^= rng->s[1];
	rng->s[1] ^= rng->s[2];
	rng->s[0] ^= rng->s[3];

	rng->s[2] ^= t;

	rng->s[3] = _rotl(rng->s[3], 11);

	return result;
}

// Returns a f32 in the range [0, 1)
inline f32 Rng_f32(Rng *rng) {
  union { u32 u; f32 f; } z = { ((u32)0xff) << 22 | Rng_u32(rng) >> 9 };
  return z.f - 1.0f;
}

#endif // KINGFISHER_H_INCLUDED
