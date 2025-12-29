#ifndef APP_H_INCLUDED
#define APP_H_INCLUDED

#include "kingfisher_core.h"

typedef struct AppOptions {
  char const *title;
  i32 width, height;

  struct {
    vec3 position;
    f32 pitch, yaw;
  } camera;
} AppOptions;

typedef struct App {
  SDL_Window *window;
  SDL_Renderer *renderer;
  UiState *ui;
  SDL_Texture *screen;

  u64 triangle_count;
  Triangle *triangles;

  EmbreeBvh build_bvh;

  vec3 *xyz; // Accumulation buffer
  u32 sample_count;

  u64 last_ticks_ns;

  i32 done;

  CameraControls controls;
  CameraControls controls_prev;

  struct {
    CameraBasis *basis;
    SDL_Thread **threads;
    ThreadWork *work;
    u32 count;
  } workers;
} App;

App *app_create(AppOptions const *opt);
void app_destroy(App *app);

#endif // APP_H_INCLUDED
