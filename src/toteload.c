#include "toteload.h"

void ttld_panic_handler(char const *func, char const *file, i32 line) {
  fprintf(stderr, "panic in %s at %s:%d\n", func, file, line);
  abort();
}

String string_from_cstr(char const *s) {
  u32 len = strlen(s);
  return (String){
    .str = Cast(const u8*, s),
    .len = len,
  };
}

u64 parse_u64(String s) {
  u64 value = 0;
  for (usize i = 0; i < s.len; i++) {
    value = value * 10 + (s.str[i] - '0');
  }

  return value;
}

#ifdef TTLD_OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRA_LEAN
#include <Windows.h>

internal b32 is_sys_info_initialized = False;
internal SYSTEM_INFO sys_info;
usize vmem_page_size(void) {
  if (!is_sys_info_initialized) {
    GetSystemInfo(&sys_info);
    is_sys_info_initialized = True;
  }

  return sys_info.dwPageSize;
}

void *vmem_reserve(usize size) {
  return VirtualAlloc(Null, size, MEM_RESERVE, PAGE_READWRITE);
}

b32 vmem_commit(void *p, usize size) {
  void *x = VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE);
  return !is_null(x);
}

void vmem_release(void *p, usize size) {
  VirtualFree(p, size, MEM_RELEASE);
}
#endif // TTLD_OS_WINDOWS

#ifdef TTLD_OS_MACOS
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>

internal usize macos_page_size = 0;

usize vmem_page_size(void) {
  if (macos_page_size == 0) {
    macos_page_size = Cast(usize, getpagesize());
  }

  return macos_page_size;
}

void *vmem_reserve(usize size) {
  return mmap(Null, size, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
}

b32 vmem_commit(void *p, usize size) {
  return mprotect(p, size, PROT_READ | PROT_WRITE) == 0;
}

void vmem_release(void *p, usize size) {
  munmap(p, size);
}
#endif // TTLD_OS_MACOS

void arena_init(Arena *arena, ArenaOptions *options) {
  void *p = vmem_reserve(options->reserve_size);
  if (options->initial_commit_size > 0) {
    b32 ok = vmem_commit(p, options->initial_commit_size);
    Assert(ok);
  }

  *arena = (Arena){
    .base        = p,
    .commit_end  = ptr_offset(p, options->initial_commit_size),
    .reserve_end = ptr_offset(p, options->reserve_size),
    .at          = p,
  };
}

void arena_deinit(Arena *arena) {
  if (arena->base) {
    vmem_release(arena->base, Cast(usize, ptr_diff(arena->reserve_end, arena->base)));
  }

  memset(arena, 0, sizeof(Arena));
}

#define Default_commit_growth_size KiB(512)

void *arena_push(Arena *arena, usize size, u32 align) {
  void *aligned        = ptr_forward_align(arena->at, align);
  void *at_after_alloc = ptr_offset(aligned, size);

  if (at_after_alloc <= arena->commit_end) {
    arena->at = at_after_alloc;
    return aligned;
  }

  if (at_after_alloc > arena->reserve_end) {
    Panic();
    return Null;
  }

  u32 page_size = vmem_page_size();
  usize commit_size_needed = Cast(usize, ptr_diff(at_after_alloc, arena->commit_end));
  usize x = Max(Default_commit_growth_size, commit_size_needed);
  usize commit_size = round_up_to_power_of_two(x, page_size);

  b32 ok = vmem_commit(arena->commit_end, commit_size);
  Assert(ok);

  arena->at = at_after_alloc;
  arena->commit_end = ptr_offset(arena->commit_end, commit_size);

  return aligned;
}

String arena_copy_string(Arena *arena, String s) {
  u8 *p = arena_push_array(u8, arena, s.len);
  memcpy(p, s.str, s.len);
  return (String){ .str = p, .len = s.len, };
}
