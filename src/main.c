#define _CRT_SECURE_NO_WARNINGS

#include <volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_render.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>
#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#include <ufbx.h>
#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstatic-in-inline"
#include <cglm/struct.h>
#pragma clang diagnostic pop

#undef _CRT_SECURE_NO_WARNINGS

#include "toteload.h"
#include "vk.h"

#include <assert.h>

int main(int argc, char *argv[]) {
  Unused(argc, argv);

  Arena scratch;
  arena_init(&scratch, &(ArenaOptions){
    .initial_commit_size = KiB(64),
    .reserve_size = MiB(4),
  });

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    return 1;
  }

  int window_width = 1280;
  int window_height = 960;

  kfvk_State vk = {0};
  b32 ok = kfvk_create(&vk, &scratch, KFVK_USE_VALIDATION);
  if (!ok) {
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow("Kingfisher", window_width, window_height, SDL_WINDOW_VULKAN);
  if (!window) {
    return 1;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(window, Null);
  if (!renderer) {
    return 1;
  }

  SDL_Texture *screen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, window_width, window_height);
  if (!screen) {
    return 1;
  }

  VkSurfaceKHR surface;
  if (!SDL_Vulkan_CreateSurface(window, vk.instance, Null, &surface)) {
    SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
    return 1;
  }

  if (!kfvk_dispatch(&vk)) {
    return 1;
  }

  void *mapped;
  VK_CHECK(vkMapMemory(vk.device, vk.storage_memory, 0, VK_WHOLE_SIZE, 0, &mapped));

  {
  f32 *ps = mapped;
  u32 l = (window_width * window_height - 1) * 3;
  SDL_Log("%f %f %f", ps[l], ps[l+1], ps[l+2]);
  }

  void *pixels = Null;
  int pitch;
  SDL_LockTexture(screen, Null, &pixels, &pitch);
  for (u32 y = 0; y < window_height; y++) {
    for (u32 x = 0; x < window_width; x++) {
      Cast(u8*,pixels)[y * pitch + 4 * x + 0] = Cast(u8, Cast(f32*,mapped)[3 * (y * window_width + x) + 0] * 255.0f);
      Cast(u8*,pixels)[y * pitch + 4 * x + 1] = Cast(u8, Cast(f32*,mapped)[3 * (y * window_width + x) + 1] * 255.0f);
      Cast(u8*,pixels)[y * pitch + 4 * x + 2] = Cast(u8, Cast(f32*,mapped)[3 * (y * window_width + x) + 2] * 255.0f);
      Cast(u8*,pixels)[y * pitch + 4 * x + 3] = 255;
    }
  }
  SDL_UnlockTexture(screen);

  b32 running = True;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        goto exit;
      }

      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
        goto exit;
      }
    }

    SDL_SetRenderDrawColor(renderer, 0,0,0,0);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, screen, Null, Null);

    SDL_RenderPresent(renderer);
  }

exit:

  SDL_Log("ok\n");

  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}

#if 0

// Spectral power distribution
typedef struct Spd_8 {
  u8 wavelengths[8];
  f32 powers[8];
} Spd_8;

bool read_obj_triangles(char const *filename, Triangle **triangles, u64 *triangle_count) {
  fastObjMesh *mesh = fast_obj_read(filename);

  if (!mesh) {
    return false;
  }

  u64 count = 0;
  for (u32 i = 0; i < mesh->group_count; i++) {
    fastObjGroup *group = &mesh->groups[i];

    for (u32 j = 0; j < group->face_count; j++) {
      // Only support triangles
      assert(mesh->face_vertices[group->face_offset + j] == 3);
    }

    count += group->face_count;
  }

  Triangle *tris = malloc(count * sizeof(Triangle));

  u64 triangle_offset = 0;
  for (u32 i = 0; i < mesh->group_count; i++) {
    fastObjGroup *group = &mesh->groups[i];

    for (u32 j = 0; j < group->face_count; j++) {
      for (u32 k = 0; k < 3; k++) {
        fastObjIndex idx = mesh->indices[group->index_offset + 3 * j + k];

        assert(idx.p != 0);

        tris[triangle_offset + j].p[k] = (vec3s){{
          mesh->positions[3 * idx.p + 0],
          mesh->positions[3 * idx.p + 1],
          mesh->positions[3 * idx.p + 2],
        }};
      }
    }

    triangle_offset += group->face_count;
  }

  *triangle_count = count;
  *triangles = tris;

  fast_obj_destroy(mesh);

  return true;
}

bool read_fbx_triangles(char const *filename, Triangle **triangles, u64 *triangle_count) {
  ufbx_error err;
  ufbx_scene *scene = ufbx_load_file(filename, NULL, &err);

  if (!scene) {
    fprintf(stderr, "Failed to load FBX: %s\n", err.description.data);
    return false;
  }

  // First pass: count total triangles
  u64 total_triangle_count = 0;
  for (u64 node_idx = 0; node_idx < scene->nodes.count; node_idx++) {
    ufbx_node *node = scene->nodes.data[node_idx];
    if (!node->mesh) continue;

    ufbx_mesh *mesh = node->mesh;
    for (u64 face_idx = 0; face_idx < mesh->faces.count; face_idx++) {
      ufbx_face face = mesh->faces.data[face_idx];
      // Assert all faces are triangles
      assert(face.num_indices == 3);
    }

    total_triangle_count += mesh->faces.count;
  }

  // Allocate triangle array
  Triangle *tris = malloc(total_triangle_count * sizeof(Triangle));
  u64 tri_idx = 0;

  // Second pass: extract and transform triangles
  for (u64 node_idx = 0; node_idx < scene->nodes.count; node_idx++) {
    ufbx_node *node = scene->nodes.data[node_idx];
    if (!node->mesh) continue;

    ufbx_mesh *mesh = node->mesh;

    // Get the node's local transform matrix
    ufbx_matrix node_to_world = node->node_to_world;

    for (u64 face_idx = 0; face_idx < mesh->faces.count; face_idx++) {
      ufbx_face face = mesh->faces.data[face_idx];

      // Extract the three vertex indices for this triangle
      for (u32 vert_idx = 0; vert_idx < 3; vert_idx++) {
        u32 index = mesh->vertex_indices.data[face.index_begin + vert_idx];
        ufbx_vec3 pos = mesh->vertices.data[index];

        // Transform vertex by node's local transform
        ufbx_vec3 transformed = ufbx_transform_position(&node_to_world, pos);

        tris[tri_idx].p[vert_idx] = (vec3s){{
          (f32)transformed.x,
          (f32)transformed.y,
          (f32)transformed.z,
        }};
      }

      tri_idx++;
    }
  }

  *triangle_count = total_triangle_count;
  *triangles = tris;

  ufbx_free_scene(scene);

  return true;
}

void transform_triangles(mat3s m, Triangle *triangles, u64 count) {
  for (u64 i = 0; i < count; i++) {
    triangles[i].p[0] = glms_mat3_mulv(m, triangles[i].p[0]);
    triangles[i].p[1] = glms_mat3_mulv(m, triangles[i].p[1]);
    triangles[i].p[2] = glms_mat3_mulv(m, triangles[i].p[2]);
  }
}

void xyz_to_srgb_pixels(i32 width, i32 height, vec3s const *xyz, u8 *pixels, i32 pitch) {
  for (i32 y = 0; y < height; y++) {
    for (i32 x = 0; x < width; x++) {
      i32 i = y * width + x;

      vec3s nxyz = normalize_xyz(xyz[i]);
      vec3s rgb = normalized_xyz_to_linear_rgb(nxyz);
      vec3s srgb = linear_rgb_to_srgb(rgb);

      pixels[(height - 1 - y) * pitch + x * 4 + 0] = (u8)(srgb.r * 255.0f);
      pixels[(height - 1 - y) * pitch + x * 4 + 1] = (u8)(srgb.g * 255.0f);
      pixels[(height - 1 - y) * pitch + x * 4 + 2] = (u8)(srgb.b * 255.0f);
    }
  }
}

void normals_to_srgb_pixels(i32 width, i32 height, vec3s const *normals, u8 *pixels, i32 pitch) {
  for (i32 y = 0; y < height; y++) {
    for (i32 x = 0; x < width; x++) {
      i32 i = y * width + x;

      vec3s rgb = glms_vec3_scale(normals[i], 255.0f);
      vec3s srgb = linear_rgb_to_srgb(rgb);

      pixels[(height - 1 - y) * pitch + x * 4 + 0] = (u8)(srgb.r * 255.0f);
      pixels[(height - 1 - y) * pitch + x * 4 + 1] = (u8)(srgb.g * 255.0f);
      pixels[(height - 1 - y) * pitch + x * 4 + 2] = (u8)(srgb.b * 255.0f);
    }
  }
}

int IsDebuggerPresent();

typedef struct AppState {
  Perspective perspective;

  struct {
    CameraControls current;
    // Previous camera state for tracking movement
    CameraControls prev;
  } camera;

  i32 selected_buffer;
} AppState;

void draw_debug_ui(struct nk_context *ctx, AppState *app) {
  f32 line_height = 18;

  if (nk_begin(ctx, "Debug info", nk_rect(20, 20, 300, 400), NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE)) {
    nk_layout_row_static(ctx, line_height, 200, 1);
    nk_label(ctx, "Position", NK_TEXT_LEFT);
    nk_property_float(ctx, "#x ", -1e6f, &app->camera.current.position.x, 1e6f, 0.01f, 0.01f);
    nk_property_float(ctx, "#y ", -1e6f, &app->camera.current.position.y, 1e6f, 0.01f, 0.01f);
    nk_property_float(ctx, "#z ", -1e6f, &app->camera.current.position.z, 1e6f, 0.01f, 0.01f);

    nk_label(ctx, "Perspective", NK_TEXT_LEFT);
    app->perspective.selected = nk_combo(
      ctx,
      (const char *[2]){"PINHOLE", "ORTHOGRAPHIC"},
      2,
      app->perspective.selected,
      line_height,
      nk_vec2(200, 200)
    );

    nk_label(ctx, "Pinhole settings", NK_TEXT_LEFT);
    nk_property_float(ctx, "fov", 0.01f * PI, &app->perspective.pinhole.fov_radians, PI, 0.01f, 0.01f);

    nk_label(ctx, "Orthograpic settings", NK_TEXT_LEFT);
    nk_label(ctx, "TODO", NK_TEXT_LEFT);

    nk_label(ctx, "Selected buffer", NK_TEXT_LEFT);
    app->selected_buffer = nk_combo(ctx, (const char *[2]){"LIGHT", "NORMALS"}, 2, app->selected_buffer, line_height, nk_vec2(200, 200));

    nk_end(ctx);
  }
}


int main(int argc, char *argv[]) {
  Unused(argc, argv);

  if (IsDebuggerPresent()) {
    // When you are running with a debugger it always breaks at the start.
    // Useful to skip all the setup code from SDL.
    __debugbreak();
  }

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    return 1;
  }

  int window_width = 1280;
  int window_height = 960;

  int render_width = window_width / 1;
  int render_height = window_height / 1;

  SDL_Window *window;
  SDL_Renderer *renderer;
  if (!SDL_CreateWindowAndRenderer("Kingfisher", window_width, window_height, 0, &window, &renderer)) {
    return 1;
  }

  {
    bool ok = SDL_SetRenderVSync(renderer, 1);
    printf("vsync: %s\n", ok ? "yes" : "no");
  }

  UiState *ui = ui_init(window, renderer);

  SDL_Texture *screen = SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_XBGR8888,
    SDL_TEXTUREACCESS_STREAMING,
    render_width,
    render_height
  );

  u64 triangle_count;
  Triangle *triangles;
#if 1
  if (!read_obj_triangles("data/models/sponza/sponza.triangulated.obj", &triangles, &triangle_count)) {
    printf("Failed to load .OBJ file\n");
    return 1;
  }

  mat3s transform;
  {
    mat4s m = glms_mat4_identity();
    m = glms_scale_uni(m, 0.1f);
    //m = glms_rotate_y(m, 0.0f);
    transform = glms_mat4_pick3(m);
  }
  transform_triangles(transform, triangles, triangle_count);
#endif
#if 0
  if (!read_obj_triangles("data/models/stanford_bunny.obj", &triangles, &triangle_count)) {
    printf("Failed to load .OBJ file\n");
    return 1;
  }
#endif
#if 0
  if (!read_fbx_triangles("data/models/pica-pica-mini-diorama-01/Mini_Diorama_01.fbx", &triangles, &triangle_count)) {
    printf("Failed to load .fbx file.\n");
    return 1;
  }
#endif

  EmbreeBvh build_bvh = embree_bvh_build(triangles, triangle_count);

  Bvh bvh;
  bvh_build_from_embree_bvh(&build_bvh, &bvh);

  // Accumulation buffer
  vec3s *acc_xyz = malloc(render_width * render_height * sizeof(vec3s));
  memset(acc_xyz, 0, render_width * render_height * sizeof(vec3s));

  vec3s *xyz = malloc(render_width * render_height * sizeof(vec3s));
  memset(xyz, 0, render_width * render_height * sizeof(vec3s));

  // Debug normals buffer
  vec3s *normals = malloc(render_width * render_height * sizeof(vec3s));
  memset(normals, 0, render_width * render_height * sizeof(vec3s));

  u32 sample_count = 0;

  u64 last_time_ns = SDL_GetTicksNS();

  Rng rng;
  Rng_seed(&rng, 13687844445);

  CameraControls camera;
  {
    vec3s position = {{ -55.0f, 64.0f, 0.0f }};
    f32 pitch, yaw;
    vec3s dir = glms_vec3_normalize(glms_vec3_sub((vec3s){{0.0f, 0.0f, 0.0f}}, position));
    vec3_to_pitch_yaw(dir, &pitch, &yaw);

    camera = (CameraControls){
      .position = position,
      .pitch = pitch,
      .yaw = yaw,
      .move_speed = 18.0f,
      .rot_speed = 2.0f,
    };
  }

  AppState app = {
    .perspective = {
      .selected = PERSPECTIVE_PINHOLE,
      .pinhole = {
        .fov_radians = 0.5f * PI,
        .inv_aspect_ratio = ((f32)render_height) / render_width,
        .near = 1.0e-3f,
        .far = 1.0e5f,
      },
      .ortho = {
        .width = render_width / 4.0f,
        .height = render_height / 4.0f,
        .near = 1.0e-3f,
        .far = 1.0e5f,
      },
    },
    .camera = {
      .current = camera,
      .prev = camera,
    },
    .selected_buffer = BUFFER_LIGHT,
  };

  CameraBasis basis;
  camera_controls_to_basis(&app.camera.current, &basis);

  // Set up multithreading
  u32 thread_count = SDL_GetNumLogicalCPUCores();
  if (thread_count == 0) thread_count = 4; // Fallback if detection fails

  SDL_Thread **threads = malloc(thread_count * sizeof(SDL_Thread *));
  ThreadWork *thread_work = malloc(thread_count * sizeof(ThreadWork));
  bool *thread_done = malloc(thread_count * sizeof(bool));

  for (u32 i = 0; i < thread_count; i++) {
    thread_done[i] = true;
  }

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
      .triangles = triangles,
      .bvh = &build_bvh,
      .perspective = app.perspective,
      .basis = basis,
      .buffer = app.selected_buffer,
      .xyz = xyz,
      .debug_normals = normals,
      .start_sem = SDL_CreateSemaphore(0),
      .done_sem = SDL_CreateSemaphore(0),
      .should_exit = false,
    };

    // Seed each thread's RNG with a unique seed
    Rng_seed(&thread_work[t].rng, 13687844445 + t);

    // Create the thread
    char thread_name[32];
    snprintf(thread_name, sizeof(thread_name), "RayWorker%u", t);
    threads[t] = SDL_CreateThread(worker, thread_name, &thread_work[t]);
  }

  u32 done = 0;
  while (!done) {
    u64 time_ns = SDL_GetTicksNS();
    u64 dt_ns = time_ns - last_time_ns;
    f32 dt = ((f32)dt_ns) / 1.0e9f;
    last_time_ns = time_ns;

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
    update_camera_from_input(&app.camera.current, keys, dt);

    bool all_render_threads_done = true;
    {
      for (u32 i = 0; i < thread_count; i++) {
        if (!thread_done[i]) {
          thread_done[i] = SDL_TryWaitSemaphore(thread_work[i].done_sem);
        }
      }

      for (u32 i = 0; i < thread_count; i++) {
        all_render_threads_done &= thread_done[i];
      }
    }

    if (all_render_threads_done) {
      // Reset accumulation if needed
      // TODO also reset accumulation on changes of other parameters.
      if (camera_has_moved(&app.camera.current, &app.camera.prev)) {
        sample_count = 0;
      }

      app.camera.prev = app.camera.current;

      void *buffer;
      int pitch;
      SDL_LockTexture(screen, NULL, &buffer, &pitch);
      {
        if (app.selected_buffer == BUFFER_LIGHT) {
          // Accumulation
          {
            f32 s;
            if (sample_count == 0) {
              s = 1.0f;
            } else {
              s = (1.0f / (f32)sample_count);
            }

            for (i32 i = 0; i < render_width * render_height; i++) {
              acc_xyz[i] = glms_vec3_add(
                glms_vec3_scale(acc_xyz[i], 1.0f - s),
                glms_vec3_scale(xyz[i], s));
            }

            sample_count += 1;
          }

          xyz_to_srgb_pixels(render_width, render_height, acc_xyz, buffer, pitch);
        }

        if (app.selected_buffer == BUFFER_NORMALS) {
          normals_to_srgb_pixels(render_width, render_height, normals, buffer, pitch);
        }
      }
      SDL_UnlockTexture(screen);

      camera_controls_to_basis(&app.camera.current, &basis);

      memset(normals, 0, render_width * render_height * sizeof(vec3s));
      memset(xyz, 0, render_width * render_height * sizeof(vec3s));

      for (u32 i = 0; i < thread_count; i++) {
        thread_work[i].buffer = app.selected_buffer;
        thread_work[i].basis = basis;
        thread_work[i].perspective = app.perspective;
      }

      // Signal all threads to start work
      for (u32 t = 0; t < thread_count; t++) {
        SDL_SignalSemaphore(thread_work[t].start_sem);
      }

      for (u32 i = 0; i < thread_count; i++) {
        thread_done[i] = false;
      }
    }

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(renderer);

    SDL_RenderTexture(
      renderer,
      screen,
      NULL,
      NULL
      //&(SDL_FRect){.x = 640, .y = 480, .w = 640, .h = 480}
      );

    draw_debug_ui(ui->ctx, &app);

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
  free(triangles);

  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  SDL_Quit();

  return 0;
}
#endif
