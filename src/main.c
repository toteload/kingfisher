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
  VK_CHECK(vkMapMemory(vk.device, vk.storage.memory, 0, VK_WHOLE_SIZE, 0, &mapped));

#if 0
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
#endif

  SDL_Log("Done copying test texture\n");

  Triangle *triangles;
  u64 triangle_count;
  if (!read_obj_triangles("data/models/sponza/sponza.triangulated.obj", &triangles, &triangle_count)) {
    SDL_Log("Failed to load .OBJ file\n");
    return 1;
  }

  SDL_Log("[ ok ] Loaded Sponza scene\n");

  if (!build_acceleration_structures(&vk, triangles, triangle_count)) {
    return 1;
  }

  SDL_Log("[ ok ] Built acceleration structures\n");

  SDL_Log("Running...\n");
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


