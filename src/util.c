
// Spectral power distribution
typedef struct Spd_8 {
  u8 wavelengths[8];
  f32 powers[8];
} Spd_8;

void transform_triangles(mat3s m, Triangle *triangles, u64 count) {
  for (u64 i = 0; i < count; i++) {
    triangles[i].p[0] = glms_mat3_mulv(m, triangles[i].p[0]);
    triangles[i].p[1] = glms_mat3_mulv(m, triangles[i].p[1]);
    triangles[i].p[2] = glms_mat3_mulv(m, triangles[i].p[2]);
  }
}

void xyz_to_srgb_pixels(i32 width, i32 height, vec3s const *xyz, u8 *pixels, i32 pitch) {
  for (i32 y = 0; y < height; y++) {
    for (i32 x = 0; x < width; x++) {
      i32 i = y * width + x;

      vec3s nxyz = normalize_xyz(xyz[i]);
      vec3s rgb = normalized_xyz_to_linear_rgb(nxyz);
      vec3s srgb = linear_rgb_to_srgb(rgb);

      pixels[(height - 1 - y) * pitch + x * 4 + 0] = (u8)(srgb.r * 255.0f);
      pixels[(height - 1 - y) * pitch + x * 4 + 1] = (u8)(srgb.g * 255.0f);
      pixels[(height - 1 - y) * pitch + x * 4 + 2] = (u8)(srgb.b * 255.0f);
    }
  }
}

void normals_to_srgb_pixels(i32 width, i32 height, vec3s const *normals, u8 *pixels, i32 pitch) {
  for (i32 y = 0; y < height; y++) {
    for (i32 x = 0; x < width; x++) {
      i32 i = y * width + x;

      vec3s rgb = glms_vec3_scale(normals[i], 255.0f);
      vec3s srgb = linear_rgb_to_srgb(rgb);

      pixels[(height - 1 - y) * pitch + x * 4 + 0] = (u8)(srgb.r * 255.0f);
      pixels[(height - 1 - y) * pitch + x * 4 + 1] = (u8)(srgb.g * 255.0f);
      pixels[(height - 1 - y) * pitch + x * 4 + 2] = (u8)(srgb.b * 255.0f);
    }
  }
}

int IsDebuggerPresent();

typedef struct AppState {
  Perspective perspective;

  struct {
    CameraControls current;
    // Previous camera state for tracking movement
    CameraControls prev;
  } camera;

  i32 selected_buffer;
} AppState;

void draw_debug_ui(struct nk_context *ctx, AppState *app) {
  f32 line_height = 18;

  if (nk_begin(ctx, "Debug info", nk_rect(20, 20, 300, 400), NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE)) {
    nk_layout_row_static(ctx, line_height, 200, 1);
    nk_label(ctx, "Position", NK_TEXT_LEFT);
    nk_property_float(ctx, "#x ", -1e6f, &app->camera.current.position.x, 1e6f, 0.01f, 0.01f);
    nk_property_float(ctx, "#y ", -1e6f, &app->camera.current.position.y, 1e6f, 0.01f, 0.01f);
    nk_property_float(ctx, "#z ", -1e6f, &app->camera.current.position.z, 1e6f, 0.01f, 0.01f);

    nk_label(ctx, "Perspective", NK_TEXT_LEFT);
    app->perspective.selected = nk_combo(
      ctx,
      (const char *[2]){"PINHOLE", "ORTHOGRAPHIC"},
      2,
      app->perspective.selected,
      line_height,
      nk_vec2(200, 200)
    );

    nk_label(ctx, "Pinhole settings", NK_TEXT_LEFT);
    nk_property_float(ctx, "fov", 0.01f * PI, &app->perspective.pinhole.fov_radians, PI, 0.01f, 0.01f);

    nk_label(ctx, "Orthograpic settings", NK_TEXT_LEFT);
    nk_label(ctx, "TODO", NK_TEXT_LEFT);

    nk_label(ctx, "Selected buffer", NK_TEXT_LEFT);
    app->selected_buffer = nk_combo(ctx, (const char *[2]){"LIGHT", "NORMALS"}, 2, app->selected_buffer, line_height, nk_vec2(200, 200));

    nk_end(ctx);
  }
}

