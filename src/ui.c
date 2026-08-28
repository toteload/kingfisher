#include "ui.h"

#define NK_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#include <nuklear/nuklear.h>
#pragma clang diagnostic pop

#define NK_INCLUDE_STANDARD_VARARGS
#define NK_SDL3_RENDERER_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#include <nuklear/nuklear_sdl3_renderer.h>
#pragma clang diagnostic pop

UiState *ui_init(SDL_Window *window, SDL_Renderer *renderer) {
  UiState *state = malloc(sizeof(UiState));
  struct nk_context *ctx = nk_sdl_init(window, renderer, nk_sdl_allocator());

  *state = (UiState){
    .window = window,
    .renderer = renderer,
    .ctx = ctx,
  };

  struct nk_font_config config = nk_font_config(0);
  f32 font_scale = 1.0f;

  struct nk_font_atlas *atlas = nk_sdl_font_stash_begin(ctx);
  struct nk_font *font = nk_font_atlas_add_default(atlas, 13 * font_scale, &config);
  nk_sdl_font_stash_end(ctx);
  nk_style_set_font(ctx, &font->handle);

  return state;
}

void ui_deinit(UiState *state) {
  nk_sdl_shutdown(state->ctx);
  free(state);
}

i32 ui_handle_event(UiState const *state, SDL_Event *event) {
  SDL_ConvertEventToRenderCoordinates(state->renderer, event);
  return nk_sdl_handle_event(state->ctx, event);
}

void ui_render(UiState const *state) {
  nk_input_end(state->ctx);
  nk_sdl_render(state->ctx, NK_ANTI_ALIASING_ON);
  nk_input_begin(state->ctx);
}
