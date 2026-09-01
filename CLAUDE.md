# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Kingfisher is a spectral path tracer written in C23. Light is carried as wavelength + power rather
than RGB, so wavelength-dependent phenomena (dispersion) can be simulated; results are converted to
sRGB through CIE XYZ for display. It is a work-in-progress personal project.

**The project is mid-migration.** It began as a multithreaded CPU tracer using Embree as a BVH
builder. Work has now moved to GPU ray tracing via Vulkan (`VK_KHR_ray_query` from a compute shader).
The `vulkan` branch is where this is happening, and `src/vk.c` is the file under active development.

The consequence for anyone reading this codebase: **most of `src/` is orphaned.** See "What is
actually live" below before assuming a file matters.

## Build and run

The build is clang + ninja, driven by a Python generator. Requires the `VULKAN_SDK` environment
variable (used for headers and for `glslc`). Windows-only — `build.py` reads `os.environ['VULKAN_SDK']`
and links `.lib` files directly.

```
python build.py            # regenerates build.ninja, then runs ninja
python build.py -v         # extra args are forwarded to ninja
out/kingfisher.exe         # run from the repo root (see path note below)
```

- `build.py` writes `build.ninja` (gitignored), runs `ninja`, then regenerates `compile_commands.json`.
- To add a source file, add it to the `inputs` list in `build.py`; to add a shader, add it to
  `shaders` with its stage. There is no globbing.
- Objects land in `out/` with path-flattened names (`src/vk.c` -> `out/src__vk.c.o`). Shaders land in
  `out/` under their **basename** (`src/trace_test.comp.glsl` -> `out/trace_test.comp.glsl.spv`).
- Shaders compile with `--target-env=vulkan1.3 --target-spv=spv1.4` (spv1.4 is required for ray query).
- `out/` must contain `SDL3.dll`, `embree4.dll`, `tbb12.dll`, `cglm.dll` next to the exe. The build
  does not copy them — they are already there, and `out/` is gitignored.
- Warning flags matter: `-Wall -Wextra -Wpedantic -Wsign-conversion`, plus `-Werror=switch` (every
  enum value must be handled) and `-Werror=incompatible-pointer-types`.

**Path gotcha:** shaders are loaded relative to `SDL_GetBasePath()` (i.e. next to the exe, in `out/`),
but model files are loaded with a path relative to the current directory (`data/models/...`). Run the
exe from the repo root or the scene load fails.

There is no test runner. `test/*.test.c` are not in the build, are point-dump programs for manual
plotting in Desmos rather than assertions, and use APIs that no longer exist — treat them as dead.

## What is actually live

`main.c` includes only `toteload.h`, `vk.h`, and `model.h`. Everything reachable from those three is
live; everything else is compiled and linked but never called.

**Live:** `main.c`, `vk.c`/`vk.h`, `model.c`/`model.h`, `toteload.c`/`toteload.h`, and the shaders.

**Compiled but unreachable from `main`:** `bvh.c`, `aabb.c`, `camera.c`, `worker.c`, `ui.c`,
`cie_xyz_lut.c`. These are the CPU tracer. They still build (and Embree/TBB are still linked), so
breaking them breaks the build, but nothing runs them. `camera.c` in particular holds the primary-ray
generation math that the GPU path will need to reimplement in GLSL.

**Not in the build at all:** `app.c`/`app.h` (an abandoned refactor), `util.c`, `lu.c`, `vec3.h`
(superseded by cglm — do not add to it), `test.comp.glsl` is built but no longer loaded by anything.

**Two conflicting `Triangle` definitions exist.** `kingfisher_core.h` declares a `union Triangle`
(named `.v0/.v1/.v2` plus `.p[3]`); `model.h` declares a `struct Triangle` with just `.p[3]`. Same
layout, but including both headers in one translation unit is a redefinition error. The Vulkan path
uses the `model.h` one; the CPU path uses the `kingfisher_core.h` one.

## Architecture

### Base layer (`src/toteload.h`, `src/toteload.c`)

A single-header-ish personal base layer that everything sits on. Short integer types (`u32`, `f32`,
`b32`, `usize`), `Null`/`True`/`False`, `Cast(T,x)`, `internal` for `static`, `Count_of`, `Unused(...)`
(variadic, up to 8 args), `Panic()`/`Todo()`/`TodoMsg()` as unimplemented markers.

Memory is arena-based: `Arena` reserves virtual address space and commits incrementally
(`arena_init` with `ArenaOptions{reserve_size, initial_commit_size}`), `arena_push_array` /
`arena_push_one` / `arena_append` to allocate, `arena_scope_begin`/`arena_scope_end` for scoped
resets. `main` creates one 4 MiB scratch arena and threads it into Vulkan setup for shader loading.
There is also a `String` type (pointer + length, `string_lit` for literals) and `Stack(type)` macros.

Note `Clamp(lo, hi, t)` takes the bounds first and the value last — not the usual order.

### Vulkan (`src/vk.c`, `src/vk.h`)

Loaded through **volk** (`volkInitializeCustom` fed from SDL's loader, then `volkLoadInstanceOnly`,
then `volkLoadDevice`). `main.c` includes `<volk.h>` before any SDL Vulkan header.

Split into two objects with separate lifetimes:

- **`kfvk_Graphics`** (`kfvk_create_graphics`) — instance, debug messenger, physical device, one
  universal queue, device, command pool, a single command buffer, and one fence. Device is created
  with `bufferDeviceAddress`, `VK_KHR_acceleration_structure`, `VK_KHR_deferred_host_operations`,
  and `VK_KHR_ray_query`. Physical device selection is `devices[0]` — no scoring.
- **`kfvk_RayTracing`** (`kfvk_create_raytracing_resources`) — the compute pipeline for
  `trace_test.comp.glsl`, its descriptor set layout/pool/set, a host-visible color output buffer, the
  vertex buffer, and the BLAS/TLAS.

`kfvk_create_buffer` is the single allocation path: it creates the buffer, finds a memory type, and
chains `VkMemoryAllocateFlagsInfo` with `DEVICE_ADDRESS_BIT` and fetches the device address whenever
`SHADER_DEVICE_ADDRESS_BIT` is in the usage flags. There is no allocator — one `VkDeviceMemory` per
buffer.

`build_as_common` builds both the BLAS and the TLAS. It sizes via
`vkGetAccelerationStructureBuildSizesKHR`, allocates the result buffer plus a scratch buffer padded by
`minAccelerationStructureScratchOffsetAlignment`, records `vkCmdBuildAccelerationStructuresKHR`, and
then **submits and blocks on the fence** before returning. Builds are synchronous and one at a time —
fine for startup, not for anything per-frame.

Geometry is fed as a bare triangle soup: `VK_INDEX_TYPE_NONE_KHR`, one `Triangle` (three `vec3s`) per
primitive, uploaded straight into a `HOST_VISIBLE | HOST_COHERENT` buffer with `memcpy`. No staging
buffer, no index buffer, no per-geometry split. The TLAS holds exactly one identity-transform instance.

`VK_CHECK` logs and continues; `VK_TRY` logs and `return False`. Prefer `VK_TRY` in anything returning
`b32`.

A large `#if 0` block (`vk.c:121`–`vk.c:520`) holds the previous `kfvk_create` / `kfvk_dispatch` pair
from before the Graphics/RayTracing split. It is a useful reference for the dispatch-and-readback
sequence that the new path still needs, but it refers to a `kfvk_State` type that no longer exists.

### Presentation

Currently a CPU round-trip, and deliberately so for now. `main.c` creates an `SDL_Renderer` and a
streaming `SDL_Texture`, and the compute shader writes `f32` RGB into a host-visible buffer intended
to be mapped and blitted into that texture. A `VkSurfaceKHR` is created but never used, and although
`VK_KHR_swapchain` is enabled, no swapchain exists. SDL's renderer runs its own backend, so there are
effectively two GPU contexts. Rendering directly into a swapchain image is the eventual replacement.

Resolution is hardcoded at 1280x960 in several independent places: `main.c`, the color buffer size in
`vk.c`, the dispatch counts, and `dim` inside both shaders. Change all of them together.

### Shaders (`src/*.comp.glsl`)

`trace_test.comp.glsl` is the real one: 16x8 workgroups, `GL_EXT_ray_query`, a storage buffer of
floats at binding 0, the TLAS at binding 1, and a camera UBO at binding 2. It initializes a
`rayQueryEXT`, drains it with `rayQueryProceedEXT`, and writes a depth-derived grey. It is a single
primary ray visualization, not a path tracer — no bounces, no RNG, no accumulation, no materials.

`test.comp.glsl` is the older UV-gradient shader with no ray tracing. Still built, no longer loaded.

Watch std140 layout in the UBO: consecutive `vec3`s are 16-byte aligned, so the C-side struct needs
padding (or `vec4`s) to match.

### Color pipeline (`src/colorspace.h`, `src/cie_xyz_lut.c`)

Wavelengths are stored in a `u8` with the encoding `nm = stored * 2 + 360`, covering 360–830 nm in
2 nm steps (236 significant entries, LUT padded to 256). That encoding exists so `spectral_to_xyz()`
is a direct table index.

The chain is `spectral_to_xyz` -> accumulate XYZ -> `normalize_xyz` (hardcoded sRGB reference black
and white points) -> `normalized_xyz_to_linear_rgb` (sRGB matrix, clamped) -> `linear_rgb_to_srgb`
(gamma).

**None of this is wired into the Vulkan path yet** — the compute shader emits plain RGB floats.
Getting the spectral pipeline onto the GPU is the point of the project and is still entirely ahead.

`cie_xyz_lut.c` is generated by `data/gen_cie_xyz_lut.py` from `data/CIE_xyz_1931_2deg.csv` — edit the
generator, not the table. That script has `print_ppm()` enabled and `print_c_lut()` commented out in
`__main__`; swap them to regenerate the C table.

### RGB to spectral reflectance (in progress)

Work toward Jakob & Hanika 2019 spectral upsampling (the paper PDF is in the repo root). Landed:
`eval_spectral_reflectance()` in `colorspace.h`, which evaluates the sigmoid-of-quadratic model
`S(c0*l^2 + c1*l + c2)`. Not landed: the coefficient tables.
`data/gen_rgb_to_spectral_reflectance.py` (numpy/scipy, L-BFGS-B fit over a 64^3 grid per RGB region)
is the working generator and emits `src/rgb_to_spectral.h`. `data/gen_rgb_to_sd.c` is an abandoned
half-written C version and does not compile.

### Scene loading (`src/model.c`)

`read_obj_triangles` (fast_obj) and `read_fbx_triangles` (ufbx) both flatten a file into a
`malloc`'d `Triangle` array — positions only, no normals, UVs, materials, or per-object grouping.
Both assert that every face is a triangle; `data/models/TriangulateOBJ.exe` is there for
pre-triangulating OBJs. The active scene is hardcoded in `main()`; it is currently
`data/models/sponza/sponza.triangulated.obj`.

### CPU tracer (orphaned — `src/worker.c`, `src/bvh.c`, `src/camera.c`)

Kept for reference and still compiled. Workers are parked on a semaphore pair, each owning a
disjoint horizontal band of rows plus its own `Rng`. Embree is used **only** as a BVH builder
(`rtcBuildBVH` with custom callbacks); traversal is hand-written and `rtcIntersect` is never called.
`bvh.c` also has a second flattened SoA layout (`Bvh`, `bvh_intersect`) that was intended to replace
the pointer-based `EmbreeBvh` and never did. Accumulation was a running average in `acc_xyz` (lerp by
`1/sample_count`), reset on camera movement.

Do not extend any of this. If the GPU path needs something from here (camera basis math, sampling
helpers, the RNG), port it rather than reviving the CPU frame loop.

## Conventions

- Types are the short forms from `toteload.h` (`u32`, `f32`, `b32`, ...). `b32` for booleans in new
  Vulkan code, `bool` in the older CPU code.
- Vector math is cglm's struct API (`vec3s`, `glms_vec3_*`). cglm headers need the
  `-Wstatic-in-inline` pragma push/pop wrapper used everywhere they are included.
- Vulkan symbols in `vk.h`/`vk.c` are prefixed `kfvk_` for functions and `kfvk_Buffer`-style for types.
- Small helpers are `inline` in headers (`toteload.h`, `kingfisher_core.h`, `colorspace.h`) rather
  than split across a `.c`.
- Y is up; camera orientation is pitch/yaw (`pitch_yaw_to_vec3`), pitch clamped to +-pi/2.
- Dependencies are vendored under `vendor/` (SDL3, Embree 4.4.0, cglm, nuklear, fast_obj, ufbx, volk,
  xoshiro128+/splitmix64) and included via `-Ivendor -Ivendor/SDL -Ivendor/embree-4.4.0/include
  -Ivendor/cglm`, plus `-isystem $VULKAN_SDK/Include`.
- `notes.txt` collects the radiometry/photometry background and the data sources behind the LUT.

## Current state of the render loop

As of the `vulkan` branch: acceleration structures build successfully and the ray-query pipeline is
created, but **nothing is drawn yet**. The frame loop in `main.c` only handles quit/ESC and presents
an untouched texture. The remaining links in the chain are the descriptor writes, the camera uniform
buffer, the dispatch, and the buffer-to-texture readback. Camera controls are not wired into the new
loop either — `update_camera_from_input` exists in the orphaned `camera.c` but is unused.
