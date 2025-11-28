#include <stdint.h>
#include <math.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef  int32_t i32;
typedef float f32;
typedef double f64;

extern float const cie_xyz_x[256];
extern float const cie_xyz_y[256];
extern float const cie_xyz_z[256];

// Spectral samples are stored as an 8-bit int wavelength and a 32-bit float power.
// To get the true wavelength in nm multiply by 2 and add 360.
// A stored wavelength of 140 represents a wavelength of 2 * 140 + 320 = 600 nm.

void update_texture(void *buffer, i32 width, i32 height, i32 pitch) {
  u8 *pixels = buffer;
  for (i32 y = 0; y < height; y++) {
    for (i32 x = 0; x < width; x++) {
      pixels[y * pitch + x * 4 + 0] = 0x00; // Blue
      pixels[y * pitch + x * 4 + 1] = 0x00; // Green
      pixels[y * pitch + x * 4 + 2] = 0xff; // Red
    }
  }
}

typedef struct Vector3 {
  f32 x;
  f32 y;
  f32 z;
} Vector3;

typedef struct Ray {
  Vector3 origin;
  f32 min_t;
  Vector3 dir;
  f32 max_t;
} Ray;

typedef struct Sphere {
  Vector3 pos;
  f32 radius;
} Sphere;

#define F32_NO_HIT INFINITY

void generate_primary_ray(Ray *ray, f32 u, f32 v) {
}

f32 ray_sphere_intersect(Ray const *ray, Sphere const *sphere) {
  return F32_NO_HIT;
}

// 1. Generate primary rays
// 2. Shoot primary rays into scene to get samples
//   a. Intersect rays with geometry
//   b. Generate and trace secondary rays
//   -  Handle material interaction 
// 3. Convert wavelength samples to XYZ spectrum format
// 4. Convert XYZ format to RGB
// 5. 

inline void spectral_to_xyz(u8 wavelength, f32 *xyz) {
  xyz[0] = cie_xyz_x[wavelength];
  xyz[1] = cie_xyz_y[wavelength];
  xyz[2] = cie_xyz_z[wavelength];
}

// Normalize XYZ based on a reference black point and white point
inline void normalize_xyz(f32 const *xyz, f32 *nxyz)
{
  f32 black[3] = { 0.1901f, 0.2f, 0.2178f, };
  f32 white[3] = { 76.04f, 80.0f, 87.12f, };

  nxyz[0] = (xyz[0] - black[0]) / (white[0] - black[0]) * (white[0] / white[1]);
  nxyz[1] = (xyz[1] - black[1]) / (white[1] - black[1]);
  nxyz[2] = (xyz[2] - black[2]) / (white[2] - black[2]) * (white[2] / white[1]);
}

#define max(a,b) (((a) < (b)) ? (b) : (a))
#define min(a,b) (((a) > (b)) ? (b) : (a))

inline f32 clampf32(f32 lo, f32 hi, f32 t) {
  return min(max(lo, t), hi);
}

inline void normalized_xyz_to_linear_rgb(f32 const *xyz, f32 *rgb) {
  rgb[0] = clampf32(0.0f, 1.0f,  3.2406255f * xyz[0] - 1.5372080f * xyz[1] - 0.4986286f * xyz[2]);
  rgb[1] = clampf32(0.0f, 1.0f, -0.9689307f * xyz[0] + 1.8757561f * xyz[1] + 0.0415175f * xyz[2]);
  rgb[2] = clampf32(0.0f, 1.0f,  0.0557101f * xyz[0] - 0.2040211f * xyz[1] + 1.0569959f * xyz[2]);
}

inline void linear_rgb_to_srgb(f32 const *rgb, f32 *srgb) {
  for (i32 i = 0; i < 3; i++) {
    if (rgb[i] <= 0.0031308f) {
      srgb[i] = 12.92f * rgb[i];
    } else {
      srgb[i] = 1.055f * powf(rgb[i], 1.0f / 2.4f) - 0.055f;
    }
  }
}

#if 0
struct SampleGrid {
  u32 width, height;
  u8 *wavelengths;
  f32 *powers;
};

void samplegrid_create(SampleGrid *grid, u32 width, u32 height);
void samplegrid_destroy(SampleGrid *grid);
#endif

int main(int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    return -1;
  }

  int width = 1024;
  int height = 480;

  SDL_Window *window;
  SDL_Renderer *renderer;
  if (!SDL_CreateWindowAndRenderer("Kingfisher", width, height, 0, &window, &renderer)) {
    return 1;
  }

  SDL_Texture *screen;
  screen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);

  Vector3 cam_pos = { 0.0f, 0.0f, 5.0f, };
  Vector3 cam_dir = { 0.0f, 0.0f, -1.0f, };

  Sphere sphere = { .pos = { 0.0f, 0.0f, 0.0f, }, .radius = 1.0f, };

  f32 *powers = malloc(width * height * sizeof(f32));
  u8 *wavelengths = malloc(width * height);

  // This can function as an accumulator
  f32 *xyz = malloc(width * height * 3 * sizeof(f32));

  u32 done = 0;
  while (!done) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        goto exit;
      }

      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
        goto exit;
      }
    }

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(renderer);

    memset(powers, 0, width * height * sizeof(f32));
    memset(wavelengths, 0, width * height);
    memset(xyz, 0, width * height * 3 * sizeof(f32));

    // Trace our rays and save the power and wavelengths
    for (i32 y = 0; y < height; y++) {
      for (i32 x = 0; x < width; x++) {
        f32 u = 2.0f * ((f32)x) / width - 1.0f;
        f32 v = 2.0f * ((f32)y) / height - 1.0f;

        i32 i = y * width + x;

        powers[i] = 10.0f;
        wavelengths[i] = (u8)(((f32)x) / width * 255.5f);
      }
    }

    // Convert the wavelength+power samples to XYZ and store.
    for (i32 y = 0; y < height; y++) {
      for (i32 x = 0; x < width; x++) {
        i32 i = y * width + x;

        f32 s[3];
        spectral_to_xyz(wavelengths[i], s);

        for (i32 j = 0; j < 3; j++) {
          xyz[i * 3 + j] += powers[i] * s[j];
        }
      }
    }

    void *buffer;
    int pitch;
    SDL_LockTexture(screen, NULL, &buffer, &pitch);
    u8 *pixels = buffer;

    for (i32 y = 0; y < height; y++) {
      for (i32 x = 0; x < width; x++) {
        i32 i = y * width + x;

        f32 nxyz[3];
        normalize_xyz(xyz + i * 3, nxyz);

        f32 rgb[3];
        normalized_xyz_to_linear_rgb(nxyz, rgb);

        f32 srgb[3];
        linear_rgb_to_srgb(rgb, srgb);

        pixels[y * pitch + x * 4 + 2] = (u8)(srgb[0] * 255.0f);
        pixels[y * pitch + x * 4 + 1] = (u8)(srgb[1] * 255.0f);
        pixels[y * pitch + x * 4 + 0] = (u8)(srgb[2] * 255.0f);
      }
    }

    SDL_UnlockTexture(screen);
    SDL_RenderTexture(renderer, screen, NULL, NULL);

    SDL_RenderPresent(renderer);
  }

exit:
  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  SDL_Quit();

  return 0;
}
