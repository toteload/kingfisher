#include "kingfisher_core.h"
#include "camera.h"
#include "colorspace.h"
#include "bvh.h"
#include "ui.h"

#include <stdio.h>
#include <assert.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#pragma warning(push)
#pragma warning(disable:4996)
#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>
#pragma warning(pop)

#include <ufbx.h>

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

bool read_obj_triangles(char const *filename, Triangle **triangles, u64 *triangle_count) {
  fastObjMesh *mesh = fast_obj_read(filename);

  if (!mesh) {
    return false;
  }

  assert(mesh->group_count == 1);

  fastObjGroup group = mesh->groups[0];

  Triangle *tris = malloc(mesh->face_count * sizeof(Triangle));
  for (u32 i = 0; i < group.face_count; i++) {
    // Only support triangles
    assert(mesh->face_vertices[group.face_offset + i] == 3);

    for (u32 j = 0; j < 3; j++) {
      fastObjIndex idx = mesh->indices[group.index_offset + 3 * i + j];

      assert(idx.p != 0);

      tris[i].p[j] = (vec3){
        mesh->positions[3 * idx.p + 0],
        mesh->positions[3 * idx.p + 1],
        mesh->positions[3 * idx.p + 2],
      };
    }
  }

  *triangle_count = group.face_count;
  *triangles = tris;

  fast_obj_destroy(mesh);

  return true;
}

// Thread work data
typedef struct ThreadWork {
  u32 y_start;
  u32 y_end;
  i32 width;
  i32 height;

  Triangle *triangles;
  EmbreeBvh *bvh;
  CameraBasis *basis;

  vec3 *xyz; // Shared accumulation buffer

  Rng rng; // Thread-local RNG

  // Synchronization
  SDL_Semaphore *start_sem; // Main signals this to start work
  SDL_Semaphore *done_sem;  // Thread signals this when work is done
  bool should_exit;         // Signal thread to exit
} ThreadWork;

// Worker thread function for ray tracing
int raytracing_worker(void *data) {
  ThreadWork *work = (ThreadWork *)data;

  while (true) {
    // Wait for main thread to signal work is ready
    SDL_WaitSemaphore(work->start_sem);

    // Check if we should exit
    if (work->should_exit) {
      break;
    }

    // Do the ray tracing work for assigned rows
    for (u32 y = work->y_start; y < work->y_end; y++) {
      for (i32 x = 0; x < work->width; x++) {
        i32 i = y * work->width + x;

        f32 u = 2.0f * ((f32)x + 0.5f) / work->width - 1.0f;
        f32 v = 2.0f * ((f32)y + 0.5f) / work->height - 1.0f;

        Ray ray;
        {
          PerspectivePinhole pinhole = {
            .fov_radians = 0.5f * PI,
            .inv_aspect_ratio = ((f32)work->height) / work->width,
            .near = 1.0e-3f,
            .far = 1.0e5f,
          };

          generate_primary_ray_pinhole(work->basis, &pinhole, &ray, u, v);
        }

        HitRecord rec;
        embree_bvh_intersect(work->bvh, &ray, work->triangles, &rec);

        if (rec.t == F32_NO_HIT) {
          continue;
        }

        // Convert wavelength+power to XYZ and accumulate.
        f32 power = 40.0f;
        u8 wavelength = 120;

        vec3 s = spectral_to_xyz(wavelength);
        work->xyz[i] = vec3_add(work->xyz[i], vec3_smul(power, s));
      }
    }

    // Signal that this thread is done
    SDL_SignalSemaphore(work->done_sem);
  }

  return 0;
}

int main(int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    return 1;
  }

  int window_width = 1280;
  int window_height = 960;

  int render_width = 640;
  int render_height = 480;

  SDL_Window *window;
  SDL_Renderer *renderer;
  if (!SDL_CreateWindowAndRenderer("Kingfisher", window_width, window_height, 0, &window, &renderer)) {
    return 1;
  }

  UiState *ui = ui_init(window, renderer);

  SDL_Texture *screen = SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_XBGR8888,
    SDL_TEXTUREACCESS_STREAMING,
    render_width,
    render_height
  );

#if 0
  {
    ufbx_error err;
    ufbx_scene *scene = ufbx_load_file("data/pica-pica-floor-junk-clutter-01/Floor_Junk_Cluster_01.fbx", NULL, &err);
    if (!scene) {
      fprintf(stderr, "Failed to load: %s\n", err.description.data);
      return 1;
    }

    for (u32 i = 0; i < scene->nodes.count; i++) {
      ufbx_node *node = scene->nodes.data[i];
      if (node->is_root) continue;

      printf("object: %s\n", node->name.data);
      if (node->mesh) {
        printf("-> mesh with %zu faces\n", node->mesh->faces.count);
      }
    }

    ufbx_free_scene(scene);
  }
#endif

  u64 bunny_triangle_count;
  Triangle *bunny_triangles;
  if (!read_obj_triangles("data/stanford_bunny.obj", &bunny_triangles, &bunny_triangle_count)) {
    printf("Failed to load Stanford bunny.\n");
    return 1;
  }

  EmbreeBvh build_bvh = embree_bvh_build(bunny_triangles, bunny_triangle_count);

  Bvh bvh;
  bvh_build_from_embree_bvh(&build_bvh, &bvh);

  // Accumulation buffer
  vec3 *xyz = malloc(render_width * render_height * sizeof(vec3));
  memset(xyz, 0, render_width * render_height * sizeof(vec3));

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

  // Set up multithreading
  u32 thread_count = SDL_GetNumLogicalCPUCores();
  if (thread_count == 0) thread_count = 4; // Fallback if detection fails

  SDL_Thread **threads = malloc(thread_count * sizeof(SDL_Thread *));
  ThreadWork *thread_work = malloc(thread_count * sizeof(ThreadWork));

  printf("Using %u worker threads\n", thread_count);

  // Create threads once and initialize synchronization primitives
  u32 rows_per_thread = render_height / thread_count;
  for (u32 t = 0; t < thread_count; t++) {
    u32 y_start = t * rows_per_thread;
    u32 y_end = (t == thread_count - 1) ? render_height : (t + 1) * rows_per_thread;

    thread_work[t] = (ThreadWork){
      .y_start = y_start,
      .y_end = y_end,
      .width = render_width,
      .height = render_height,
      .triangles = bunny_triangles,
      .bvh = &build_bvh,
      .basis = NULL, // Will be updated each frame
      .xyz = xyz,
      .start_sem = SDL_CreateSemaphore(0),
      .done_sem = SDL_CreateSemaphore(0),
      .should_exit = false,
    };

    // Seed each thread's RNG with a unique seed
    Rng_seed(&thread_work[t].rng, 13687844445 + t);

    // Create the thread
    char thread_name[32];
    snprintf(thread_name, sizeof(thread_name), "RayWorker%u", t);
    threads[t] = SDL_CreateThread(raytracing_worker, thread_name, &thread_work[t]);
  }

  u32 done = 0;
  while (!done) {
    u64 time_ns = SDL_GetTicksNS();
    u64 dt_ns = time_ns - last_time_ns;
    f32 dt = ((f32)dt_ns) / 1.0e9f;
    last_time_ns = time_ns;

    time += dt;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ui_handle_event(ui, &event);

      if (event.type == SDL_EVENT_QUIT) {
        goto exit;
      }

      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
        goto exit;
      }
    }

    // Update camera from keyboard input
    const bool *keys = SDL_GetKeyboardState(NULL);
    update_camera_from_input(&camera, keys, dt);

    // Check if camera has moved and reset accumulation if needed
    if (camera_has_moved(&camera, &camera_prev)) {
      memset(xyz, 0, render_width * render_height * sizeof(vec3));
      sample_count = 0;
    }

    camera_prev = camera;

    // Calculate camera basis from camera controls
    CameraBasis basis;
    camera_controls_to_basis(&camera, &basis);

    // Update camera basis for all threads
    for (u32 t = 0; t < thread_count; t++) {
      thread_work[t].basis = &basis;
    }

    // Signal all threads to start work
    for (u32 t = 0; t < thread_count; t++) {
      SDL_SignalSemaphore(thread_work[t].start_sem);
    }

    // Wait for all threads to complete (synchronization point)
    for (u32 t = 0; t < thread_count; t++) {
      SDL_WaitSemaphore(thread_work[t].done_sem);
    }

    sample_count += 1;

    void *buffer;
    int pitch;
    SDL_LockTexture(screen, NULL, &buffer, &pitch);
    u8 *pixels = buffer;

    for (i32 y = 0; y < render_height; y++) {
      for (i32 x = 0; x < render_width; x++) {
        i32 i = y * render_width + x;

        // Average accumulated XYZ values
        f32 inv_count = 1.0f / (f32)sample_count;
        vec3 avg_xyz = vec3_smul(inv_count, xyz[i]);

        vec3 nxyz = normalize_xyz(avg_xyz);
        vec3 rgb = normalized_xyz_to_linear_rgb(nxyz);
        vec3 srgb = linear_rgb_to_srgb(rgb);

        pixels[(render_height - 1 - y) * pitch + x * 4 + 0] = (u8)(srgb.r * 255.0f);
        pixels[(render_height - 1 - y) * pitch + x * 4 + 1] = (u8)(srgb.g * 255.0f);
        pixels[(render_height - 1 - y) * pitch + x * 4 + 2] = (u8)(srgb.b * 255.0f);
      }
    }

    SDL_UnlockTexture(screen);

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(renderer);

    SDL_RenderTexture(renderer, screen, NULL, NULL);

    {
      struct nk_context *ctx = ui->ctx;
      if (nk_begin(ctx, "Hello", nk_rect(50, 50, 200, 200), NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_MOVABLE)) {
        nk_layout_row_static(ctx, 30, 80, 1);
        if (nk_button_label(ctx, "button")) {
          // if pressed
        }
      }
      nk_end(ctx);
    }

    ui_render(ui);

    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderDebugTextFormat(renderer, 2.0f, 2.0f, "%4.01fms", dt * 1000.0f);

    SDL_RenderPresent(renderer);
  }

exit:
  // Signal all threads to exit
  for (u32 t = 0; t < thread_count; t++) {
    thread_work[t].should_exit = true;
    SDL_SignalSemaphore(thread_work[t].start_sem);
  }

  // Wait for all threads to finish
  for (u32 t = 0; t < thread_count; t++) {
    SDL_WaitThread(threads[t], NULL);
  }

  // Clean up semaphores
  for (u32 t = 0; t < thread_count; t++) {
    SDL_DestroySemaphore(thread_work[t].start_sem);
    SDL_DestroySemaphore(thread_work[t].done_sem);
  }

  free(threads);
  free(thread_work);
  free(xyz);
  free(bunny_triangles);

  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  SDL_Quit();

  return 0;
}
