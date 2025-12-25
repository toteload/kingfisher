#ifndef KINGFISHER_H_INCLUDED
#define KINGFISHER_H_INCLUDED

#include <stdint.h>
#include <math.h>
#include <stdlib.h> // _rotl

// Basic types
// -

typedef uint8_t u8;
typedef uint32_t u32;
typedef  int32_t i32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

// Common macros
// -

#define PI 3.14159265358979323846f

#ifndef max
// Windows is being annoying. It defines max and min in stdlib.h and there is no flag you can give
// to prevent it from being defined. And it only defines it you are using C not C++...
#define max(a,b) (((a) < (b)) ? (b) : (a))
#endif

#ifndef min
#define min(a,b) (((a) > (b)) ? (b) : (a))
#endif

#define clamp(lo,hi,t) min(max(lo, t), hi)

// Vectors
// -

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

// Sampling
// -

inline vec3 sample_unit_sphere(f32 u1, f32 u2) {
  // Code from PBRT `UniformSampleSphere` page 664
  f32 z = 1.0f - 2.0f * u1;
  f32 r = sqrtf(max(0.0f, 1.0f - z * z));
  f32 phi = 2.0f * PI * u2;
  f32 x = r * cosf(phi);
  f32 y = r * sinf(phi);
  return (vec3){ x, y, z, };
}

// Ray
// -

typedef struct Ray {
  vec3 origin;
  f32 min_t;
  vec3 dir;
  f32 max_t;
} Ray;

// Camera
// -

typedef struct CameraOrtho {
  vec3 origin;
  vec3 du;
  vec3 dv;
  vec3 dir;
  f32 width;
  f32 height;
  f32 near;
  f32 far;
} CameraOrtho;

inline void generate_primary_ray_ortho(CameraOrtho const *options, Ray *ray, f32 u, f32 v) {
  vec3 offset = vec3_add(
    vec3_smul(0.5f * u * options->width, options->du),
    vec3_smul(0.5f * v * options->height, options->dv)
  );

  *ray = (Ray){
    .origin = vec3_add(options->origin, offset),
    .dir = options->dir,
    .min_t = options->near,
    .max_t = options->far,
  };
}

typedef struct CameraPinhole {
  vec3 origin;
  vec3 du;
  vec3 dv;
  vec3 dir;
  f32 fov_radians; // Horizontal field of view
  f32 inv_aspect_ratio; // height / width
  f32 near;
  f32 far;
} CameraPinhole;

inline void generate_primary_ray_pinhole(CameraPinhole const *opt, Ray *ray, f32 u, f32 v) {
  f32 a = tanf(0.5f * opt->fov_radians);
  vec3 dir = vec3_normalized(
    vec3_add(
      opt->dir,
      vec3_add(
        vec3_smul(a * u, opt->du),
        vec3_smul(opt->inv_aspect_ratio * a * v, opt->dv))));

  *ray = (Ray){
    .origin = opt->origin,
    .dir = dir,
    .min_t = opt->near,
    .max_t = opt->far,
  };
}

// Ray intersection
// -

#define F32_NO_HIT INFINITY

typedef struct Sphere {
  vec3 origin;
  f32 radius;
} Sphere;

typedef struct Triangle {
  vec3 v0;
  vec3 v1;
  vec3 v2;
} Triangle;

inline f32 ray_sphere_intersect_distance(Ray const *ray, Sphere const *sphere) {
  vec3 m = vec3_sub(ray->origin, sphere->origin);
  f32 b = vec3_dot(m, ray->dir);
  f32 c = vec3_dot(m, m) - sphere->radius * sphere->radius;

  if (c > 0.0f && b > 0.0f) {
    return F32_NO_HIT;
  }

  f32 d = b * b - c;

  if (d < 0.0f) {
    return F32_NO_HIT;
  }

  f32 ds = sqrtf(d);

  f32 t0 = -b - ds;
  f32 t1 = -b + ds;

  f32 t_min;
  f32 t_max;

  if (t0 < t1) {
    t_min = t0;
    t_max = t1;
  } else {
    t_min = t1;
    t_max = t0;
  }

  // t_max is negative so both t are behind us, thus no intersection.
  if (t_max < 0.0f) {
    return F32_NO_HIT;
  }

  if (t_min < 0.0f) {
    return t_max;
  }

  return t_min;
}

// Möller-Trumbore ray-triangle intersection algorithm
inline f32 ray_triangle_intersect_distance(Ray const *ray, Triangle const *tri) {
  const f32 EPSILON = 0.0000001f;

  // Compute edges from v0
  vec3 edge1 = vec3_sub(tri->v1, tri->v0);
  vec3 edge2 = vec3_sub(tri->v2, tri->v0);

  // Begin calculating determinant - also used to calculate u parameter
  vec3 h = vec3_cross(ray->dir, edge2);
  f32 a = vec3_dot(edge1, h);

  // Ray is parallel to triangle
  if (fabs(a) < EPSILON) {
    return F32_NO_HIT;
  }

  f32 f = 1.0f / a;
  vec3 s = vec3_sub(ray->origin, tri->v0);
  f32 u = f * vec3_dot(s, h);

  // Intersection is outside triangle
  if (u < 0.0f || u > 1.0f) {
    return F32_NO_HIT;
  }

  vec3 q = vec3_cross(s, edge1);
  f32 v = f * vec3_dot(ray->dir, q);

  // Intersection is outside triangle
  if (v < 0.0f || u + v > 1.0f) {
    return F32_NO_HIT;
  }

  // Compute t to find intersection point on ray
  f32 t = f * vec3_dot(edge2, q);

  // Check if intersection is within ray bounds
  if (t >= ray->min_t && t <= ray->max_t) {
    return t;
  }

  return F32_NO_HIT;
}

// Color space
// -

extern float const cie_xyz_x[256];
extern float const cie_xyz_y[256];
extern float const cie_xyz_z[256];

// Spectral samples are stored as an 8-bit int wavelength and a 32-bit float power.
// To get the true wavelength in nm multiply by 2 and add 360.
// A stored wavelength of 140 represents a wavelength of 2 * 140 + 360 = 640 nm.

inline vec3 spectral_to_xyz(u8 wavelength) {
  return (vec3){ cie_xyz_x[wavelength], cie_xyz_y[wavelength], cie_xyz_z[wavelength], };
}

// Normalize XYZ based on a reference black point and white point
inline vec3 normalize_xyz(vec3 xyz)
{
  f32 black[3] = { 0.1901f, 0.2f, 0.2178f, };
  f32 white[3] = { 76.04f, 80.0f, 87.12f, };

  return (vec3){
    (xyz.e[0] - black[0]) / (white[0] - black[0]) * (white[0] / white[1]),
    (xyz.e[1] - black[1]) / (white[1] - black[1]),
    (xyz.e[2] - black[2]) / (white[2] - black[2]) * (white[2] / white[1]),
  };
}

inline vec3 normalized_xyz_to_linear_rgb(vec3 nxyz) {
  return (vec3){
    clamp(0.0f, 1.0f,  3.2406255f * nxyz.x - 1.5372080f * nxyz.y - 0.4986286f * nxyz.z),
    clamp(0.0f, 1.0f, -0.9689307f * nxyz.x + 1.8757561f * nxyz.y + 0.0415175f * nxyz.z),
    clamp(0.0f, 1.0f,  0.0557101f * nxyz.x - 0.2040211f * nxyz.y + 1.0569959f * nxyz.z),
  };
}

inline vec3 linear_rgb_to_srgb(vec3 rgb) {
  vec3 srgb;
  for (i32 i = 0; i < 3; i++) {
    if (rgb.e[i] <= 0.0031308f) {
      srgb.e[i] = 12.92f * rgb.e[i];
    } else {
      srgb.e[i] = 1.055f * powf(rgb.e[i], 1.0f / 2.4f) - 0.055f;
    }
  }
  return srgb;
}

#endif // KINGFISHER_H_INCLUDED
