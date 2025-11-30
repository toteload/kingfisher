#include "kingfisher.h"

#include <stdio.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

typedef struct Spd_8 {
  u8 wavelengths[8];
  f32 powers[8];
} Spd_8;

typedef struct HitRecord {
  f32 t;
  vec3 n;
  u32 idx;
} HitRecord;

enum MaterialKind {
  MATERIAL_EMISSIVE,
  MATERIAL_DIFFUSE,
};

typedef struct Material {
  u8 kind;
  union {
    // At the moment an emissive material only emits a single wavelength
    struct {
      f32 power;
      u8 wavelength;
    } emissive;
  };
} Material;

typedef struct Scene {
  i32 sphere_count;
  Sphere const *spheres;
  Material const *materials;
} Scene;

void trace_scene(Ray const *ray, Scene const *scene, HitRecord *hit) {
  f32 t = INFINITY;
  i32 hit_idx = 0;

  for (i32 i = 0; i < scene->sphere_count; i++) {
    f32 st = ray_sphere_intersect_distance(ray, &scene->spheres[i]);
    if (st < t) {
      t = st;
      hit_idx = i;
    }
  }

  vec3 p = vec3_add(ray->origin, vec3_mul(ray->dir, (vec3){ t, t, t, }));
  vec3 n = vec3_normalized(vec3_sub(p, scene->spheres[hit_idx].origin));

  *hit = (HitRecord){
    .t = t,
    .n = n,
    .idx = hit_idx,
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

  Sphere spheres[] = {
    { .origin = {  2.0f,  0.5f,  0.0f, }, .radius = 1.0f, },
    { .origin = { -2.0f,  0.0f,  0.0f, }, .radius = 1.0f, },
    { .origin = {  0.0f, -0.5f,  2.0f, }, .radius = 1.0f, },
    { .origin = {  0.0f,  0.0f, -2.0f, }, .radius = 1.0f, },
    { .origin = {  0.0f, -2.0f,  0.0f, }, .radius = 0.1f, },
  };

  Material materials[] = {
    { MATERIAL_DIFFUSE, },
    { MATERIAL_DIFFUSE, },
    { MATERIAL_DIFFUSE, },
    { MATERIAL_DIFFUSE, },
    { MATERIAL_EMISSIVE, { 20.0f, 110, } },
  };

  Scene scene = {
    .sphere_count = 5,
    .spheres = spheres,
    .materials = materials,
  };

  f32 *powers = malloc(width * height * sizeof(f32));
  u8 *wavelengths = malloc(width * height);

  // This can function as an accumulator
  f32 *xyz = malloc(width * height * 3 * sizeof(f32));

  u64 last_time_ns = SDL_GetTicksNS();

  f32 time = 0.0f;

  Rng rng;
  Rng_seed(&rng, 13687844445);

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

    CameraOrtho options = {
      .origin = pos,
      .dir = dir,
      .du = du,
      .dv = dv,
      .width = width / 80.0f,
      .height = height / 80.0f,
      .near = 1.0e-3f,
      .far = 1.0e5f,
    };

    CameraPinhole pinhole = {
      .origin = pos,
      .dir = dir,
      .du = du,
      .dv = dv,
      .fov_radians = 0.5f * PI,
      .inv_aspect_ratio = ((f32)height) / width,
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
        //generate_primary_ray_ortho(&options, &ray, u, v);
        generate_primary_ray_pinhole(&pinhole, &ray, u, v);

        HitRecord hit;
        trace_scene(&ray, &scene, &hit);

        if (hit.t == F32_NO_HIT) {
          powers[i] = 0.0f;
          continue;
        }

        Material mat = scene.materials[hit.idx];

        if (mat.kind == MATERIAL_EMISSIVE) {
          powers[i] = mat.emissive.power;
          wavelengths[i] = mat.emissive.wavelength;
          continue;
        }

        // The sphere at index 4 is hardcoded emissive for now
        u32 light_idx = 4;

        Ray light_ray;
        {
          vec3 p = vec3_add(ray.origin, vec3_smul(hit.t, ray.dir));

          vec3 sample = sample_unit_sphere(Rng_f32(&rng), Rng_f32(&rng));

          vec3 lp = vec3_add(scene.spheres[light_idx].origin, vec3_smul(scene.spheres[light_idx].radius, sample));

          vec3 origin = vec3_add(p, vec3_smul(0.001f, hit.n)); 

          light_ray = (Ray){
            .origin = origin,
            .dir = vec3_normalized(vec3_sub(lp, origin)),
            .min_t = 1.0e-3f,
            .max_t = 1.0e5f,
          };
        }

        HitRecord light_hit;
        trace_scene(&light_ray, &scene, &light_hit);

        if (light_hit.idx != light_idx) {
          continue;
        }

        powers[i] = scene.materials[light_idx].emissive.power * clamp(0.0f, 1.0f, vec3_dot(hit.n, light_ray.dir));
        wavelengths[i] = scene.materials[light_idx].emissive.wavelength;
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
