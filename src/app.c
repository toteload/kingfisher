#include "app.h"

#define FALLBACK_THREAD_COUNT 8

App *app_create() {
  u32 thread_count = SDL_GetNumLogicalCPUCores();
  if (thread_count == 0) {
    thread_count = FALLBACK_THREAD_COUNT;
  }

  SDL_Thread **threads = SDL_malloc(thread_count * sizeof(SDL_Thread *));
  ThreadWork *works = SDL_malloc(thread_count * sizeof(ThreadWork));

  // Create threads once and initialize synchronization primitives
  u32 rows_per_thread = height / thread_count;

  for (u32 i = 0; i < thread_count; i++) {
    u32 y_start = i * rows_per_thread;
    u32 y_end = (i == thread_count - 1) ? height : (i + 1) * rows_per_thread;

    thread_work[i] = (ThreadWork){
      .thread_id = i,
      .y_start = y_start,
      .y_end = y_end,
      .width = width,
      .height = height,
      .triangles = bunny_triangles,
      .bvh = &build_bvh,
      .basis = NULL, // Will be updated each frame
      .xyz = xyz,
      .start_sem = SDL_CreateSemaphore(0),
      .done_sem = SDL_CreateSemaphore(0),
      .should_exit = false,
    };

    // Seed each thread's RNG with a unique seed
    Rng_seed(&thread_work[t].rng, 13687844445 + i);

    // Create the thread
    char thread_name[32];
    snprintf(thread_name, sizeof(thread_name), "ray-worker-%u", i);
    threads[t] = SDL_CreateThread(raytracing_worker, thread_name, &thread_work[i]);
  }


}
