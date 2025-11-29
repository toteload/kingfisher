#include <stdint.h>
#include <math.h>
#include <stdio.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef  int32_t i32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

extern float const cie_xyz_x[256];
extern float const cie_xyz_y[256];
extern float const cie_xyz_z[256];

#define PI 3.14159265358979323846f

// Spectral samples are stored as an 8-bit int wavelength and a 32-bit float power.
// To get the true wavelength in nm multiply by 2 and add 360.
// A stored wavelength of 140 represents a wavelength of 2 * 140 + 320 = 600 nm.

typedef struct vec3 {
  f32 x;
  f32 y;
  f32 z;
} vec3;

typedef struct HitRecord {
  f32 t;
  vec3 n;
} HitRecord;

typedef struct Ray {
  vec3 origin;
  f32 min_t;
  vec3 dir;
  f32 max_t;
} Ray;

typedef struct Sphere {
  vec3 origin;
  f32 radius;
} Sphere;

#define F32_NO_HIT INFINITY

f32 vec3_dot(vec3 a, vec3 b);

vec3 vec3_sub(vec3 a, vec3 b) {
  return (vec3){ a.x - b.x, a.y - b.y, a.z - b.z, };
}

vec3 vec3_add(vec3 a, vec3 b) {
  return (vec3){ a.x + b.x, a.y + b.y, a.z + b.z, };
}

vec3 vec3_mul(vec3 a, vec3 b) {
  return (vec3){ a.x * b.x, a.y * b.y, a.z * b.z, };
}

vec3 vec3_smul(f32 s, vec3 a) {
  return (vec3){ s * a.x, s * a.y, s * a.z, };
}

f32 vec3_magnitude(vec3 a) {
  return sqrtf(vec3_dot(a, a));
}

vec3 vec3_normalized(vec3 a) {
  f32 m = 1.0f / vec3_magnitude(a);
  return vec3_mul(a, (vec3){ m, m, m, });
}

f32 vec3_dot(vec3 a, vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3 vec3_cross(vec3 a, vec3 b) {
  return (vec3){
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x,
  };
}

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

void generate_primary_ray_ortho(OrthoOptions const *options, Ray *ray, f32 u, f32 v) {
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

f32 ray_sphere_intersect_distance(Ray const *ray, Sphere const *sphere) {
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

#define max(a,b) (((a) < (b)) ? (b) : (a))
#define min(a,b) (((a) > (b)) ? (b) : (a))

inline f32 clampf32(f32 lo, f32 hi, f32 t) {
  return min(max(lo, t), hi);
}

inline void normalized_xyz_to_linear_rgb(f32 const *xyz, f32 *rgb) {
  rgb[0] = clampf32(0.0f, 1.0f,  3.2406255f * xyz[0] - 1.5372080f * xyz[1] - 0.4986286f * xyz[2]);
  rgb[1] = clampf32(0.0f, 1.0f, -0.9689307f * xyz[0] + 1.8757561f * xyz[1] + 0.0415175f * xyz[2]);
  rgb[2] = clampf32(0.0f, 1.0f,  0.0557101f * xyz[0] - 0.2040211f * xyz[1] + 1.0569959f * xyz[2]);
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

typedef struct Scene {
  i32 sphere_count;
  Sphere *spheres;
} Scene;

void trace_scene(Ray const *ray, Scene const *scene, HitRecord *hit) {
  f32 t = INFINITY;
  i32 closest_sphere_idx = 0;

  for (i32 i = 0; i < scene->sphere_count; i++) {
    f32 st = ray_sphere_intersect_distance(ray, &scene->spheres[i]);
    if (st < t) {
      t = st;
      closest_sphere_idx = i;
    }
  }

  vec3 p = vec3_add(ray->origin, vec3_mul(ray->dir, (vec3){ t, t, t, }));
  vec3 n = vec3_normalized(vec3_sub(p, scene->spheres[closest_sphere_idx].origin));

  *hit = (HitRecord){
    .t = t,
    .n = n,
  };
}

int main(int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    return -1;
  }

  int width = 640;
  int height = 480;

  SDL_Window *window;
  SDL_Renderer *renderer;
  if (!SDL_CreateWindowAndRenderer("Kingfisher", width, height, 0, &window, &renderer)) {
    return 1;
  }

  SDL_Texture *screen = SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_XBGR8888,
    SDL_TEXTUREACCESS_STREAMING,
    width,
    height
  );

  Scene scene = {
    .sphere_count = 4,
    .spheres = malloc(4 * sizeof(Sphere)),
  };

  scene.spheres[0] = (Sphere){ .origin = {  2.0f,  0.5f,  0.0f, }, .radius = 1.0f, };
  scene.spheres[1] = (Sphere){ .origin = { -2.0f,  0.0f,  0.0f, }, .radius = 1.0f, };
  scene.spheres[2] = (Sphere){ .origin = {  0.0f, -0.5f,  2.0f, }, .radius = 1.0f, };
  scene.spheres[3] = (Sphere){ .origin = {  0.0f,  0.0f, -2.0f, }, .radius = 1.0f, };

  f32 *powers = malloc(width * height * sizeof(f32));
  u8 *wavelengths = malloc(width * height);

  // This can function as an accumulator
  f32 *xyz = malloc(width * height * 3 * sizeof(f32));

  u64 last_time_ns = SDL_GetTicksNS();

  f32 time = 0.0f;

  u32 done = 0;
  while (!done) {
    u64 time_ns = SDL_GetTicksNS();
    u64 dt_ns = time_ns - last_time_ns;
    f32 dt = ((f32)dt_ns) / 1.0e9f;
    last_time_ns = time_ns;

    time += dt;

    printf("%f\n", 1.0f / dt);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        goto exit;
      }

      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
        goto exit;
      }
    }

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(renderer);

    memset(powers, 0, width * height * sizeof(f32));
    memset(wavelengths, 0, width * height);
    memset(xyz, 0, width * height * 3 * sizeof(f32));

    vec3 pos = {
      5.0f * cosf(0.2f * time * PI),
      -5.0f,
      5.0f * sinf(0.2f * time * PI),
    };

    vec3 poi = { 0.0f, 0.0f, 0.0f, };
    vec3 dir = vec3_normalized(vec3_sub(poi, pos));

    vec3 up = { 0.0f, 1.0f, 0.0f, };
    vec3 du = vec3_normalized(vec3_cross(dir, up));
    vec3 dv = vec3_normalized(vec3_cross(du, dir));

    OrthoOptions options = {
      .origin = pos,
      .dir = dir,
      .du = du,
      .dv = dv,
      .width = width / 80.0f,
      .height = height / 80.0f,
      .near = 1.0e-3f,
      .far = 1.0e5f,
    };

    // Trace our rays and save the power and wavelengths
    for (i32 y = 0; y < height; y++) {
      for (i32 x = 0; x < width; x++) {
        i32 i = y * width + x;

        // In range [-1, 1]
        f32 u = 2.0f * ((f32)x) / width - 1.0f;
        f32 v = 2.0f * ((f32)y) / height - 1.0f;

        Ray ray;
        generate_primary_ray_ortho(&options, &ray, u, v);

        HitRecord hit;
        trace_scene(&ray, &scene, &hit);

        if (hit.t == F32_NO_HIT) {
          powers[i] = 0.0f;
          continue;
        }

        vec3 l = { 0.0f, -1.0f, 0.0f, };

        powers[i] = 12.0f * clampf32(0.0f, 1.0f, vec3_dot(hit.n, l));
        wavelengths[i] = 100;
      }
    }

    // Convert the wavelength+power samples to XYZ and store.
    for (i32 y = 0; y < height; y++) {
      for (i32 x = 0; x < width; x++) {
        i32 i = y * width + x;

        f32 s[3];
        spectral_to_xyz(wavelengths[i], s);

        for (i32 j = 0; j < 3; j++) {
          xyz[i * 3 + j] += powers[i] * s[j];
        }
      }
    }

    void *buffer;
    int pitch;
    SDL_LockTexture(screen, NULL, &buffer, &pitch);
    u8 *pixels = buffer;

    for (i32 y = 0; y < height; y++) {
      for (i32 x = 0; x < width; x++) {
        i32 i = y * width + x;

        f32 nxyz[3];
        normalize_xyz(xyz + i * 3, nxyz);

        f32 rgb[3];
        normalized_xyz_to_linear_rgb(nxyz, rgb);

        f32 srgb[3];
        linear_rgb_to_srgb(rgb, srgb);

        pixels[y * pitch + x * 4 + 0] = (u8)(srgb[0] * 255.0f);
        pixels[y * pitch + x * 4 + 1] = (u8)(srgb[1] * 255.0f);
        pixels[y * pitch + x * 4 + 2] = (u8)(srgb[2] * 255.0f);
      }
    }

    SDL_UnlockTexture(screen);
    SDL_RenderTexture(renderer, screen, NULL, NULL);

    SDL_RenderPresent(renderer);
  }

exit:
  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  SDL_Quit();

  return 0;
}
