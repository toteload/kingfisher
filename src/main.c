#include "kingfisher.h"
#include "camera.h"

#include <stdio.h>
#include <assert.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

// Spectral power distribution
typedef struct Spd_8 {
  u8 wavelengths[8];
  f32 powers[8];
} Spd_8;

#define IDX_NO_HIT UINT32_MAX

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

  i32 triangle_count;
  Triangle const *triangles;

  Material const *materials;
} Scene;

void trace_scene(Ray const *ray, Scene const *scene, HitRecord *hit) {
  f32 t = F32_NO_HIT;
  i32 hit_idx = IDX_NO_HIT;
  i32 hit_type = 0; // 0 = sphere, 1 = triangle

  for (i32 i = 0; i < scene->sphere_count; i++) {
    f32 st = ray_sphere_intersect_distance(ray, &scene->spheres[i]);
    if (st < t) {
      t = st;
      hit_idx = i;
      hit_type = 0;
    }
  }

  for (i32 i = 0; i < scene->triangle_count; i++) {
    f32 tt = ray_triangle_intersect_distance(ray, &scene->triangles[i]);
    if (tt < t) {
      t = tt;
      hit_idx = i + scene->sphere_count;
      hit_type = 1;
    }
  }

  if (hit_idx == IDX_NO_HIT) {
    *hit = (HitRecord){
      .t = F32_NO_HIT,
      .idx = IDX_NO_HIT,
    };
    return;
  }

  vec3 n;
  if (hit_type == 0) {
    // Sphere normal
    vec3 p = vec3_add(ray->origin, vec3_mul(ray->dir, (vec3){ t, t, t, }));
    n = vec3_normalized(vec3_sub(p, scene->spheres[hit_idx].origin));
  } else if (hit_type == 1) {
    // Triangle normal
    i32 tri_idx = hit_idx - scene->sphere_count;
    vec3 edge1 = vec3_sub(scene->triangles[tri_idx].v1, scene->triangles[tri_idx].v0);
    vec3 edge2 = vec3_sub(scene->triangles[tri_idx].v2, scene->triangles[tri_idx].v0);
    n = vec3_normalized(vec3_cross(edge1, edge2));

    if (vec3_dot(n, ray->dir) > 0.0f) {
      n = vec3_smul(-1.0f, n);
    }
  } else {
    assert(!"Unknown type of object hit");
  }

  *hit = (HitRecord){
    .t = t,
    .n = n,
    .idx = hit_idx,
  };
}

void update_camera_from_input(CameraControls *camera, const bool *keys, f32 dt) {
  // Get current camera direction vectors
  vec3 dir, du, dv;
  camera_controls_to_vectors(camera, &dir, &du, &dv);

  // Movement in local space
  vec3 movement = { 0.0f, 0.0f, 0.0f };

  if (keys[SDL_SCANCODE_W]) {
    movement = vec3_add(movement, dir);  // Forward
  }
  if (keys[SDL_SCANCODE_S]) {
    movement = vec3_sub(movement, dir);  // Backward
  }
  if (keys[SDL_SCANCODE_A]) {
    movement = vec3_sub(movement, du);   // Strafe left
  }
  if (keys[SDL_SCANCODE_D]) {
    movement = vec3_add(movement, du);   // Strafe right
  }

  // Vertical movement in world space
  if (keys[SDL_SCANCODE_E]) {
    movement.y += 1.0f;  // Move up
  }
  if (keys[SDL_SCANCODE_Q]) {
    movement.y -= 1.0f;  // Move down
  }

  // Apply movement
  if (vec3_dot(movement, movement) > 0.0f) {
    vec3 velocity = vec3_smul(camera->move_speed * dt, vec3_normalized(movement));
    camera->position = vec3_add(camera->position, velocity);
  }

  // Rotation
  if (keys[SDL_SCANCODE_UP]) {
    camera->pitch += camera->rot_speed * dt;  // Pitch up
  }
  if (keys[SDL_SCANCODE_DOWN]) {
    camera->pitch -= camera->rot_speed * dt;  // Pitch down
  }
  if (keys[SDL_SCANCODE_LEFT]) {
    camera->yaw -= camera->rot_speed * dt;    // Yaw left
  }
  if (keys[SDL_SCANCODE_RIGHT]) {
    camera->yaw += camera->rot_speed * dt;    // Yaw right
  }

  while (camera->yaw < 0.0f) {
    camera->yaw += 2.0f * PI;
  }

  while (camera->yaw >= 2.0f * PI) {
    camera->yaw -= 2.0f * PI;
  }

  camera->pitch = clamp(-0.5f * PI, 0.5f * PI, camera->pitch);
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
    { .origin = {  0.0f,  0.5f,  2.0f, }, .radius = 1.0f, },
    { .origin = {  0.0f,  0.0f, -2.0f, }, .radius = 1.0f, },
    { .origin = {  0.0f,  2.0f,  0.0f, }, .radius = 0.1f, },
  };

  Triangle triangles[] = {
    {
      .v0 = {  0.0f,  0.0f, 0.0f },
      .v1 = { -1.0f,  0.0f, 0.0f },
      .v2 = { -1.0f, -1.0f, 0.5f }
    },
    {
      .v0 = { 0.0f,  0.0f, 0.0f },
      .v1 = { 0.0f,  0.0f, 1.0f },
      .v2 = { 0.5f, -1.0f, 1.0f }
    },
    {
      .v0 = {  0.0f,  0.0f, 0.0f },
      .v1 = {  1.0f,  0.0f, 0.0f },
      .v2 = {  1.0f, -1.0f, 0.5f }
    },
    {
      .v0 = { 0.0f,  0.0f,  0.0f },
      .v1 = { 0.0f,  0.0f, -1.0f },
      .v2 = { 0.5f, -1.0f, -1.0f }
    },
    {
      .v0 = {  1.0f,  0.0f,  0.0f },
      .v1 = { -1.0f,  0.0f,  1.0f },
      .v2 = { -1.0f,  0.0f, -1.0f }
    },
  };

  Material materials[] = {
    { MATERIAL_DIFFUSE, },
    { MATERIAL_DIFFUSE, },
    { MATERIAL_DIFFUSE, },
    { MATERIAL_DIFFUSE, },
    { MATERIAL_EMISSIVE, { 40.0f, 110, } },
    { MATERIAL_DIFFUSE, },
    { MATERIAL_DIFFUSE, },
    { MATERIAL_DIFFUSE, },
    { MATERIAL_DIFFUSE, },
  };

  Scene scene = {
    .sphere_count = 5,
    .spheres = spheres,
    .triangle_count = 5,
    .triangles = triangles,
    .materials = materials,
  };

  f32 *powers = malloc(width * height * sizeof(f32));
  u8 *wavelengths = malloc(width * height);

  // Accumulation buffer
  vec3 *xyz = malloc(width * height * sizeof(vec3));
  memset(xyz, 0, width * height * sizeof(vec3));

  // Sample accumulation counter
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

    memset(powers, 0, width * height * sizeof(f32));
    memset(wavelengths, 0, width * height);

    // Calculate camera vectors from camera state
    CameraBasis basis;
    camera_controls_to_basis(&camera, &basis);

    // Trace our rays and save the power and wavelengths
    for (i32 y = 0; y < height; y++) {
      for (i32 x = 0; x < width; x++) {
        i32 i = y * width + x;

        // In range [-1, 1]
        f32 u = 2.0f * ((f32)x) / width - 1.0f;
        f32 v = 2.0f * ((f32)y) / height - 1.0f;

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

        if (vec3_dot(hit.n, light_ray.dir) < 0.0f) {
          continue;
        }

        trace_scene(&light_ray, &scene, &light_hit);

        if (light_hit.idx != light_idx) {
          continue;
        }

        powers[i] = scene.materials[light_idx].emissive.power * clamp(0.0f, 1.0f, vec3_dot(hit.n, light_ray.dir));
        wavelengths[i] = scene.materials[light_idx].emissive.wavelength;
      }
    }

    // Convert the wavelength+power samples to XYZ and accumulate.
    for (i32 y = 0; y < height; y++) {
      for (i32 x = 0; x < width; x++) {
        i32 i = y * width + x;

        vec3 s = spectral_to_xyz(wavelengths[i]);
        xyz[i] = vec3_add(xyz[i], vec3_smul(powers[i], s));
      }
    }

    // Increment sample count after accumulating this frame
    sample_count++;

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
