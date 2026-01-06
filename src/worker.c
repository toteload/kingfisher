#include "worker.h"
#include "colorspace.h"

void render_xyz(ThreadWork *work) {
  // Do the ray tracing work for assigned rows
  for (u32 y = work->y_start; y < work->y_end; y++) {
    for (i32 x = 0; x < work->width; x++) {
      i32 i = y * work->width + x;

      f32 u = 2.0f * ((f32)x + 0.5f) / work->width - 1.0f;
      f32 v = 2.0f * ((f32)y + 0.5f) / work->height - 1.0f;

      Ray ray;
      if (work->perspective.selected == PERSPECTIVE_PINHOLE) {
        generate_primary_ray_pinhole(&work->basis, &work->perspective.pinhole, &ray, u, v);
      }

      if (work->perspective.selected == PERSPECTIVE_ORTHOGRAPHIC) {
        generate_primary_ray_ortho(&work->basis, &work->perspective.ortho, &ray, u, v);
      }

      f32 strength = 1.0f;
      bool escaped = false;

      for (i32 d = 0; d < 4; d++) {
        HitRecord rec;
        embree_bvh_intersect(work->bvh, &ray, work->triangles, &rec);

        if (rec.t == F32_NO_HIT) {
          escaped = true;
          break;
        }

        vec3 n = triangle_normal(&work->triangles[rec.idx]);
        if (vec3_dot(n, ray.dir) > 0.0f) {
          n = vec3_smul(-1, n);
        }

        vec3 dir = sample_unit_sphere(Rng_f32(&work->rng), Rng_f32(&work->rng));
        if (vec3_dot(n, dir) < 0.0f) {
          dir = vec3_smul(-1, dir);
        }

        strength *= vec3_dot(n, dir);

        ray = (Ray){
          .origin = vec3_add(
            vec3_smul(0.001f, n),
            vec3_add(ray.origin, vec3_smul(rec.t, ray.dir))
          ),
          .dir = dir,
          .min_t = ray.min_t,
          .max_t = ray.max_t,
        };
      }

      if (!escaped) {
        strength = 0.0f;
      }

      // Convert wavelength+power to XYZ and accumulate.
      f32 power = strength * 50.0f;
      u8 wavelength = 120;
     
      vec3 s = spectral_to_xyz(wavelength);
      work->xyz[i] = vec3_add(work->xyz[i], vec3_smul(power, s));
    }
  }
}

void render_normals(ThreadWork *work) {
  // Do the ray tracing work for assigned rows
  for (u32 y = work->y_start; y < work->y_end; y++) {
    for (i32 x = 0; x < work->width; x++) {
      i32 i = y * work->width + x;

      f32 u = 2.0f * ((f32)x + 0.5f) / work->width - 1.0f;
      f32 v = 2.0f * ((f32)y + 0.5f) / work->height - 1.0f;

      Ray ray;
      if (work->perspective.selected == PERSPECTIVE_PINHOLE) {
        generate_primary_ray_pinhole(&work->basis, &work->perspective.pinhole, &ray, u, v);
      }

      if (work->perspective.selected == PERSPECTIVE_ORTHOGRAPHIC) {
        generate_primary_ray_ortho(&work->basis, &work->perspective.ortho, &ray, u, v);
      }

      HitRecord rec;
      embree_bvh_intersect(work->bvh, &ray, work->triangles, &rec);

      if (rec.t == F32_NO_HIT) {
        work->debug_normals[i] = (vec3){ 0.0f, 0.0f, 0.0f };
        continue;
      }

      vec3 n = triangle_normal(&work->triangles[rec.idx]);
      if (vec3_dot(n, ray.dir) > 0.0f) {
        n = vec3_smul(-1.0f, n);
      }
     
      work->debug_normals[i] = vec3_smul(0.5f, vec3_add(n, (vec3){ 1.0f, 1.0f, 1.0f }));
    }
  }
}

int SDLCALL worker(void *data) {
  ThreadWork *work = (ThreadWork *)data;

  while (true) {
    // Wait for main thread to signal work is ready
    SDL_WaitSemaphore(work->start_sem);

    // Check if we should exit
    if (work->should_exit) {
      break;
    }

    if (work->buffer == BUFFER_LIGHT) {
      render_xyz(work);
    }

    if (work->buffer == BUFFER_NORMALS) {
      render_normals(work);
    }

    // Signal that this thread is done
    SDL_SignalSemaphore(work->done_sem);
  }

  return 0;
}
