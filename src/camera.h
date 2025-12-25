#ifndef CAMERA_H_INCLUDED
#define CAMERA_H_INCLUDED

#include "kingfisher.h"

typedef struct CameraControls {
  vec3 position;
  f32 pitch;      // Radians, always in [-pi/2, pi/2]
  f32 yaw;        // Radians, always in [-pi, pi]
  f32 move_speed;
  f32 rot_speed;
} CameraControls;

typedef struct CameraBasis {
  vec3 position;
  vec3 forward;
  vec3 du;
  vec3 dv;
} CameraBasis;

typedef struct PerspectiveOrtho {
  f32 width;
  f32 height;
  f32 near;
  f32 far;
} PerspectiveOrtho;

inline void generate_primary_ray_ortho(CameraBasis const *basis,
  PerspectiveOrtho const *opt,
  Ray *ray,
  f32 u,
  f32 v
) {
  vec3 offset = vec3_add(
    vec3_smul(0.5f * u * opt->width, basis->du),
    vec3_smul(0.5f * v * opt->height, basis->dv)
  );

  *ray = (Ray){
    .origin = vec3_add(basis->position, offset),
    .dir = basis->forward,
    .min_t = opt->near,
    .max_t = opt->far,
  };
}

typedef struct PerspectivePinhole {
  f32 fov_radians; // Horizontal field of view
  f32 inv_aspect_ratio; // height / width
  f32 near;
  f32 far;
} PerspectivePinhole;

inline void generate_primary_ray_pinhole(
  CameraBasis const *basis,
  PerspectivePinhole const *opt,
  Ray *ray,
  f32 u,
  f32 v
) {
  f32 a = tanf(0.5f * opt->fov_radians);

  vec3 foward = vec3_normalized(
    vec3_add(
      basis->forward,
      vec3_add(
        vec3_smul(a * u, basis->du),
        vec3_smul(opt->inv_aspect_ratio * a * v, basis->dv))));

  *ray = (Ray){
    .origin = basis->position,
    .dir = foward,
    .min_t = opt->near,
    .max_t = opt->far,
  };
}

inline void camera_controls_to_vectors(CameraControls const *controls, vec3 *forward, vec3 *du, vec3 *dv) {
  // Calculate forward direction from pitch and yaw
  // Pitch rotates around the right axis (up/down)
  // Yaw rotates around the world up axis (left/right)

  *forward = pitch_yaw_to_vec3(controls->pitch, controls->yaw);

  // Calculate right vector (perpendicular to xz_forward and world up).
  // We cannot use forward, because it is possible to look straight up or down
  // and then forward and world_up are in the same (or exactly opposite) direction.
  vec3 xz_forward = { cosf(controls->yaw), 0, sinf(controls->yaw), };
  vec3 world_up = { 0.0f, 1.0f, 0.0f };
  *du = vec3_normalized(vec3_cross(xz_forward, world_up));

  // Calculate up vector (perpendicular to right and forward)
  *dv = vec3_normalized(vec3_cross(*du, *forward));
}

inline void camera_controls_to_basis(CameraControls const *controls, CameraBasis *basis) {
  camera_controls_to_vectors(controls, &basis->forward, &basis->du, &basis->dv);
  basis->position = controls->position;
}

inline bool camera_has_moved(CameraControls const *current, CameraControls const *previous) {
  if (current->position.x != previous->position.x) return true;
  if (current->position.y != previous->position.y) return true;
  if (current->position.z != previous->position.z) return true;
  if (current->pitch != previous->pitch) return true;
  if (current->yaw != previous->yaw) return true;

  return false;
}

#endif // CAMERA_H_INCLUDED
