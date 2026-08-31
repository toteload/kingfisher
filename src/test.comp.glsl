#version 460

layout(local_size_x=16, local_size_y=8, local_size_z=1) in;

layout(binding=0, set=0) buffer storage_buffer {
  float image[];
};

void main() {
  const uvec2 dim = uvec2(1280, 960);
  const uvec2 pixel = gl_GlobalInvocationID.xy;

  if ((pixel.x >= dim.x) || (pixel.y >= dim.y)) {
    return;
  }

  const vec3 color = vec3(
    float(pixel.x) / float(dim.x),
    float(pixel.y) / float(dim.y),
    0.0
  );

  uint idx = pixel.x + pixel.y * dim.x;

  image[idx * 3 + 0] = color.r;
  image[idx * 3 + 1] = color.g;
  image[idx * 3 + 2] = color.b;
}
