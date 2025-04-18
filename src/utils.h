#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "./arena.h"

#define arena_da_insert(a, da, i, item)                                 \
    do {                                                                \
        assert((i) <= (da)->count);                                     \
        if ((da)->count >= (da)->capacity) {                            \
            size_t new_capacity = (da)->capacity == 0 ? ARENA_DA_INIT_CAP : (da)->capacity*2; \
            (da)->items = cast_ptr((da)->items)arena_realloc(           \
                (a), (da)->items,                                       \
                (da)->capacity*sizeof(*(da)->items),                    \
                new_capacity*sizeof(*(da)->items));                     \
            (da)->capacity = new_capacity;                              \
        }                                                               \
        memmove((da)->items + (i) + 1, (da)->items + (i), ((da)->count - (i)) * sizeof(*(da)->items)); \
        (da)->items[(i)] = (item);                                      \
        (da)->count += 1;                                               \
    } while (0)

#define arena_da_remove(da, i)                                          \
    do {                                                                \
        assert((i) < (da)->count);                                      \
        memmove((da)->items + (i), (da)->items + (i) + 1, ((da)->count - (i)) * sizeof(*(da)->items)); \
        (da)->count -= 1;                                               \
    } while (0)

#define arena_da_copy_overwrite(a, da_dst, da_src)                      \
    do {                                                                \
        assert(sizeof(*(da_dst)->items) == sizeof(*(da_src)->items));   \
        (da_dst)->items = arena_alloc((a), sizeof(*(da_src)->items)*(da_src)->count); \
        memcpy((da_dst)->items, (da_src)->items, sizeof(*(da_src)->items)*(da_src)->count); \
        (da_dst)->count = (da_src)->count;                              \
        (da_dst)->capacity = (da_src)->capacity;                        \
    } while (0)

#define SV(cstr) ((String_View) { .data = (cstr), .size = strlen(cstr) })
#define SV_STATIC(cstr) { .data = (cstr), .size = sizeof(cstr) - 1 }

#define str_append_char(a, str, ch) arena_da_append((a), (str), ch)
#define str_append_sized(a, str, data, size) arena_da_append_many((a), (str), (data), (size))
#define str_append_sv(a, str, sv) arena_da_append_many((a), (str), (sv).data, (sv).size)
#define str_append_cstr(a, str, cstr) arena_da_append_many((a), (str), (cstr), strlen(cstr))
#define str_append_null(a, str) arena_da_append((a), (str), '\0')

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} String;

String str_with_cap(Arena *arena, size_t cap);
bool str_eq(String *a, String *b);
bool str_eq_cstr(String *a, const char *b);
void str_append_vfmt(Arena *arena, String *str, const char *fmt, va_list args);
void str_append_fmt(Arena *arena, String *str, const char *fmt, ...);

typedef struct {
    const char *data;
    size_t size;
} String_View;

int64_t sv_to_int64(String_View sv);
uint64_t sv_to_uint64(String_View sv);
double sv_to_decimal(String_View sv);

String_View sv_from_parts(const char *data, size_t size);

bool sv_find(String_View sv, char ch, size_t *index);
bool sv_find_rev(String_View sv, char ch, size_t *index);
bool sv_eq(String_View a, String_View b);

bool sv_starts_with(String_View sv, const char *prefix);
String_View sv_chop_until(String_View *sv, char ch);
String_View sv_trim_left(String_View sv);
String_View sv_trim_right(String_View sv);
String_View sv_trim(String_View sv);

#endif // UTILS_H_
