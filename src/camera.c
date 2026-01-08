#include "camera.h"
#include <SDL3/SDL.h>

void generate_primary_ray_ortho(CameraBasis const *basis,
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

void generate_primary_ray_pinhole(
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

void camera_controls_to_vectors(CameraControls const *controls, vec3 *forward, vec3 *du, vec3 *dv) {
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

void camera_controls_to_basis(CameraControls const *controls, CameraBasis *basis) {
  camera_controls_to_vectors(controls, &basis->forward, &basis->du, &basis->dv);
  basis->position = controls->position;
}

bool camera_has_moved(CameraControls const *current, CameraControls const *previous) {
  if (current->position.x != previous->position.x) return true;
  if (current->position.y != previous->position.y) return true;
  if (current->position.z != previous->position.z) return true;
  if (current->pitch != previous->pitch) return true;
  if (current->yaw != previous->yaw) return true;

  return false;
}

void update_camera_from_input(CameraControls *camera, const bool *keys, f32 dt) {
  // Get current camera direction vectors
  vec3 dir, du, dv;
  camera_controls_to_vectors(camera, &dir, &du, &dv);

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
  if (keys[SDL_SCANCODE_E]) {
    movement = vec3_add(movement, dv);
  }
  if (keys[SDL_SCANCODE_Q]) {
    movement = vec3_sub(movement, dv);
  }

  f32 mult = 1.0f;
  if (keys[SDL_SCANCODE_LSHIFT]) {
    mult = 4.0f;
  }

  if (vec3_dot(movement, movement) > 0.0f) {
    vec3 displacement = vec3_smul(camera->move_speed * mult * dt, vec3_normalized(movement));
    camera->position = vec3_add(camera->position, displacement);
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
