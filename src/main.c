#define _CRT_SECURE_NO_WARNINGS

#include <volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_render.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstatic-in-inline"
#include <cglm/struct.h>
#pragma clang diagnostic pop

#undef _CRT_SECURE_NO_WARNINGS

#include "toteload.h"
#include "vk.h"
#include "model.h"
#include "camera.h"

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

  SDL_Window *window = SDL_CreateWindow("Kingfisher", window_width, window_height, SDL_WINDOW_VULKAN);
  if (!window) {
    return 1;
  }

  kfvk_Graphics gfx = {0};
  b32 ok = kfvk_create_graphics(&gfx, window, KFVK_USE_VALIDATION);
  if (!ok) {
    return 1;
  }

  kfvk_Swapchain swapchain = {0};
  ok = kfvk_create_swapchain(&swapchain, &gfx, window);
  if (!ok) {
    return 1;
  }

  Triangle *triangles;
  u64 triangle_count;
  if (!read_obj_triangles("data/models/sponza/sponza.triangulated.obj", &triangles, &triangle_count)) {
    SDL_Log("Failed to load .OBJ file\n");
    return 1;
  }

  SDL_Log("[ ok ] Loaded Sponza scene\n");

  kfvk_RayTracing rt;
  if (!kfvk_create_raytracing_resources(&rt, &gfx, &scratch, triangles, triangle_count)) {
    return 1;
  }

  SDL_Log("[ ok ] Built acceleration structures\n");

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

  CameraBasis camera_basis;
  camera_controls_to_basis(&camera, &camera_basis);

  kfvk_rt_update_context(&gfx, &rt, &camera_basis);

  u64 frequency = SDL_GetPerformanceFrequency();

  SDL_Log("Running...\n");
  b32 running = True;
  while (running) {
    u64 counter_start = SDL_GetPerformanceCounter();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        goto exit;
      }

      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
        goto exit;
      }
    }

    b32 ok = True;

    ok = kfvk_swapchain_acquire(&gfx, &swapchain);

    u64 counter_dispatch = SDL_GetPerformanceCounter();

    kfvk_rt_dispatch(&gfx, &rt, &swapchain);

    u64 counter_read_image = SDL_GetPerformanceCounter();

    u64 counter_render = SDL_GetPerformanceCounter();

    u64 counter_present = SDL_GetPerformanceCounter();

    ok = kfvk_swapchain_present_and_wait(&gfx, &swapchain);

    u64 counter_end = SDL_GetPerformanceCounter();

    u64 elapsed = counter_end - counter_start;
    f64 to_ms = 1000.0 / Cast(f64, frequency);
    f64 to_pct = 100.0 / Cast(f64, elapsed);

    SDL_Log("%4.2fms | events: %3.1f, dispatch: %3.1f, read: %3.1f, render: %3.1f, present: %3.1f\n",
      Cast(f64, elapsed) * to_ms,
      Cast(f64, counter_dispatch   - counter_start)      * to_pct,
      Cast(f64, counter_read_image - counter_dispatch)   * to_pct,
      Cast(f64, counter_render     - counter_read_image) * to_pct,
      Cast(f64, counter_present    - counter_render)     * to_pct,
      Cast(f64, counter_end        - counter_present)    * to_pct);
  }

exit:

  SDL_Log("ok\n");

  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}


