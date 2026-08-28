#ifndef KINGFISHER_H_INCLUDED
#define KINGFISHER_H_INCLUDED

#include <stdint.h>
#include <math.h>
#include <float.h>
#include <stdlib.h> // _rotl
#include <stdbool.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstatic-in-inline"
#include <cglm/struct.h>
#pragma clang diagnostic pop

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

#define F32_NO_HIT FLT_MAX

typedef struct Ray {
  vec3s origin;
  f32 min_t;
  vec3s dir;
  f32 max_t;
} Ray;

typedef union Triangle {
  struct {
    vec3s v0;
    vec3s v1;
    vec3s v2;
  };
  vec3s p[3];
} Triangle;

inline vec3s triangle_normal(Triangle const *triangle) {
  vec3s e1 = glms_vec3_sub(triangle->v1, triangle->v0);
  vec3s e2 = glms_vec3_sub(triangle->v2, triangle->v0);
  return glms_vec3_normalize(glms_vec3_cross(e1, e2));
}

inline vec3s vec3_reciprocal(vec3s a) {
  return (vec3s){{ 1.0f / a.x, 1.0f / a.y, 1.0f / a.z, }}; 
}

inline vec3s pitch_yaw_to_vec3(f32 pitch, f32 yaw) {
  f32 cos_pitch = cosf(pitch);

  return (vec3s){{
    cosf(yaw) * cos_pitch,
    sinf(pitch),
    sinf(yaw) * cos_pitch,
  }};
}

// Assumes that up is (0, 1, 0)
// `n` must be normalized.
inline void vec3_to_pitch_yaw(vec3s n, f32 *pitch, f32 *yaw) {
  *pitch = asinf(n.y);
  *yaw = atan2f(n.z, n.x);
}

//inline mat3x3 triangle_basis(Triangle const *triangle) {
//  vec3 e01 = vec3_sub(triangle->v1, triangle->v0);
//  vec3 e02 = vec3_sub(triangle->v2, triangle->v0);
//
//  vec3 e0 = vec3_normalized(e01);
//  vec3 e1 = vec3_normalized(vec3_cross(e01, e02));
//  vec3 e2 = vec3_normalized(vec3_cross(e0, e1));
//
//  return (mat3x3){ e0, e1, e2, };
//}
//
//inline mat3x3 mat3x3_rotate_y(f32 radians) {
//  f32 c = cosf(radians);
//  f32 s = sinf(radians);
//  return (mat3x3){
//    .e = {
//      (vec3){ c, 0.0f, -s },
//      (vec3){ 0.0f, 1.0f, 0.0f },
//      (vec3){ s, 0.0f, c },
//    }
//  };
//}
//
//inline vec3 mat3x3_mul_vec3(mat3x3 mat, vec3 x) {
//  return (vec3){
//    vec3_dot((vec3){ mat.e[0].e[0], mat.e[1].e[0], mat.e[2].e[0], }, x),
//    vec3_dot((vec3){ mat.e[0].e[1], mat.e[1].e[1], mat.e[2].e[1], }, x),
//    vec3_dot((vec3){ mat.e[0].e[2], mat.e[1].e[2], mat.e[2].e[2], }, x),
//  };
//} 
//
//inline mat3x3 mat3x3_mul(mat3x3 a, mat3x3 b) {
//  return (mat3x3){
//    .e = {
//      mat3x3_mul_vec3(a, b.e[0]),
//      mat3x3_mul_vec3(a, b.e[1]),
//      mat3x3_mul_vec3(a, b.e[2]),
//    }
//  };
//}

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
  vec3s edge1 = glms_vec3_sub(tri->v1, tri->v0);
  vec3s edge2 = glms_vec3_sub(tri->v2, tri->v0);

  // Begin calculating determinant - also used to calculate u parameter
  vec3s h = glms_vec3_cross(ray->dir, edge2);
  f32 a = glms_vec3_dot(edge1, h);

  // Ray is parallel to triangle
  if (fabs(a) < EPSILON) {
    *hit = (TriangleHit){ .t = F32_NO_HIT };
    return false;
  }

  f32 f = 1.0f / a;
  vec3s s = glms_vec3_sub(ray->origin, tri->v0);
  f32 u = f * glms_vec3_dot(s, h);

  // Intersection is outside triangle
  if (u < 0.0f || u > 1.0f) {
    *hit = (TriangleHit){ .t = F32_NO_HIT };
    return false;
  }

  vec3s q = glms_vec3_cross(s, edge1);
  f32 v = f * glms_vec3_dot(ray->dir, q);

  // Intersection is outside triangle
  if (v < 0.0f || u + v > 1.0f) {
    *hit = (TriangleHit){ .t = F32_NO_HIT };
    return false;
  }

  // Compute t to find intersection point on ray
  f32 t = f * glms_vec3_dot(edge2, q);

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
  return x ? __builtin_clz(x) : 32;
}

inline u32 round_up_to_nearest_power_of_two32(u32 x) {
  return (u32)1 << (32 - clz32(x));
}

// Sampling
// -

inline vec3s sample_unit_sphere(f32 u1, f32 u2) {
  f32 theta = TWO_PI * u2;
  f32 phi = PI * u1;

  f32 r = sinf(phi);

  f32 x = r * cosf(theta);
  f32 y = cosf(phi);
  f32 z = r * sinf(theta);

  return (vec3s){{ x, y, z, }};
}

// The Y-axis is considered up and the hemisphere is centered around this axis.
inline vec3s sample_unit_hemisphere(f32 u1, f32 u2) {
#if 0
  f32 s = sqrtf(1.0f - u1 * u1);
  f32 phi = 2.0f * PI * u2;
  f32 x = s * sinf(phi);
  f32 z = s * cosf(phi);
  return (vec3){ x, u1, z, };
#else
  // According to the link below you can also get a cosine weighted direction around
  // a given normal `n` by doing `normalized(n + random_unit_direction())`
  // https://pema.dev/obsidian/math/light-transport/cosine-weighted-sampling.html
  //
  // Cosine weighted
  f32 theta = acosf(sqrtf(u1));
  f32 phi = TWO_PI * u2;

  f32 x = cosf(phi) * sinf(theta);
  f32 y = cosf(theta);
  f32 z = sinf(phi) * sinf(theta);

  return (vec3s){{ x, y, z, }};
#endif
}

// u1 and u2 must be in range [0, 1).
// Returns barycentric coordinates.
inline void sample_unit_triangle(f32 u1, f32 u2, f32 *u, f32 *v) {
  f32 t = sqrtf(u1);
  *u = 1.0f - t;
  *v = u2 * t;
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
  u32 x = Rng_u32(rng);
  return (x >> 8) * 0x1.0p-24f;
}

#endif // KINGFISHER_H_INCLUDED
