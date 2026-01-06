#include "kingfisher_core.h"
#include <stdio.h>

// NOTE: This is not really a test on its own.

i32 main(i32 argc, char const *args[]) {
  // Used to generate points that are plotted with Desmos for manual inspection.
  i32 n = 20;
  printf("[");
  for (i32 i = 0; i < n; i++) {
    for (i32 j = 0; j < n; j++) {
      f32 ii = ((f32)i+0.5f) / n;
      f32 jj = ((f32)j+0.5f) / n;
      vec3 s = sample_unit_hemisphere(ii, jj);
      printf("(%f, %f, %f),", s.e[0], s.e[1], s.e[2]);
    }
  }
  printf("]");

  return 0;
}
