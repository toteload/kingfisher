#ifndef COLORSPACE_H_INCLUDED
#define COLORSPACE_H_INCLUDED

#include "kingfisher_core.h"
#include "vec3.h"

extern float const cie_xyz_x[256];
extern float const cie_xyz_y[256];
extern float const cie_xyz_z[256];

// Spectral samples are stored as an 8-bit int wavelength and a 32-bit float power.
// To get the true wavelength in nm multiply by 2 and add 360.
// A stored wavelength of 140 represents a wavelength of 2 * 140 + 360 = 640 nm.

inline vec3 spectral_to_xyz(u8 wavelength) {
  return (vec3){ cie_xyz_x[wavelength], cie_xyz_y[wavelength], cie_xyz_z[wavelength], };
}

// Normalize XYZ based on a reference black point and white point
inline vec3 normalize_xyz(vec3 xyz)
{
  f32 black[3] = { 0.1901f, 0.2f, 0.2178f, };
  f32 white[3] = { 76.04f, 80.0f, 87.12f, };

  return (vec3){
    (xyz.e[0] - black[0]) / (white[0] - black[0]) * (white[0] / white[1]),
    (xyz.e[1] - black[1]) / (white[1] - black[1]),
    (xyz.e[2] - black[2]) / (white[2] - black[2]) * (white[2] / white[1]),
  };
}

inline vec3 normalized_xyz_to_linear_rgb(vec3 nxyz) {
  return (vec3){
    clamp(0.0f, 1.0f,  3.2406255f * nxyz.x - 1.5372080f * nxyz.y - 0.4986286f * nxyz.z),
    clamp(0.0f, 1.0f, -0.9689307f * nxyz.x + 1.8757561f * nxyz.y + 0.0415175f * nxyz.z),
    clamp(0.0f, 1.0f,  0.0557101f * nxyz.x - 0.2040211f * nxyz.y + 1.0569959f * nxyz.z),
  };
}

inline vec3 linear_rgb_to_srgb(vec3 rgb) {
  vec3 srgb;
  for (i32 i = 0; i < 3; i++) {
    if (rgb.e[i] <= 0.0031308f) {
      srgb.e[i] = 12.92f * rgb.e[i];
    } else {
      srgb.e[i] = 1.055f * powf(rgb.e[i], 1.0f / 2.4f) - 0.055f;
    }
  }
  return srgb;
}

#endif // COLORSPACE_H_INCLUDED
