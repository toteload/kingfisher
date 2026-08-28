# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Kingfisher is a spectral path tracer written in C23. Light is carried as wavelength + power rather
than RGB, so wavelength-dependent phenomena (dispersion) can be simulated; results are converted to
sRGB through CIE XYZ for display. It is a work-in-progress personal project. The long-term goal
stated in the README is hardware-accelerated ray tracing via Vulkan; today it is a CPU tracer,
multithreaded with one worker thread per logical core.

## Build and run

The build is clang + ninja, driven by a Python generator. From the repo root:

```
python build.py            # regenerates build.ninja, then runs ninja
python build.py -v         # extra args are forwarded to ninja
out/kingfisher.exe         # run (start from the repo root; asset paths are relative)
```

- `build.py` writes `build.ninja` (gitignored) and shells out to `ninja`. To add a source file, add
  it to the `inputs` list in `build.py` — there is no globbing.
- Objects land in `out/` with path-flattened names (`src/bvh.c` -> `out/src__bvh.c.o`).
- Windows-only: `build.py` raises on any other platform.
- `build.bat` is a stale MSVC-era wrapper (it calls the deleted `initcl.bat`); use `python build.py`.
- `out/` must contain `SDL3.dll`, `embree4.dll`, `tbb12.dll`, `cglm.dll` next to the exe. The build
  does not copy them — they are already there, and `out/` is gitignored.
- Warning flags matter: `-Wall -Wextra -Wpedantic`, plus `-Werror=switch` (every enum value must be
  handled) and `-Werror=incompatible-pointer-types`. `-Wsign-conversion` is enabled and then
  explicitly disabled again ("temporarily"). Optimization is `-O2 -march=native`.

There is no test runner. `test/*.test.c` are not in the build, are point-dump programs for manual
plotting in Desmos rather than assertions, and still use the removed `vec3` API — treat them as dead.

Running under a debugger: `main()` calls `__debugbreak()` when `IsDebuggerPresent()`, so the process
intentionally breaks at startup, before SDL initialization.

## Architecture

### Frame loop and threading (`src/main.c`, `src/worker.c`)

`main.c` owns everything: window, scene load, BVH build, worker threads, accumulation, UI. There is
no scene or renderer abstraction.

Workers are created once and parked on a semaphore pair. Each `ThreadWork` owns a horizontal band of
rows (`y_start..y_end`), its own `Rng`, and pointers to the shared output buffers; the bands are
disjoint, so no locking is needed. Per frame the main thread polls `done_sem` with
`SDL_TryWaitSemaphore`; only when *all* workers are done does it consume the buffers, push the new
camera basis / perspective / buffer selection into every `ThreadWork`, clear the buffers, and signal
`start_sem`. Shutdown sets `should_exit` and signals `start_sem` once per thread.

Two consequences when changing render parameters: anything a worker reads must be copied into
`ThreadWork` during that "all done" window, and any parameter change that invalidates accumulated
samples must reset `sample_count` — currently only camera movement does, see the TODO in `main.c`.

Accumulation is a running average in `acc_xyz` (lerp by `1/sample_count`), not a running sum. Each
worker writes one sample per pixel per frame into `xyz`, which is zeroed every frame.

`main.c` also has an unfinished refactor sitting beside it: `src/app.c` / `src/app.h` extract this
setup into an `App` struct, but they are not in the build and do not compile. Either finish or ignore
them; they do not reflect current behavior.

### Tracing (`src/worker.c`)

Two render modes selected by `ThreadWork.buffer` (`BUFFER_LIGHT` / `BUFFER_NORMALS`, chosen in the
debug UI). The light path is:

- One primary ray per pixel through the pixel *center* — there is currently no jitter and therefore
  no antialiasing.
- Up to 4 bounces, cosine-weighted diffuse via the `normalize(n + sample_unit_sphere())` trick, with
  the normal flipped to face the ray and the origin offset by `0.001 * n`.
- `strength` accumulates `dot(n, dir)` per bounce. A path that escapes the scene keeps its strength
  (the environment is the only light source); a path that never escapes contributes 0.
- The escaped energy becomes a single hardcoded wavelength (`120`, i.e. 600 nm) times a hardcoded
  power, converted to XYZ via the LUT and added to the pixel. There are no materials and no light
  sampling yet — surface color, emission, and NEE are all absent.

### BVH (`src/bvh.c`)

Embree is used **only as a BVH builder**, not as a tracer. `rtcBuildBVH` is called with custom
callbacks that allocate a `BvhBuildNode` tree (binary, max leaf size 4, high build quality) out of
Embree's thread-local allocator. Traversal is hand-written: `embree_bvh_intersect` walks that node
tree with a 64-deep explicit stack, slab-tests AABBs (`aabb_intersect`) and runs Möller–Trumbore
(`ray_triangle_intersect` in `kingfisher_core.h`) at the leaves. `rtcIntersect` is never called.

A second, flattened layout also exists: `Bvh` (SoA `offset` / `meta` / `bounds` / `prims`, where the
high bit of `meta` marks an internal node and the low 7 bits hold the child or primitive count), plus
`bvh_build_from_embree_bvh` and `bvh_intersect`. `main.c` builds it, but the workers still traverse
the pointer-based `EmbreeBvh`; the flattened path is the intended replacement and is currently
unused.

Both traversals push children unordered — no near/far sorting, so no early termination on distance.

### Color pipeline (`src/colorspace.h`, `src/cie_xyz_lut.c`)

Wavelengths are stored in a `u8` with the encoding `nm = stored * 2 + 360`, covering 360–830 nm in
2 nm steps (236 significant entries, LUT padded to 256). That encoding exists so `spectral_to_xyz()`
is a direct table index.

Per frame: `spectral_to_xyz` -> accumulate XYZ -> `normalize_xyz` (hardcoded sRGB reference black and
white points) -> `normalized_xyz_to_linear_rgb` (sRGB matrix, clamped) -> `linear_rgb_to_srgb`
(gamma). `xyz_to_srgb_pixels` in `main.c` also flips Y while writing into the SDL texture.

`cie_xyz_lut.c` is generated by `data/gen_cie_xyz_lut.py` from `data/CIE_xyz_1931_2deg.csv` — edit the
generator, not the table. That script currently has `print_ppm()` enabled and `print_c_lut()`
commented out in `__main__`; swap them to regenerate the C table.

### RGB to spectral reflectance (in progress)

Current work is toward Jakob & Hanika 2019 spectral upsampling (the paper PDF is in the repo root).
Landed so far: `eval_spectral_reflectance()` in `colorspace.h`, which evaluates the
sigmoid-of-quadratic model `S(c0*l^2 + c1*l + c2)`. Not yet landed: the coefficient tables.
`data/gen_rgb_to_spectral_reflectance.py` (numpy/scipy, L-BFGS-B fit over a 64^3 grid per RGB region)
is the working generator and emits `src/rgb_to_spectral.h`. `data/gen_rgb_to_sd.c` is an abandoned
half-written C version of the same idea and does not compile.

### Scene loading

`read_obj_triangles` (fast_obj) and `read_fbx_triangles` (ufbx) both flatten a file into a bare
`Triangle` array — positions only, no normals, UVs, materials, or per-object grouping. Both assert
that every face is a triangle; `data/models/TriangulateOBJ.exe` is there for pre-triangulating OBJs.
The active scene is picked with `#if 1` / `#if 0` blocks in `main()`; Sponza is current and is scaled
by 0.1 via `transform_triangles`.

### UI (`src/ui.c`, `src/ui.h`)

Nuklear on the SDL3 renderer backend. `ui.h` sets the Nuklear configuration macros (routing
assert/memset/vsnprintf to SDL) and must be included before any Nuklear header. Note that
`ui_render()` calls `nk_input_end()` *before* rendering and `nk_input_begin()` after, so the input
block spans the frame boundary. The debug window itself (`draw_debug_ui`) lives in `main.c` and drives
camera position, perspective kind, FOV, and buffer selection.

## Conventions

- Types are the short forms from `kingfisher_core.h` (`u32`, `f32`, ...); `F32_NO_HIT` (`FLT_MAX`) is
  the miss sentinel in `HitRecord.t` and `aabb_intersect`.
- Vector math is cglm's struct API (`vec3s`, `glms_vec3_*`). `src/vec3.h` is the superseded
  hand-rolled version, still present but unused by the build — do not add to it.
- Small helpers are `inline` in headers (`kingfisher_core.h`, `colorspace.h`) rather than split across
  a `.c`.
- `clamp(lo, hi, t)` takes the bounds first and the value last — not the usual order.
- Y is up; camera orientation is stored as pitch/yaw (`pitch_yaw_to_vec3`), pitch clamped to +-pi/2.
- Dependencies are vendored under `ext/` (SDL3, Embree 4.4.0, cglm, nuklear, fast_obj, ufbx,
  xoshiro128+/splitmix64) and included via `-Iext -Iext/SDL -Iext/embree-4.4.0/include -Iext/cglm`.
- `notes.txt` collects the radiometry/photometry background and the data sources behind the LUT.

## Controls

WASD to move, Q/E down/up, hold Shift for 4x speed, arrow keys to look. ESC or closing the window
quits. The image sharpens the longer the camera stays still, since samples accumulate and reset on
movement.
