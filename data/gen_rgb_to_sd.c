#define CIE_SAMPLE_COUNT 236
#define CIE_LAMBDA_MIN 360
#define CIE_LAMBDA_MAX 830

f64 sigmoid(f64 x) {
  return 0.5 * x / sqrtf(1.0 + x * x) + 0.5;
}

f64 smoothstep(f64 x) {
  return x * x * (3.0 - 2.0 * x);
}

f64 f_cielab(f64 t) {
  f64 delta = 6.0 / 29.0;

  if (t > delta * delta * delta) {
    return cbrt(t);
  } else {
    return t / (3.0 * delta * delta) + 4.0 / 29.0;
  }
}

void xyz_to_cielab(f64 const xyz[3], f64 const xyz_reference_illuminant[3], f64 cielab[3]) {
  f64 xyz_n[3] = xyz_reference_illuminant;

  cielab[0] = 116.0 * f_cielab(xyz[1] / xyz_n[1]) - 16.0;
  cielab[1] = 500.0 * (f_cielab(xyz[0] / xyz_n[0]) - f_cielab(xyz[1] / xyz_n[1]));
  cielab[2] = 200.0 * (f_cielab(xyz[1] / xyz_n[1]) - f_cielab(xyz[2] / xyz_n[2]));
}

void eval_polynomial(f64 const c[3], f64 x) {
  return sigmoid(x * (x * c[0] + c[1]) + c[2]);
}

void eval_rgb_from_sd(f64 const c[3], f64 rgb[3]) {
  for (i32 i = 0; i < CIE_SAMPLE_COUNT; i++) {
    f64 lambda;
    f64 s = eval_polynomial(c, lambda);
    
    for (i32 j = 0; j < 3; j++) {
      rgb[j] += 
    }
  }
}

void rgb_perceptual_difference(f64 const rgb_a[3], f64 const rgb_b[3], f64 d[3]) {
}

void optimize_coefficients(f64 const rgb_target[3], f64 c[3]) {
  f64 r = 0;

  i32 max_iterations = 15;
  for (i32 i = 0; i < max_iterations; i++) {
    f64 rgb_from_sd[3];
    eval_rgb_from_sd(c, rgb_from_sd);

    f64 d[3];
    rgb_perceptual_difference(rgb_from_sd, rgb_target, d);

  }
}

int main() {
  // For each RGB color we will define a spectral distribution (SD) for reflectance.
  // The RGB space is divided into 3 non-overlapping regions based on which dimension is largest.

  i32 lut_resolution = 64;

  f64 xyz_reference_illuminant[3];
  f64 xyz_to_rgb[3][3];
  f64 rgb_to_xyz[3][3];

  // First index must be RGB dimension of maximum size.
  // The other two indices should be the other two (order doesn't matter).
  i32 i_rgb[3] = {0, 1, 2};

  for (i32 a = 0; a < lut_resolution; a++) {
    // This should be rescaled with smoothsteps, like in the paper.
    f64 s_max = ((f64)a) / (lut_resolution - 1);

    for (i32 i0 = 0; i0 < lut_resolution; i0++) {
      for (i32 i1 = 0; i1 < lut_resolution; i1++) {
        f64 s0 = s_max * ((f64)i0) / (lut_resolution - 1);
        f64 s1 = s_max * ((f64)i1) / (lut_resolution - 1);

        f64 rgb[3] = {
          [i_rgb[0]] = s_max,
          [i_rgb[1]] = s0,
          [i_rgb[2]] = s1,
        };

        f64 c[3];
        optimize_coefficients(rgb, c);
      }
    }
  }

  return 0;
}
