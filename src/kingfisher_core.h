#ifndef KINGFISHER_H_INCLUDED
#define KINGFISHER_H_INCLUDED

#include <stdint.h>
#include <math.h>
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

// Common macros
// -

#define PI 3.14159265358979323846f
#define TWO_PI 6.28318530717958647692f

#ifndef max
// Windows is being annoying. It defines max and min in stdlib.h and there is no flag you can give
// to prevent it from being defined. And it only defines it you are using C not C++...
#define max(a,b) (((a) < (b)) ? (b) : (a))
#endif

#ifndef min
#define min(a,b) (((a) > (b)) ? (b) : (a))
#endif

#define clamp(lo,hi,t) min(max(lo, t), hi)

typedef struct Ray {
  vec3 origin;
  f32 min_t;
  vec3 dir;
  f32 max_t;
} Ray;

typedef struct Triangle {
  vec3 v0;
  vec3 v1;
  vec3 v2;
} Triangle;

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
