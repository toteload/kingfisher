#include "ui.h"

#pragma warning(push)
#pragma warning(disable:4116)
#pragma warning(disable:4127)
#pragma warning(disable:4701)
#define NK_IMPLEMENTATION
#include <nuklear/nuklear.h>
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable:4116)
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#define NK_SDL3_RENDERER_IMPLEMENTATION
#include <nuklear/nuklear_sdl3_renderer.h>
#pragma warning(pop)

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
