#include "kingfisher_core.h"
#include "camera.h"
#include "colorspace.h"
#include "bvh.h"

#include <stdio.h>
#include <assert.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#pragma warning(push)
#pragma warning(disable:4996)
#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>
#pragma warning(pop)

// Spectral power distribution
typedef struct Spd_8 {
  u8 wavelengths[8];
  f32 powers[8];
} Spd_8;

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

// u1 and u2 must be in range [0, 1).
// Returns barycentric coordinates.
inline void sample_unit_triangle(f32 u1, f32 u2, f32 *u, f32 *v) {
  f32 t = sqrtf(u1);
  *u = 1.0f - t;
  *v = u2 * t;
}

#if 0
#define IDX_NO_HIT UINT32_MAX

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
  i32 triangle_count;
  Triangle const *triangles;
  Material const *materials;
} Scene;

void trace_scene(Ray const *ray, Scene const *scene, HitRecord *hit) {
  f32 t = F32_NO_HIT;
  i32 hit_idx = IDX_NO_HIT;

  for (i32 i = 0; i < scene->triangle_count; i++) {
    f32 tt = ray_triangle_intersect_distance(ray, &scene->triangles[i]);
    if (tt < t) {
      t = tt;
      hit_idx = i;
    }
  }

  if (hit_idx == IDX_NO_HIT) {
    *hit = (HitRecord){
      .t = F32_NO_HIT,
      .idx = IDX_NO_HIT,
    };
    return;
  }

  vec3 edge1 = vec3_sub(scene->triangles[hit_idx].v1, scene->triangles[hit_idx].v0);
  vec3 edge2 = vec3_sub(scene->triangles[hit_idx].v2, scene->triangles[hit_idx].v0);
  vec3 n = vec3_normalized(vec3_cross(edge1, edge2));

  if (vec3_dot(n, ray->dir) > 0.0f) {
    n = vec3_smul(-1.0f, n);
  }

  *hit = (HitRecord){
    .t = t,
    .n = n,
    .idx = hit_idx,
  };
}
#endif

int main(int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    return 1;
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

  u32 bunny_triangle_count;
  Triangle *bunny_triangles;
  {
    fastObjMesh *mesh = fast_obj_read("data/stanford_bunny.obj");

    if (!mesh) {
      printf("Failed to load mesh\n");
      return 1;
    }

    assert(mesh->group_count == 1);

    fastObjGroup group = mesh->groups[0];

    Triangle *triangles = malloc(mesh->face_count * sizeof(Triangle));
    for (u32 i = 0; i < group.face_count; i++) {
      // Only support triangles
      assert(mesh->face_vertices[group.face_offset + i] == 3);

      for (u32 j = 0; j < 3; j++) {
        fastObjIndex idx = mesh->indices[group.index_offset + 3 * i + j];

        assert(idx.p != 0);

        triangles[i].p[j] = (vec3){
          mesh->positions[3 * idx.p + 0],
          mesh->positions[3 * idx.p + 1],
          mesh->positions[3 * idx.p + 2],
        };
      }
    }

    bunny_triangle_count = group.face_count;
    bunny_triangles = triangles;

    fast_obj_destroy(mesh);
  }

  EmbreeBvh build_bvh = embree_bvh_build(bunny_triangles, bunny_triangle_count);

  //Bvh bvh;
  //bvh_build_from_embree_bvh(&build_bvh, &bvh);

  // Accumulation buffer
  vec3 *xyz = malloc(width * height * sizeof(vec3));
  memset(xyz, 0, width * height * sizeof(vec3));

  u32 sample_count = 0;

  u64 last_time_ns = SDL_GetTicksNS();

  f32 time = 0.0f;

  Rng rng;
  Rng_seed(&rng, 13687844445);

  CameraControls camera;
  {
    vec3 position = { 5.0f, 5.0f, 5.0f };
    f32 pitch, yaw;
    vec3 dir = vec3_normalized(vec3_sub((vec3){0.0f, 0.0f, 0.0f}, position));
    vec3_to_pitch_yaw(dir, &pitch, &yaw);

    camera = (CameraControls){
      .position = position,
      .pitch = pitch,
      .yaw = yaw,
      .move_speed = 3.0f,
      .rot_speed = 2.0f,
    };
  }

  // Previous camera state for tracking movement
  CameraControls camera_prev = camera;

  u32 done = 0;
  while (!done) {
    u64 time_ns = SDL_GetTicksNS();
    u64 dt_ns = time_ns - last_time_ns;
    f32 dt = ((f32)dt_ns) / 1.0e9f;
    last_time_ns = time_ns;

    time += dt;

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

    // Update camera from keyboard input
    const bool *keys = SDL_GetKeyboardState(NULL);
    update_camera_from_input(&camera, keys, dt);

    // Check if camera has moved and reset accumulation if needed
    if (camera_has_moved(&camera, &camera_prev)) {
      memset(xyz, 0, width * height * sizeof(vec3));
      sample_count = 0;
    }

    camera_prev = camera;

    // Calculate camera basis from camera controls
    CameraBasis basis;
    camera_controls_to_basis(&camera, &basis);

    // Trace our rays and accumulate XYZ directly
    for (i32 y = 0; y < height; y++) {
      for (i32 x = 0; x < width; x++) {
        i32 i = y * width + x;

        // pixel dimensions in range [-1, 1]
        f32 pixel_width = 2.0f / width;
        f32 pixel_height = 2.0f / height;

        // Base pixel position (bottom-left corner of pixel)
        f32 base_u = 2.0f * ((f32)x) / width - 1.0f;
        f32 base_v = 2.0f * ((f32)y) / height - 1.0f;

        // Stratified jittered sampling: divide pixel into 2 x 2 grid
        u32 strat_size = 2;
        for (u32 sy = 0; sy < strat_size; sy++) {
          for (u32 sx = 0; sx < strat_size; sx++) {
            // Jittered position within this stratum
            f32 jitter_x = Rng_f32(&rng);  // Random in [0, 1)
            f32 jitter_y = Rng_f32(&rng);  // Random in [0, 1)

            // Compute jittered sample position in normalized device coordinates
            f32 u = base_u + pixel_width * (sx + jitter_x) / strat_size;
            f32 v = base_v + pixel_height * (sy + jitter_y) / strat_size;

            Ray ray;
            //{
            //  PerspectiveOrtho ortho = {
            //    .width = width / 80.0f,
            //    .height = height / 80.0f,
            //    .near = 1.0e-3f,
            //    .far = 1.0e5f,
            //  };
            //  generate_primary_ray_ortho(&basis, &ortho, &ray, u, v);
            //}
            {
              PerspectivePinhole pinhole = {
                .fov_radians = 0.5f * PI,
                .inv_aspect_ratio = ((f32)height) / width,
                .near = 1.0e-3f,
                .far = 1.0e5f,
              };

              generate_primary_ray_pinhole(&basis, &pinhole, &ray, u, v);
            }

            HitRecord rec;
            embree_bvh_intersect(&build_bvh, &ray, bunny_triangles, &rec);

            if (rec.t == F32_NO_HIT) {
              continue;
            }

            // Convert wavelength+power to XYZ and accumulate directly
            f32 power = 40.0f;
            u8 wavelength = 120;

            vec3 s = spectral_to_xyz(wavelength);
            xyz[i] = vec3_add(xyz[i], vec3_smul(power, s));
          }
        }

#if 0
        Material mat = scene.materials[hit.idx];

        if (mat.kind == MATERIAL_EMISSIVE) {
          powers[i] = mat.emissive.power;
          wavelengths[i] = mat.emissive.wavelength;
          continue;
        }

        // The triangle at index 0 is hardcoded emissive for now
        u32 light_idx = 0;

        Ray light_ray;
        {
          vec3 p = vec3_add(ray.origin, vec3_smul(hit.t, ray.dir));

          f32 tu, tv;
          sample_unit_triangle(Rng_f32(&rng), Rng_f32(&rng), &tu, &tv);

          vec3 lp = vec3_add(
            triangles[0].v0,
            vec3_add(
              vec3_smul(tu, vec3_sub(triangles[0].v1, triangles[0].v0)),
              vec3_smul(tv, vec3_sub(triangles[0].v2, triangles[0].v0))
            )
          );

          vec3 origin = vec3_add(p, vec3_smul(0.001f, hit.n)); 

          light_ray = (Ray){
            .origin = origin,
            .dir = vec3_normalized(vec3_sub(lp, origin)),
            .min_t = 1.0e-3f,
            .max_t = 1.0e5f,
          };
        }

        if (vec3_dot(hit.n, light_ray.dir) < 0.0f) {
          continue;
        }

        HitRecord light_hit;
        trace_scene(&light_ray, &scene, &light_hit);

        if (light_hit.idx != light_idx) {
          continue;
        }

        powers[i] = scene.materials[light_idx].emissive.power * clamp(0.0f, 1.0f, vec3_dot(hit.n, light_ray.dir));
        wavelengths[i] = scene.materials[light_idx].emissive.wavelength;
#endif
      }
    }

    // Increment sample count by total samples per pixel for this frame
    sample_count += sqrt_spp * sqrt_spp;

    void *buffer;
    int pitch;
    SDL_LockTexture(screen, NULL, &buffer, &pitch);
    u8 *pixels = buffer;

    for (i32 y = 0; y < height; y++) {
      for (i32 x = 0; x < width; x++) {
        i32 i = y * width + x;

        // Average accumulated XYZ values
        f32 inv_count = 1.0f / (f32)sample_count;
        vec3 avg_xyz = vec3_smul(inv_count, xyz[i]);

        vec3 nxyz = normalize_xyz(avg_xyz);
        vec3 rgb = normalized_xyz_to_linear_rgb(nxyz);
        vec3 srgb = linear_rgb_to_srgb(rgb);

        pixels[(height - 1 - y) * pitch + x * 4 + 0] = (u8)(srgb.r * 255.0f);
        pixels[(height - 1 - y) * pitch + x * 4 + 1] = (u8)(srgb.g * 255.0f);
        pixels[(height - 1 - y) * pitch + x * 4 + 2] = (u8)(srgb.b * 255.0f);
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
