"""
Generate RGB to spectral reflectance coefficients using the method from:
"A Low-Dimensional Function Space for Efficient Spectral Upsampling"
by Wenzel Jakob and Johannes Hanika (2019)

The spectral model is: f(lambda) = S(c0*lambda^2 + c1*lambda + c2)
where S(x) = 0.5 + x / (2 * sqrt(1 + x^2))

This produces smooth spectra bounded in [0, 1] (energy conserving).
"""

import csv
import numpy as np
from scipy.optimize import minimize
from typing import Tuple
import struct
import sys

# Wavelength range (nm)
LAMBDA_MIN = 360
LAMBDA_MAX = 830
LAMBDA_STEP = 2  # Match the 2nm steps used in the renderer's wavelength encoding

# Grid resolution for coefficient tables
GRID_RES = 64

# CIE 1931 2-degree observer data
CIE_DATA = None

# sRGB to XYZ matrix (D65 white point)
SRGB_TO_XYZ = np.array([
    [0.4124564, 0.3575761, 0.1804375],
    [0.2126729, 0.7151522, 0.0721750],
    [0.0193339, 0.1191920, 0.9503041]
])

# XYZ to sRGB matrix (D65 white point)
XYZ_TO_SRGB = np.array([
    [ 3.2404542, -1.5371385, -0.4985314],
    [-0.9692660,  1.8760108,  0.0415560],
    [ 0.0556434, -0.2040259,  1.0572252]
])

# D65 white point
D65_WHITE = np.array([0.95047, 1.0, 1.08883])


def load_cie_data(filepath: str) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """Load CIE 1931 2-degree observer color matching functions."""
    wavelengths = []
    x_bar = []
    y_bar = []
    z_bar = []

    with open(filepath, 'r') as f:
        reader = csv.reader(f)
        for row in reader:
            wl = int(row[0])
            if LAMBDA_MIN <= wl <= LAMBDA_MAX:
                wavelengths.append(wl)
                x_bar.append(float(row[1]))
                y_bar.append(float(row[2]))
                z_bar.append(float(row[3]))

    return (np.array(wavelengths), np.array(x_bar), np.array(y_bar), np.array(z_bar))


def sigmoid(x: np.ndarray) -> np.ndarray:
    """Compute the sigmoid function S(x) = 0.5 + x / (2 * sqrt(1 + x^2))"""
    return 0.5 + x / (2.0 * np.sqrt(1.0 + x * x))


def eval_spectrum(coeffs: np.ndarray, wavelengths: np.ndarray) -> np.ndarray:
    """Evaluate the spectral reflectance model at given wavelengths."""
    c0, c1, c2 = coeffs
    x = c0 * wavelengths**2 + c1 * wavelengths + c2
    return sigmoid(x)


def spectrum_to_xyz(spectrum: np.ndarray, wavelengths: np.ndarray,
                    x_bar: np.ndarray, y_bar: np.ndarray, z_bar: np.ndarray) -> np.ndarray:
    """Convert a spectrum to XYZ using CIE color matching functions.

    Uses composite Simpson's 3/8 rule for integration.
    """
    # Simple trapezoidal integration (good enough for smooth spectra)
    dlambda = wavelengths[1] - wavelengths[0]

    X = np.trapezoid(spectrum * x_bar, dx=dlambda)
    Y = np.trapezoid(spectrum * y_bar, dx=dlambda)
    Z = np.trapezoid(spectrum * z_bar, dx=dlambda)

    return np.array([X, Y, Z])


def xyz_to_linear_srgb(xyz: np.ndarray) -> np.ndarray:
    """Convert XYZ to linear sRGB."""
    return XYZ_TO_SRGB @ xyz


def linear_srgb_to_xyz(rgb: np.ndarray) -> np.ndarray:
    """Convert linear sRGB to XYZ."""
    return SRGB_TO_XYZ @ rgb


def smoothstep(x: float) -> float:
    """Compute smoothstep function for alpha discretization."""
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3.0 - 2.0 * x)


def compute_alpha(i: int, n: int = GRID_RES) -> float:
    """Compute alpha value for the i-th slice using double smoothstep."""
    t = i / (n - 1)
    return smoothstep(smoothstep(t))


def objective_function(coeffs: np.ndarray, target_rgb: np.ndarray,
                       wavelengths: np.ndarray, x_bar: np.ndarray,
                       y_bar: np.ndarray, z_bar: np.ndarray,
                       normalization: float) -> float:
    """Compute the color error between target RGB and spectrum RGB.

    Returns squared CIE76 Delta E (simplified as Euclidean distance in Lab-like space).
    """
    spectrum = eval_spectrum(coeffs, wavelengths)
    xyz = spectrum_to_xyz(spectrum, wavelengths, x_bar, y_bar, z_bar)

    # Normalize XYZ (illuminant E normalization for reflectance)
    xyz = xyz / normalization

    rgb = xyz_to_linear_srgb(xyz)

    # Compute error in linear RGB space
    diff = rgb - target_rgb
    return np.sum(diff * diff)


def find_coefficients(target_rgb: np.ndarray, wavelengths: np.ndarray,
                      x_bar: np.ndarray, y_bar: np.ndarray, z_bar: np.ndarray,
                      initial_guess: np.ndarray = None) -> np.ndarray:
    """Find coefficients (c0, c1, c2) that produce the target RGB color."""

    # Compute normalization factor (integral of y_bar for illuminant E)
    dlambda = wavelengths[1] - wavelengths[0]
    normalization = np.trapezoid(y_bar, dx=dlambda)

    if initial_guess is None:
        initial_guess = np.array([0.0, 0.0, 0.0])

    result = minimize(
        objective_function,
        initial_guess,
        args=(target_rgb, wavelengths, x_bar, y_bar, z_bar, normalization),
        method='L-BFGS-B',
        options={'ftol': 1e-12, 'gtol': 1e-10, 'maxiter': 1000}
    )

    return result.x


def generate_coefficient_table(wavelengths: np.ndarray, x_bar: np.ndarray,
                               y_bar: np.ndarray, z_bar: np.ndarray,
                               region: int) -> np.ndarray:
    """Generate coefficient table for one of the three RGB regions.

    Region 0: R is max -> parameterize by (G/R, B/R, R)
    Region 1: G is max -> parameterize by (B/G, R/G, G)
    Region 2: B is max -> parameterize by (R/B, G/B, B)
    """
    table = np.zeros((GRID_RES, GRID_RES, GRID_RES, 3), dtype=np.float32)

    total = GRID_RES * GRID_RES * GRID_RES
    count = 0

    for i in range(GRID_RES):
        alpha = compute_alpha(i)  # The max component value

        for j in range(GRID_RES):
            u = j / (GRID_RES - 1)  # First normalized component

            for k in range(GRID_RES):
                v = k / (GRID_RES - 1)  # Second normalized component

                # Construct RGB based on region
                if region == 0:  # R is max
                    rgb = np.array([alpha, u * alpha, v * alpha])
                elif region == 1:  # G is max
                    rgb = np.array([v * alpha, alpha, u * alpha])
                else:  # B is max
                    rgb = np.array([u * alpha, v * alpha, alpha])

                # Use previous solution as initial guess for continuity
                if k > 0:
                    initial = table[i, j, k-1]
                elif j > 0:
                    initial = table[i, j-1, k]
                elif i > 0:
                    initial = table[i-1, j, k]
                else:
                    initial = None

                coeffs = find_coefficients(rgb, wavelengths, x_bar, y_bar, z_bar, initial)
                table[i, j, k] = coeffs

                count += 1
                if count % 1000 == 0:
                    print(f"  Region {region}: {count}/{total} ({100*count/total:.1f}%)", file=sys.stderr)

    return table


def generate_c_header(tables: list, output_path: str):
    """Generate C header file with coefficient tables."""

    header = '''// This file was generated by gen_rgb_to_spectral_reflectance.py
// Implements the method from "A Low-Dimensional Function Space for Efficient Spectral Upsampling"
// by Wenzel Jakob and Johannes Hanika (2019)
//
// Three 64x64x64 tables of (c0, c1, c2) coefficients, one for each RGB region:
// - Table 0: R is the maximum component, indexed by (alpha, G/R, B/R)
// - Table 1: G is the maximum component, indexed by (alpha, B/G, R/G)
// - Table 2: B is the maximum component, indexed by (alpha, R/B, G/B)
//
// alpha = smoothstep(smoothstep(i/63)) where i is the first index
//
// To evaluate: f(lambda) = 0.5 + x / (2 * sqrt(1 + x^2))
// where x = c0 * lambda^2 + c1 * lambda + c2

#ifndef RGB_TO_SPECTRAL_H_INCLUDED
#define RGB_TO_SPECTRAL_H_INCLUDED

#include "kingfisher_core.h"

#define RGB_TO_SPECTRAL_GRID_RES 64

'''

    for region in range(3):
        table = tables[region]
        header += f'// Region {region}: {"RGB"[region]} is maximum component\n'
        header += f'static float const rgb_to_spectral_coeffs_{region}[{GRID_RES}][{GRID_RES}][{GRID_RES}][3] = {{\n'

        for i in range(GRID_RES):
            header += '  {\n'
            for j in range(GRID_RES):
                header += '    {\n'
                for k in range(GRID_RES):
                    c0, c1, c2 = table[i, j, k]
                    header += f'      {{ {c0:15.8e}f, {c1:15.8e}f, {c2:15.8e}f }},\n'
                header += '    },\n'
            header += '  },\n'
        header += '};\n\n'

    header += '''
// Helper function: compute smoothstep
static inline f32 smoothstep_f32(f32 x) {
  x = clamp(0.0f, 1.0f, x);
  return x * x * (3.0f - 2.0f * x);
}

// Helper function: trilinear interpolation
static inline void trilinear_interp(float const table[64][64][64][3],
                                    f32 u, f32 v, f32 w, f32 *out) {
  // Clamp to valid range
  u = clamp(0.0f, 1.0f, u);
  v = clamp(0.0f, 1.0f, v);
  w = clamp(0.0f, 1.0f, w);

  // Scale to grid indices
  f32 uf = u * 63.0f;
  f32 vf = v * 63.0f;
  f32 wf = w * 63.0f;

  i32 u0 = (i32)uf;
  i32 v0 = (i32)vf;
  i32 w0 = (i32)wf;

  i32 u1 = min(u0 + 1, 63);
  i32 v1 = min(v0 + 1, 63);
  i32 w1 = min(w0 + 1, 63);

  f32 ud = uf - u0;
  f32 vd = vf - v0;
  f32 wd = wf - w0;

  for (i32 c = 0; c < 3; c++) {
    f32 c000 = table[u0][v0][w0][c];
    f32 c001 = table[u0][v0][w1][c];
    f32 c010 = table[u0][v1][w0][c];
    f32 c011 = table[u0][v1][w1][c];
    f32 c100 = table[u1][v0][w0][c];
    f32 c101 = table[u1][v0][w1][c];
    f32 c110 = table[u1][v1][w0][c];
    f32 c111 = table[u1][v1][w1][c];

    f32 c00 = c000 * (1 - wd) + c001 * wd;
    f32 c01 = c010 * (1 - wd) + c011 * wd;
    f32 c10 = c100 * (1 - wd) + c101 * wd;
    f32 c11 = c110 * (1 - wd) + c111 * wd;

    f32 c0 = c00 * (1 - vd) + c01 * vd;
    f32 c1 = c10 * (1 - vd) + c11 * vd;

    out[c] = c0 * (1 - ud) + c1 * ud;
  }
}

// Convert linear sRGB to spectral reflectance coefficients
// rgb: linear sRGB values in [0, 1]
// out: array of 3 coefficients (c0, c1, c2)
static inline void rgb_to_spectral_coeffs(vec3s rgb, f32 *out) {
  // Handle black specially
  f32 max_comp = fmaxf(fmaxf(rgb.r, rgb.g), rgb.b);
  if (max_comp < 1e-6f) {
    // Black: return coefficients that produce ~0 reflectance
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = -1e6f;  // Large negative -> sigmoid approaches 0
    return;
  }

  // Inverse smoothstep to find alpha index
  // We need to solve smoothstep(smoothstep(t)) = max_comp for t
  // This is approximated by inverting numerically
  f32 alpha = max_comp;

  // Binary search for the alpha index
  f32 t_lo = 0.0f, t_hi = 1.0f;
  for (i32 iter = 0; iter < 20; iter++) {
    f32 t_mid = 0.5f * (t_lo + t_hi);
    f32 ss = smoothstep_f32(smoothstep_f32(t_mid));
    if (ss < alpha) {
      t_lo = t_mid;
    } else {
      t_hi = t_mid;
    }
  }
  f32 t = 0.5f * (t_lo + t_hi);

  // Determine which region and compute normalized coordinates
  i32 region;
  f32 u, v;

  if (rgb.r >= rgb.g && rgb.r >= rgb.b) {
    region = 0;
    u = rgb.g / max_comp;
    v = rgb.b / max_comp;
  } else if (rgb.g >= rgb.r && rgb.g >= rgb.b) {
    region = 1;
    u = rgb.b / max_comp;
    v = rgb.r / max_comp;
  } else {
    region = 2;
    u = rgb.r / max_comp;
    v = rgb.g / max_comp;
  }

  // Look up and interpolate coefficients
  switch (region) {
    case 0:
      trilinear_interp(rgb_to_spectral_coeffs_0, t, u, v, out);
      break;
    case 1:
      trilinear_interp(rgb_to_spectral_coeffs_1, t, u, v, out);
      break;
    case 2:
      trilinear_interp(rgb_to_spectral_coeffs_2, t, u, v, out);
      break;
  }
}

#endif // RGB_TO_SPECTRAL_H_INCLUDED
'''

    with open(output_path, 'w') as f:
        f.write(header)


def generate_binary_tables(tables: list, output_path: str):
    """Generate binary file with coefficient tables for faster loading."""
    with open(output_path, 'wb') as f:
        # Write header
        f.write(b'SPEC')  # Magic number
        f.write(struct.pack('I', GRID_RES))  # Grid resolution
        f.write(struct.pack('I', 3))  # Number of regions

        # Write tables
        for table in tables:
            f.write(table.tobytes())


def main():
    import os

    # Get directory of this script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    cie_path = os.path.join(script_dir, 'CIE_xyz_1931_2deg.csv')

    print("Loading CIE color matching functions...", file=sys.stderr)
    wavelengths, x_bar, y_bar, z_bar = load_cie_data(cie_path)

    print(f"Wavelength range: {wavelengths[0]} - {wavelengths[-1]} nm", file=sys.stderr)
    print(f"Number of samples: {len(wavelengths)}", file=sys.stderr)

    tables = []

    for region in range(3):
        print(f"\nGenerating coefficients for region {region}...", file=sys.stderr)
        table = generate_coefficient_table(wavelengths, x_bar, y_bar, z_bar, region)
        tables.append(table)

    # Generate C header
    output_path = os.path.join(script_dir, '..', 'src', 'rgb_to_spectral.h')
    print(f"\nWriting C header to {output_path}...", file=sys.stderr)
    generate_c_header(tables, output_path)

    # Also save binary for faster loading during development
    binary_path = os.path.join(script_dir, 'rgb_to_spectral.bin')
    print(f"Writing binary data to {binary_path}...", file=sys.stderr)
    generate_binary_tables(tables, binary_path)

    print("\nDone!", file=sys.stderr)


if __name__ == "__main__":
    main()
