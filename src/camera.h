#ifndef CAMERA_H_INCLUDED
#define CAMERA_H_INCLUDED

#include "kingfisher_core.h"

typedef struct CameraControls {
  vec3s position;
  f32 pitch;      // Radians, always in [-pi/2, pi/2]
  f32 yaw;        // Radians, always in [-pi, pi]
  f32 move_speed;
  f32 rot_speed;
} CameraControls;

typedef struct CameraBasis {
  vec3s position;
  vec3s forward;
  vec3s du;
  vec3s dv;
} CameraBasis;

typedef struct PerspectiveOrtho {
  f32 width;
  f32 height;
  f32 near;
  f32 far;
} PerspectiveOrtho;

typedef struct PerspectivePinhole {
  f32 fov_radians; // Horizontal field of view
  f32 inv_aspect_ratio; // height / width
  f32 near;
  f32 far;
} PerspectivePinhole;

enum PerspectiveKind {
  PERSPECTIVE_PINHOLE,
  PERSPECTIVE_ORTHOGRAPHIC,
};

typedef struct Perspective {
  i32 selected; // PerspectiveKind
  PerspectivePinhole pinhole;
  PerspectiveOrtho ortho;
} Perspective;

void generate_primary_ray_ortho(CameraBasis const *basis, PerspectiveOrtho const *opt, Ray *ray, f32 u, f32 v);
void generate_primary_ray_pinhole(CameraBasis const *basis, PerspectivePinhole const *opt, Ray *ray, f32 u, f32 v);

void camera_controls_to_vectors(CameraControls const *controls, vec3s *forward, vec3s *du, vec3s *dv);
void camera_controls_to_basis(CameraControls const *controls, CameraBasis *basis);
bool camera_has_moved(CameraControls const *current, CameraControls const *previous);
void update_camera_from_input(CameraControls *camera, const bool *keys, f32 dt);

#endif // CAMERA_H_INCLUDED
