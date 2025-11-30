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

typedef struct vec3 {
  f32 x;
  f32 y;
  f32 z;
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

typedef struct OrthoOptions {
  vec3 origin;
  vec3 du;
  vec3 dv;
  vec3 dir;
  f32 width;
  f32 height;
  f32 near;
  f32 far;
} OrthoOptions;

inline void generate_primary_ray_ortho(OrthoOptions const *options, Ray *ray, f32 u, f32 v) {
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

// Ray intersection
// -

#define F32_NO_HIT INFINITY

typedef struct Sphere {
  vec3 origin;
  f32 radius;
} Sphere;

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

// Color space
// -

extern float const cie_xyz_x[256];
extern float const cie_xyz_y[256];
extern float const cie_xyz_z[256];

// Spectral samples are stored as an 8-bit int wavelength and a 32-bit float power.
// To get the true wavelength in nm multiply by 2 and add 360.
// A stored wavelength of 140 represents a wavelength of 2 * 140 + 360 = 640 nm.

inline void spectral_to_xyz(u8 wavelength, f32 *xyz) {
  xyz[0] = cie_xyz_x[wavelength];
  xyz[1] = cie_xyz_y[wavelength];
  xyz[2] = cie_xyz_z[wavelength];
}

// Normalize XYZ based on a reference black point and white point
inline void normalize_xyz(f32 const *xyz, f32 *nxyz)
{
  f32 black[3] = { 0.1901f, 0.2f, 0.2178f, };
  f32 white[3] = { 76.04f, 80.0f, 87.12f, };

  nxyz[0] = (xyz[0] - black[0]) / (white[0] - black[0]) * (white[0] / white[1]);
  nxyz[1] = (xyz[1] - black[1]) / (white[1] - black[1]);
  nxyz[2] = (xyz[2] - black[2]) / (white[2] - black[2]) * (white[2] / white[1]);
}

inline void normalized_xyz_to_linear_rgb(f32 const *xyz, f32 *rgb) {
  rgb[0] = clamp(0.0f, 1.0f,  3.2406255f * xyz[0] - 1.5372080f * xyz[1] - 0.4986286f * xyz[2]);
  rgb[1] = clamp(0.0f, 1.0f, -0.9689307f * xyz[0] + 1.8757561f * xyz[1] + 0.0415175f * xyz[2]);
  rgb[2] = clamp(0.0f, 1.0f,  0.0557101f * xyz[0] - 0.2040211f * xyz[1] + 1.0569959f * xyz[2]);
}

inline void linear_rgb_to_srgb(f32 const *rgb, f32 *srgb) {
  for (i32 i = 0; i < 3; i++) {
    if (rgb[i] <= 0.0031308f) {
      srgb[i] = 12.92f * rgb[i];
    } else {
      srgb[i] = 1.055f * powf(rgb[i], 1.0f / 2.4f) - 0.055f;
    }
  }
}

#endif // KINGFISHER_H_INCLUDED
