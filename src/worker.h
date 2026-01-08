#ifndef WORKER_H_INCLUDED
#define WORKER_H_INCLUDED

#include "kingfisher_core.h"
#include "bvh.h"
#include "camera.h"
#include <SDL3/SDL.h>

enum BufferKind {
  BUFFER_LIGHT,
  BUFFER_NORMALS,
};

// Thread work data
typedef struct ThreadWork {
  u32 y_start;
  u32 y_end;
  i32 width;
  i32 height;

  Triangle *triangles;
  EmbreeBvh *bvh;

  Perspective perspective;
  CameraBasis basis;

  u32 buffer; // BufferKind

  vec3s *xyz; // Shared accumulation buffer
  vec3s *debug_normals;

  Rng rng; // Thread-local RNG

  // Synchronization
  SDL_Semaphore *start_sem; // Main signals this to start work
  SDL_Semaphore *done_sem;  // Thread signals this when work is done
  bool should_exit;         // Signal thread to exit
} ThreadWork;

int SDLCALL worker(void *data);

#endif // WORKER_H_INCLUDED
