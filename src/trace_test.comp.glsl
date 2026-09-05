#version 460
#extension GL_EXT_ray_query : require

const float PI = 3.14159265358979323846;

layout(local_size_x=16, local_size_y=8, local_size_z=1) in;

layout(binding=0, set=0, rgba32f) uniform image2D image;
layout(binding=1, set=0) uniform accelerationStructureEXT tlas;
layout(binding=2, set=0) readonly buffer vertices {
  float vertex_data[];
};
layout(push_constant) uniform context {
  vec3 cam_position;
  vec3 cam_forward;
  vec3 cam_du;
  vec3 cam_dv;
  uint sample_index;
};

vec3 read_vertex(uint i) {
  return vec3(vertex_data[3*i + 0], vertex_data[3*i + 1], vertex_data[3*i + 2]);
}

vec3 triangle_normal(vec3 ray_dir, vec3 p0, vec3 p1, vec3 p2) {
  vec3 n = normalize(cross(p1 - p0, p2 - p0));
  return faceforward(n, ray_dir, n);
}

struct HitRecord {
  vec3 world_pos;
  vec3 world_normal;
};

// Assumes the query has an intersection with a triangle
HitRecord get_hit_record(vec3 origin, vec3 dir, rayQueryEXT query) {
  uint prim = rayQueryGetIntersectionPrimitiveIndexEXT(query, true);

  HitRecord rec;

  vec3 p0 = read_vertex(3*prim+0);
  vec3 p1 = read_vertex(3*prim+1);
  vec3 p2 = read_vertex(3*prim+2);

  rec.world_normal = triangle_normal(dir, p0, p1, p2);

  float t = rayQueryGetIntersectionTEXT(query, true);
  rec.world_pos = origin + t * dir;

  return rec;
}

// Random number generation using pcg32i_random_t, using inc = 1. Our random state is a uint.
uint stepRNG(uint rngState)
{
  return rngState * 747796405 + 1;
}

// Steps the RNG and returns a floating-point value between 0 and 1 inclusive.
float rand(inout uint rngState)
{
  // Condensed version of pcg_output_rxs_m_xs_32_32, with simple conversion to floating-point [0,1].
  rngState  = stepRNG(rngState);
  uint word = ((rngState >> ((rngState >> 28) + 4)) ^ rngState) * 277803737;
  word      = (word >> 22) ^ word;
  return float(word) / 4294967295.0f;
}

vec3 sample_unit_sphere(float u1, float u2) {
  float theta = 2.0 * PI * u2;
  float phi = PI * u1;

  float r = sin(phi);

  float x = r * cos(theta);
  float y = cos(phi);
  float z = r * sin(theta);

  return vec3(x,y,z);
}

void main() {
  const ivec2 dim = imageSize(image);
  const float fov_radians = 0.5 * PI;
  const float inv_aspect_ratio = float(dim.y) / float(dim.x);
  const ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

  if (any(greaterThanEqual(pixel, dim))) {
    return;
  }

  uint rng = (pixel.y * dim.x + pixel.x) ^ (sample_index * 2654435761u);

  vec2 p = 2.0 * (pixel + vec2(rand(rng), rand(rng)) - 0.5) / dim - 1.0;
  vec3 origin = cam_position;
  const float a = tan(0.5 * fov_radians);
  vec3 dir = normalize(
    cam_forward
    + (a * p.x * cam_du)
    + (a * p.y * inv_aspect_ratio * cam_dv));

  const float t_min = 0.0;
  const float t_max = 1000.0;

  bool escaped = false;

  const uint n_samples = 4;
  const uint max_bounces = 8;

  vec3 acc = vec3(0.0);

  for (uint i_samples = 0; i_samples < n_samples; i_samples++) {
    for (uint i = 0; i < max_bounces; i++) {
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

      if (rayQueryGetIntersectionTypeEXT(query, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
        HitRecord rec = get_hit_record(origin, dir, query);
        origin = rec.world_pos + 0.001 * rec.world_normal;
        float u1 = rand(rng);
        float u2 = rand(rng);

        // this way of generating normals can create zero vectors
        dir = normalize(rec.world_normal + sample_unit_sphere(u1, u2));
      } else {
        escaped = true;
        break;
      }
    }

    acc += (escaped) ? vec3(1.0) : vec3(0.0);
  }

  vec3 color = acc / n_samples;

  if (sample_index == 0) {
    imageStore(image, pixel, vec4(color, 1.0));
  } else {
    vec3 prev = imageLoad(image, pixel).rgb;
    vec3 acc = mix(prev, color, 1.0 / float(sample_index + 1));
    imageStore(image, pixel, vec4(acc, 1.0));
  }
}
