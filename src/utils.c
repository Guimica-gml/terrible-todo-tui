#include "./utils.h"

// NOTE(nic): We need alloca because cl.exe does not allow for variable-length arrays
#if _WIN32
#    define utils_alloca(size) _alloca(size)
#elif __unix__
#    include <alloca.h>
#    define utils_alloca(size) alloca(size)
#else
#    error "Platform does not support `alloca`"
#endif

String str_with_cap(Arena *arena, size_t cap) {
    String str = {0};
    str.items = arena_alloc(arena, cap * sizeof(char));
    str.capacity = cap;
    return str;
}

bool str_eq(String *a, String *b) {
    if (a->count != b->count) {
        return false;
    }
    return memcmp(a->items, b->items, a->count) == 0;
}

bool str_eq_cstr(String *a, const char *b) {
    size_t b_count = strlen(b);
    if (a->count != b_count) {
        return false;
    }
    return memcmp(a->items, b, a->count) == 0;
}

void str_append_vfmt(Arena *arena, String *str, const char *fmt, va_list args) {
    va_list copy;
    va_copy(copy, args);
    int str_size = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    char *temp = utils_alloca((str_size + 1) * sizeof(char));
    vsnprintf(temp, str_size + 1, fmt, args);
    arena_da_append_many(arena, str, temp, str_size);
}

void str_append_fmt(Arena *arena, String *str, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    str_append_vfmt(arena, str, fmt, args);
    va_end(args);
}

int64_t sv_to_int64(String_View sv) {
    char *int64_string = utils_alloca(sv.size + 1);
    memcpy(int64_string, sv.data, sv.size);
    int64_string[sv.size] = '\0';
    return atoll(int64_string);
}

uint64_t sv_to_uint64(String_View sv) {
    char *uint64_string = utils_alloca(sv.size + 1);
    memcpy(uint64_string, sv.data, sv.size);
    uint64_string[sv.size] = '\0';
    return strtoull(uint64_string, NULL, 10);
}

double sv_to_decimal(String_View sv) {
    char *decimal_string = utils_alloca(sv.size + 1);
    memcpy(decimal_string, sv.data, sv.size);
    decimal_string[sv.size] = '\0';
    return strtod(decimal_string, NULL);
}

String_View sv_from_parts(const char *data, size_t size) {
    return (String_View) { data, size };
}

bool sv_find(String_View sv, char ch, size_t *index) {
    for (size_t i = 0; i < sv.size; ++i) {
        if (sv.data[i] == ch) {
            if (index != NULL) {
                *index = i;
            }
            return true;
        }
    }
    return false;
}

bool sv_find_rev(String_View sv, char ch, size_t *index) {
    for (int i = sv.size; i >= 0; --i) {
        if (sv.data[i] == ch) {
            if (index != NULL) {
                *index = i;
            }
            return true;
        }
    }
    return false;
}

bool sv_eq(String_View a, String_View b) {
    if (a.size != b.size) {
        return false;
    }
    return memcmp(a.data, b.data, a.size) == 0;
}

bool sv_starts_with(String_View sv, const char *prefix) {
    size_t prefix_size = strlen(prefix);
    if (prefix_size > sv.size) {
        return false;
    }
    return memcmp(sv.data, prefix, prefix_size) == 0;
}

String_View sv_chop_until(String_View *sv, char ch) {
    size_t i = 0;
    while (i < sv->size && sv->data[i] != ch) {
        i += 1;
    }

    String_View result = sv_from_parts(sv->data, i);
    if (i < sv->size) {
        sv->size -= i + 1;
        sv->data  += i + 1;
    } else {
        sv->size -= i;
        sv->data  += i;
    }
    return result;
}

String_View sv_trim_left(String_View sv) {
    size_t i = 0;
    while (i < sv.size && isspace(sv.data[i])) {
        i += 1;
    }
    return sv_from_parts(sv.data + i, sv.size - i);
}

String_View sv_trim_right(String_View sv) {
    size_t i = 0;
    while (i < sv.size && isspace(sv.data[sv.size - 1 - i])) {
        i += 1;
    }
    return sv_from_parts(sv.data, sv.size - i);
}

String_View sv_trim(String_View sv) {
    return sv_trim_right(sv_trim_left(sv));
}
