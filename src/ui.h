#ifndef UI_H_INCLUDED
#define UI_H_INCLUDED

#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT

/* mandatory: sdl3_renderer depends on those defines */
#define NK_INCLUDE_COMMAND_USERDATA
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT

#ifndef NK_INCLUDE_FIXED_TYPES
    #define NK_INT8              Sint8
    #define NK_UINT8             Uint8
    #define NK_INT16             Sint16
    #define NK_UINT16            Uint16
    #define NK_INT32             Sint32
    #define NK_UINT32            Uint32
    /* SDL guarantees 'uintptr_t' typedef */
    #define NK_SIZE_TYPE         uintptr_t
    #define NK_POINTER_TYPE      uintptr_t
#endif

#define NK_ASSERT(condition)      SDL_assert(condition)
#define NK_STATIC_ASSERT(exp)     SDL_COMPILE_TIME_ASSERT(, exp)
#define NK_MEMSET(dst, c, len)    SDL_memset(dst, c, len)
#define NK_MEMCPY(dst, src, len)  SDL_memcpy(dst, src, len)
#define NK_VSNPRINTF(s, n, f, a)  SDL_vsnprintf(s, n, f, a)
#define NK_STRTOD(str, endptr)    SDL_strtod(str, endptr)

#define NK_INCLUDE_STANDARD_VARARGS

#include <SDL3/SDL.h>
#include <nuklear/nuklear.h>

#include "toteload.h"

typedef struct UiState {
  SDL_Window *window;
  SDL_Renderer *renderer;
  struct nk_context *ctx;
} UiState;

UiState *ui_init(SDL_Window *window, SDL_Renderer *renderer);
void ui_deinit(UiState *state);

// Returns 1 if the ui used this event
i32 ui_handle_event(UiState const *state, SDL_Event *event);
void ui_render(UiState const *state);

#endif // UI_H_INCLUDED
