#version 460
#extension GL_EXT_ray_query : require

const float PI = 3.14159265358979323846;

layout(local_size_x=16, local_size_y=8, local_size_z=1) in;

layout(binding=0, set=0, rgba32f) uniform writeonly image2D image;
layout(binding=1, set=0) uniform accelerationStructureEXT tlas;
layout(push_constant) uniform context {
  vec3 cam_position;
  vec3 cam_forward;
  vec3 cam_du;
  vec3 cam_dv;
};

void main() {
  const ivec2 dim = imageSize(image);
  const float fov_radians = 0.5 * PI;
  const float inv_aspect_ratio = float(dim.y) / float(dim.x);
  const ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

  if (any(greaterThanEqual(pixel, dim))) {
    return;
  }

  vec2 p = 2.0 * (pixel + 0.5) / dim - 1.0;
  float a = tan(0.5 * fov_radians);
  vec3 origin = cam_position;
  vec3 dir = normalize(
    cam_forward
    + (a * p.x * cam_du)
    + (a * p.y * inv_aspect_ratio * cam_dv));

  const float t_min = 0.0;
  const float t_max = 1000.0;

  rayQueryEXT query;
  rayQueryInitializeEXT(
    query,
    tlas,
    gl_RayFlagsOpaqueEXT,
    0xff,
    origin, t_min, dir, t_max);

  // Loop over all intersections of this query.
  // We don't use transparency, so will always be one intersection.
  while (rayQueryProceedEXT(query)) {}

  vec3 color = vec3(0.0);

  if (rayQueryGetIntersectionTypeEXT(query, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
    float t = rayQueryGetIntersectionTEXT(query, true);
    //color = vec3(min(t / 400.0, 1.0));
    color = vec3(0.0, rayQueryGetIntersectionBarycentricsEXT(query, true));
    color.x = 1.0 - color.y - color.z;
  }

  imageStore(image, pixel, vec4(color, 1.0));
}
