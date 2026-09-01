#version 460
#extension GL_EXT_ray_query : require

const float PI = 3.14159265358979323846;

layout(local_size_x=16, local_size_y=8, local_size_z=1) in;

layout(binding=0, set=0) buffer storage_buffer {
  float image[];
};

layout(binding=1, set=0) uniform accelerationStructureEXT tlas;

layout(binding=2, set=0) uniform context {
  vec3 cam_position;
  vec3 cam_forward;
  vec3 cam_du;
  vec3 cam_dv;
};

void main() {
  //const vec3 origin = cam_origin; // vec3(-55.0, 64.0, 0.0);
  //const vec3 dir = cam_dir; // normalize(vec3(0.0) - origin);
  const uvec2 dim = uvec2(1280, 960);
  const float fov_radians = 0.5 * PI;
  const float inv_aspect_ratio = float(dim.y) / float(dim.x);
  const uvec2 pixel = gl_GlobalInvocationID.xy;

  uint idx = pixel.x + pixel.y * dim.x;

  if (idx >= dim.x * dim.y) {
    return;
  }

  vec2 p = 2.0 * (pixel + 0.5) / dim - 1.0;
  float a = tan(0.5 * fov_radians);
  vec3 origin = cam_position;
  vec3 dir = normalize(cam_forward + (a * p.x * cam_du) + (a * p.y * inv_aspect_ratio * cam_dv));

  const float t_min = 0.0;
  const float t_max = 1000.0;

  rayQueryEXT query;
  rayQueryInitializeEXT(query, tlas, gl_RayFlagsOpaqueEXT, 0xff, origin, t_min, dir, t_max);

  // Loop over all intersections of this query.
  // We don't use transparency, so will always be one intersection.
  while (rayQueryProceedEXT(query)) {}

  vec3 color = vec3(0.0);

  if (rayQueryGetIntersectionTypeEXT(query, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
    float t = rayQueryGetIntersectionTEXT(query, true);

    color = vec3(min(t / 400.0, 1.0));
  }

  image[idx * 3 + 0] = color.r;
  image[idx * 3 + 1] = color.g;
  image[idx * 3 + 2] = color.b;
}
