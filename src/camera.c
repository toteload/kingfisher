#include <SDL3/SDL.h>

#include "camera.h"
#include "toteload.h"

void generate_primary_ray_ortho(CameraBasis const *basis,
  PerspectiveOrtho const *opt,
  Ray *ray,
  f32 u,
  f32 v
) {
  vec3s offset = glms_vec3_add(
    glms_vec3_scale(basis->du, 0.5f * u * opt->width),
    glms_vec3_scale(basis->dv, 0.5f * v * opt->height)
  );

  *ray = (Ray){
    .origin = glms_vec3_add(basis->position, offset),
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

  vec3s forward = glms_vec3_normalize(
    glms_vec3_add(
      basis->forward,
      glms_vec3_add(
        glms_vec3_scale(basis->du, a * u),
        glms_vec3_scale(basis->dv, opt->inv_aspect_ratio * a * v))));

  *ray = (Ray){
    .origin = basis->position,
    .dir = forward,
    .min_t = opt->near,
    .max_t = opt->far,
  };
}

void camera_controls_to_vectors(CameraControls const *controls, vec3s *forward, vec3s *du, vec3s *dv) {
  // Calculate forward direction from pitch and yaw
  // Pitch rotates around the right axis (up/down)
  // Yaw rotates around the world up axis (left/right)

  *forward = pitch_yaw_to_vec3(controls->pitch, controls->yaw);

  // Calculate right vector (perpendicular to xz_forward and world up).
  // We cannot use forward, because it is possible to look straight up or down
  // and then forward and world_up are in the same (or exactly opposite) direction.
  vec3s xz_forward = {{ cosf(controls->yaw), 0, sinf(controls->yaw), }};
  vec3s world_up = {{ 0.0f, 1.0f, 0.0f }};
  *du = glms_vec3_normalize(glms_vec3_cross(xz_forward, world_up));

  // Calculate up vector (perpendicular to right and forward)
  *dv = glms_vec3_normalize(glms_vec3_cross(*du, *forward));
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
  vec3s dir, du, dv;
  camera_controls_to_vectors(camera, &dir, &du, &dv);

  vec3s movement = {{ 0.0f, 0.0f, 0.0f }};

  if (keys[SDL_SCANCODE_W]) {
    movement = glms_vec3_add(movement, dir);  // Forward
  }
  if (keys[SDL_SCANCODE_S]) {
    movement = glms_vec3_sub(movement, dir);  // Backward
  }
  if (keys[SDL_SCANCODE_A]) {
    movement = glms_vec3_sub(movement, du);   // Strafe left
  }
  if (keys[SDL_SCANCODE_D]) {
    movement = glms_vec3_add(movement, du);   // Strafe right
  }
  if (keys[SDL_SCANCODE_E]) {
    movement = glms_vec3_add(movement, dv);
  }
  if (keys[SDL_SCANCODE_Q]) {
    movement = glms_vec3_sub(movement, dv);
  }

  f32 mult = 1.0f;
  if (keys[SDL_SCANCODE_LSHIFT]) {
    mult = 4.0f;
  }

  if (glms_vec3_dot(movement, movement) > 0.0f) {
    vec3s displacement = glms_vec3_scale(glms_vec3_normalize(movement), camera->move_speed * mult * dt);
    camera->position = glms_vec3_add(camera->position, displacement);
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

  camera->pitch = Clamp(-0.5f * PI, 0.5f * PI, camera->pitch);
}
