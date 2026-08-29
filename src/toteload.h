#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdalign.h>

typedef float  f32;
typedef double f64;

typedef int8_t      i8;
typedef uint8_t     u8;
typedef int16_t     i16;
typedef uint16_t    u16;
typedef int32_t     i32;
typedef uint32_t    u32;
typedef int64_t     i64;
typedef uint64_t    u64;

typedef uintptr_t usize;
typedef intptr_t  isize;

typedef uint8_t  b8;
typedef uint16_t b16;
typedef uint32_t b32;
typedef uint64_t b64;

#ifdef __clang__
#define TTLD_COMPILER_CLANG 1
#endif

#ifdef _WIN32
#define TTLD_OS_WINDOWS 1
#endif

#ifdef __APPLE__
#define TTLD_OS_MACOS 1
#endif

#ifdef TTLD_COMPILER_CLANG
#define TTLD_FUNC __func__
#else
#error "unimplemented: function name macro"
#endif

#define Cat_(a, b) a##b
#define Cat(a, b) Cat_(a, b)

#define internal      static
#define always_inline __attribute__((always_inline)) inline

#define Cast(T, x) ((T)(x))

always_inline u32 bitwidth(u64 x) {
#ifdef TTLD_COMPILER_CLANG
  if (x == 0) {
    return 0;
  }
  return 64 - Cast(u32, __builtin_clzll(x));
#else
#error "todo: bitwidth is not implemented for this platform"
#endif
}

// undefined if x == 0
always_inline u32 count_trailing_zeros64(u64 x) {
#ifdef TTLD_COMPILER_CLANG
  return Cast(u32, __builtin_ctzll(x));
#else
#error "todo: count_trailing_zeros64"
#endif
}

#define KiB(x) (Cast(u64, x) << 10)
#define MiB(x) (Cast(u64, x) << 20)
#define GiB(x) (Cast(u64, x) << 30)

#define Swap(Type, a, b) do { \
  Type tmp_ = (a); \
  a = (b); \
  b = tmp_; \
} while (0)

#define Max(a, b) (((a)>(b))?(a):(b))
#define Min(a, b) (((a)<(b))?(a):(b))
#define Clamp(lo,hi,t) Min(Max(lo, t), hi)

#define Null NULL
#define True 1
#define False 0

#define is_null(p) ((p) == Null)

#define Unused_1(a)     (void)(a)
#define Unused_2(a,...) (void)(a); Unused_1(__VA_ARGS__)
#define Unused_3(a,...) (void)(a); Unused_2(__VA_ARGS__)
#define Unused_4(a,...) (void)(a); Unused_3(__VA_ARGS__)
#define Unused_5(a,...) (void)(a); Unused_4(__VA_ARGS__)
#define Unused_6(a,...) (void)(a); Unused_5(__VA_ARGS__)
#define Unused_7(a,...) (void)(a); Unused_6(__VA_ARGS__)
#define Unused_8(a,...) (void)(a); Unused_7(__VA_ARGS__)
#define Unused_PICK(_1,_2,_3,_4,_5,_6,_7,_8,NAME,...) NAME
#define Unused(...)                                                     \
  Unused_PICK(__VA_ARGS__, Unused_8, Unused_7, Unused_6, Unused_5,      \
                           Unused_4, Unused_3, Unused_2, Unused_1,      \
                           Unused_END)(__VA_ARGS__)

always_inline b32 is_zero_or_power_of_two(usize x) {
  return ((((x)-1) & (x)) == 0);
}

always_inline usize round_up_to_power_of_two(usize x, usize power) {
  return (x + (power - 1)) & (~(power - 1));
}

always_inline void *ptr_offset(void const *p, usize offset) {
  return Cast(void*, Cast(u8 const*, p) + offset);
}

always_inline isize ptr_diff(void const *a, void const *b) {
  return Cast(isize, Cast(u8*, a) - Cast(u8*, b));
}

always_inline void *ptr_forward_align(void const *p, u32 align) {
  return Cast(void*, round_up_to_power_of_two(Cast(usize, p), align));
}

#define zero_struct(type, p) memset(p, 0, sizeof(type))

#define Count_of(x) (sizeof(x)/sizeof(x[0]))
#define Align_of(x) _Alignof(x)
#define Offsetof(s,m) offsetof(s,m)

void ttld_panic_handler(char const *func, char const *file, i32 line);

#ifdef TTLD_COMPILER_CLANG
#define Unreachable() __builtin_unreachable()
#else
#error "unimplemented: Unreachable"
#endif

#define Todo() do { fprintf(stderr, "TODO: "); Panic(); } while (0)
#define TodoMsg(msg) do { fprintf(stderr, "TODO: %s. ", msg); Panic(); } while (0)
#define Assert(e) assert(e)
#define Panic()   do { ttld_panic_handler(TTLD_FUNC, __FILE__, __LINE__); Unreachable(); } while (0)
#define PanicMsg(msg) do { fprintf(stderr, "%s", msg); Panic(); } while (0)

typedef struct String {
  u8 const *str;
  usize len;
} String;

#define string_lit(s) \
  _Pragma("clang diagnostic push") \
  _Pragma("clang diagnostic ignored \"-Wpointer-sign\"") \
  ((String){ .str = s, .len = (sizeof(s) - 1), }) \
  _Pragma("clang diagnostic pop")

String string_from_cstr(char const *s);

// Parses a decimal integer literal.
// Assumes the string contains only digits.
u64 parse_u64(String s);

always_inline b32 string_eq(String a, String b) {
  if (a.len != b.len) {
    return False;
  }

  return memcmp(a.str, b.str, a.len) == 0;
}

typedef void *(*AllocatorFunction)(
  void *ctx, void *ptr, size_t old_byte_size, size_t new_byte_size, u32 align
);

typedef struct Allocator {
  AllocatorFunction fn;
  void *ctx;
} Allocator;

#define Alloc(allocator, size, align)                      (allocator).fn((allocator).ctx, Null, 0, size, align)
#define Realloc(allocator, ptr, old_size, new_size, align) (allocator).fn((allocator).ctx, ptr, old_size, new_size, align)
#define Free(allocator, ptr, size)                         (allocator).fn((allocator).ctx, ptr, size, 0, 0)

usize vmem_page_size(void);
void *vmem_reserve(usize size);
b32   vmem_commit(void *p, usize size);
void  vmem_release(void *p, usize size);

#define Stack(type) struct { type* data; u32 len; u32 cap; }

#define stack_init(sp,p,c) do { (sp)->data = (p); (sp)->len = 0; (sp)->cap = (c); } while (0)
#define stack_push_ptr_unchecked(sp) ((sp)->data + ((sp)->len)++)
#define stack_push_ptr(sp) (Assert((sp)->len < (sp)->cap), stack_push_ptr_unchecked(sp))
#define stack_push_unchecked(sp,x) ((sp)->data[(sp)->len++] = (x))
#define stack_push(sp,x) (Assert((sp)->len < (sp)->cap), stack_push_unchecked((sp),(x)))
#define stack_pop_unchecked(sp) ((sp)->data[--(sp)->len])
#define stack_pop(sp) (Assert(!stack_is_empty(sp)), stack_pop_unchecked(sp))
#define stack_peek_ptr_unchecked(sp) (&((sp)->data[(sp)->len-1]))
#define stack_peek_ptr(sp) (Assert(!stack_is_empty(sp)), stack_peek_ptr_unchecked(sp))
#define stack_peek_unchecked(sp) (*stack_peek_ptr_unchecked(sp))
#define stack_is_empty(sp) ((sp)->len == 0)

typedef struct {
  void *base;
  void *commit_end;
  void *reserve_end;
  void *at;
} Arena;

typedef struct {
  Arena *arena;
  void  *at;
} ArenaSnapshot;

typedef struct {
  usize reserve_size;
  usize initial_commit_size;
} ArenaOptions;

void arena_init(Arena *arena, ArenaOptions *options);
void arena_deinit(Arena *arena);

void *arena_push(Arena *arena, usize size, u32 align);

#define arena_push_array(type, arena, count) arena_push(arena, (count) * sizeof(type), Align_of(type))
#define arena_push_one(type, arena) arena_push_array(type, arena, 1)

#define arena_freelist_alloc_typed(type, arena, freelist, reserve) \
  Cast(type*, arena_freelist_alloc(arena, freelist, sizeof(type), Align_of(type), reserve))

String arena_copy_string(Arena *arena, String s);

always_inline ArenaSnapshot arena_scope_begin(Arena *arena) {
  return (ArenaSnapshot){ .arena = arena, .at = arena->at, };
}

// [Thought] Everything you need is present in `snapshot` and passing in `arena` is only done as an
// extra safety check. It might be more convenient to only pass in the snapshot.
always_inline void arena_scope_end(Arena *arena, ArenaSnapshot snapshot) {
  Assert(arena == snapshot.arena);
  arena->at = snapshot.at;
}

// The amount of memory pointed to by `mem` must be at least `stride * count` bytes.
always_inline void freelist_grow(void **freelist, void *mem, usize stride, usize count) {
  for (usize i = 0; i < count-1; i++) {
    void **p = ptr_offset(mem, i * stride);
    *p = ptr_offset(mem, (i + 1) * stride);
  }

  void **p = ptr_offset(mem, (count - 1) * stride);
  *p = *freelist;

  *freelist = mem;
}

always_inline void *freelist_alloc(void **freelist) {
  void *p = *freelist;
  *freelist = *Cast(void***,freelist);
  return p;
}

always_inline void freelist_free(void **freelist, void *p) {
  *Cast(void**,p) = **Cast(void***, freelist);
  *freelist = p;
}
