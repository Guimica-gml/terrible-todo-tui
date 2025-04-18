#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#ifdef __linux__
#    include <unistd.h>
#elif _WIN32
#    include <windows.h>
#else
#    error "OS not supported"
#endif

// TODO(artik): add raylib as a dependency
// TODO(nic):
// - completely change the way we do Been :Kapp:
// - add utf8 support

#include "./utils.h"
#define PLAT_IMPLEMENTATION
#include "./plat.h"
#define ARENA_IMPLEMENTATION
#include "./arena.h"

#define SCROLL_EFFECT_SPEED_MULT 4.0f
#define WAIT_EFFECT_TIME 2.0f

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define clamp(v, _min, _max) min(max(v, _min), _max)
#define ceilf(v) ((int)((v) + 0.5f))

#define delta_time (1.0f/60.0f)

typedef struct {
    size_t x;
    size_t y;
    size_t w;
    size_t h;
} Rect;

typedef struct {
    Rect left;
    Rect right;
} Split;

typedef struct {
    String *items;
    size_t count;
    size_t capacity;
    size_t cursor;
    size_t offset;
} List;

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
    size_t cursor;
    size_t offset;
} Line_Edit;

typedef enum {
    TODO_STATE_IDLE = 0,
    TODO_STATE_ADD,
    TODO_STATE_EDIT,
} TODO_State;

typedef enum {
    TODO_LIST_TODOS,
    TODO_LIST_DONES,
} TODO_List_Index;

typedef struct {
    List lists[2];
    TODO_State state;

    // TODO_STATE_IDLE
    TODO_List_Index list_index;
    float scroll_effect;
    float wait_effect;

    // TODO_STATE_ADD
    Line_Edit line_edit;
} TODO_App;

Split split_rect(Rect rect) {
    Split split = {0};
    split.left.x = rect.x;
    split.left.y = rect.y;
    split.left.w = ceilf(rect.w / 2.0f);
    split.left.h = rect.h;

    split.right.x = rect.x + split.left.w;
    split.right.y = rect.y;
    split.right.w = (size_t) (rect.w / 2.0f);
    split.right.h = rect.h;
    return split;
}

void draw_rect(Rect rect) {
    if (rect.w < 3 || rect.h < 3) {
        return;
    }
    position_cursor(rect.x, rect.y);
    printf("╔");
    for (size_t i = 0; i < rect.w - 2; ++i) {
        printf("═");
    }
    printf("╗");

    for (size_t i = 1; i < rect.h; ++i) {
        position_cursor(rect.x, rect.y + i);
        printf("║%*s║", (int) (rect.w - 2), " ");
    }

    position_cursor(rect.x, rect.y + rect.h);
    printf("╚");
    for (size_t i = 0; i < rect.w - 2; ++i) {
        printf("═");
    }
    printf("╝");
}

Rect draw_box(Rect rect, const char *title) {
    draw_rect(rect);
    size_t title_len = strlen(title);
    if (title_len <= rect.w - 2) {
        position_cursor(rect.x + 1, rect.y);
        printf("%s", title);
    }
    return (Rect) {
        rect.x + 1,
        rect.y + 1,
        rect.w - 2,
        rect.h - 2,
    };
}

typedef enum {
    BEEN_PRINTABLE_LAST = 255,
    BEEN_UP,
    BEEN_DOWN,
    BEEN_LEFT,
    BEEN_RIGHT,
    BEEN_ENTER,
    BEEN_BACKSPACE,
    BEEN_DELETE,
    BEEN_ESC,
    BEEN_UNKNOWN,
} Been;

int fgetbeen(FILE *stream) {
    char ch = fgetc(stream);
    if (ch == EOF) {
        return BEEN_UNKNOWN;
    } else if (ch == '\n') {
        return BEEN_ENTER;
    } else if (ch == 127) {
        return BEEN_BACKSPACE;
    } else if (ch == 27) {
        char buffer[64];
        buffer[0] = ch;
        buffer[1] = fgetc(stream);
        if (buffer[1] == EOF) {
            return BEEN_ESC;
        }
        size_t count = 2;
        while (!isalpha(buffer[count - 1]) && buffer[count - 1] != '~') {
            char ch = fgetc(stream);
            if (ch != EOF) {
                assert(count < 64);
                buffer[count++] = ch;
                fprintf(stderr, "%d\n", buffer[count - 1]);
            }
        }
        if (count == 3) {
            switch (buffer[2]) {
            case 65: return BEEN_UP;
            case 66: return BEEN_DOWN;
            case 67: return BEEN_RIGHT;
            case 68: return BEEN_LEFT;
            }
        } else if (count == 4) {
            if (buffer[2] == '3' && buffer[3] == '~') {
                return BEEN_DELETE;
            }
        }
        return BEEN_UNKNOWN;
    }
    return ch;
}

void handle_exit(void) {
    unprepare_terminal();
    visible_cursor();
    delete_page();
    exit(0);
}

#ifdef __linux__
void sigint_handler(int signum) {
    (void) signum;
    handle_exit();
}
#elif _WIN32
BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        handle_exit();
    }
    return false;
}
#endif

void limit_cursor(size_t *offset, size_t size, size_t cursor) {
    if (cursor > *offset + size) {
        *offset += 1;
    } else if (cursor < *offset) {
        *offset -= 1;
    }
}

void app_add_entry(Arena *arena, TODO_App *app, TODO_List_Index list_index, const char *todo, size_t todo_len) {
    List *list = &app->lists[list_index];
    String entry = {0};
    str_append_sized(arena, &entry, todo, todo_len);
    arena_da_append(arena, list, entry);
    list->cursor = list->count - 1;
}

String app_delete_entry(TODO_App *app, TODO_List_Index list_index, size_t entry_index) {
    List *list = &app->lists[list_index];
    if (list->count <= 0) {
        return (String) {0};
    }
    assert(entry_index < list->count);
    String entry = list->items[entry_index];
    arena_da_remove(list, entry_index);
    list->cursor = clamp(list->cursor, 0, list->count - 1);
    return entry;
}

void app_move_entry(Arena *arena, TODO_App *app, TODO_List_Index from_list_index, size_t entry_index) {
    List *list = &app->lists[from_list_index];
    if (list->count <= 0) {
        return;
    }
    assert(entry_index < list->count);
    String entry = app_delete_entry(app, from_list_index, entry_index);
    TODO_List_Index to_entry_index = !from_list_index;
    app_add_entry(arena, app, to_entry_index, entry.items, entry.count);
}

void app_reset_effects(TODO_App *app) {
    app->scroll_effect = 0.0f;
    app->wait_effect = 0.0f;
}

bool line_edit_all_whitespace(Line_Edit *line) {
    for (size_t i = 0; i < line->count; ++i) {
        if (!isspace(line->items[i])) {
            return false;
        }
    }
    return true;
}

int line_edit_handle_been(Arena *arena, Line_Edit *line, int been) {
    if (been >= BEEN_PRINTABLE_LAST) {
        switch (been) {
        case BEEN_LEFT: {
            if (line->cursor > 0) {
                line->cursor -= 1;
            }
        } break;
        case BEEN_RIGHT: {
            if (line->cursor < line->count) {
                line->cursor += 1;
            }
        } break;
        case BEEN_ESC: {
            return -1;
        } break;
        case BEEN_ENTER: {
            if (line->count == 0) {
                return -1;
            }
            if (line_edit_all_whitespace(line)) {
                return -1;
            }
            return 1;
        } break;
        case BEEN_BACKSPACE: {
            if (line->cursor > 0) {
                arena_da_remove(line, line->cursor - 1);
                line->cursor -= 1;
            }
        } break;
        case BEEN_DELETE: {
            if (line->cursor < line->count) {
                arena_da_remove(line, line->cursor);
            }
        } break;
        case BEEN_UNKNOWN: {
            return 0;
        } break;
        }
    } else {
        arena_da_insert(arena, line, line->cursor, been);
        line->cursor += 1;
    }
    return 0;
}

int update_and_draw_line_edit(Arena *arena, Line_Edit *line, size_t x, size_t w, size_t y) {
    position_cursor(x, y);
    const char *a = line->items + line->offset;
    printf("%.*s", (int) min(line->count - line->offset, w - 1), a);

    int ch = fgetbeen(stdin);
    int state = line_edit_handle_been(arena, line, ch);
    limit_cursor(&line->offset, w - 1, line->cursor);

    position_cursor(x + line->cursor - line->offset, y);
    set_bg_color(47, 30);
    if (line->cursor < line->count) {
        char ch = line->items[line->cursor];
        printf("%c", ch);
    } else {
        printf(" ");
    }
    reset_bg_color();
    return state;
}

void clear_line_edit(Line_Edit *line) {
    line->count = 0;
    line->cursor = 0;
    line->offset = 0;
}

void update_and_draw_list(Arena *arena, Rect rect, TODO_App *app, TODO_List_Index list_index) {
    List *list = &app->lists[list_index];

    if (list_index == app->list_index) {
        switch (app->state) {
        case TODO_STATE_IDLE: {
            int ch = fgetbeen(stdin);
            if (ch == 'q') {
                handle_exit();
            } else if (ch == 'a') {
                app->state = TODO_STATE_ADD;
                app_reset_effects(app);
            } else if (ch == 'e') {
                String *text = &list->items[list->cursor];
                arena_da_copy_overwrite(arena, &app->line_edit, text);
                app->state = TODO_STATE_EDIT;
                app_reset_effects(app);
            } else if (ch == 'd') {
                app_delete_entry(app, app->list_index, list->cursor);
            } else if (ch == BEEN_ENTER) {
                app_move_entry(arena, app, app->list_index, list->cursor);
            } else if (ch == BEEN_UP) {
                if (list->cursor > 0) {
                    list->cursor -= 1;
                }
                app_reset_effects(app);
            } else if (ch == BEEN_DOWN) {
                if (list->cursor < list->count - 1) {
                    list->cursor += 1;
                }
                app_reset_effects(app);
            } else if (ch == BEEN_RIGHT) {
                app->list_index = TODO_LIST_DONES;
                app_reset_effects(app);
            } else if (ch == BEEN_LEFT) {
                app->list_index = TODO_LIST_TODOS;
                app_reset_effects(app);
            }
        } break;
        case TODO_STATE_ADD: {
            int state = update_and_draw_line_edit(arena, &app->line_edit, rect.x, rect.w, rect.y + list->count);
            if (state != 0) {
                if (state > 0) {
                    app_add_entry(arena, app, app->list_index, app->line_edit.items, app->line_edit.count);
                }
                clear_line_edit(&app->line_edit);
                app->state = TODO_STATE_IDLE;
            }
        } break;
        case TODO_STATE_EDIT: {
            int state = update_and_draw_line_edit(arena, &app->line_edit, rect.x, rect.w, rect.y + list->cursor - list->offset);
            if (state != 0) {
                if (state > 0) {
                    //__asm__("int3");
                    String *line = &list->items[list->cursor];
                    arena_da_copy_overwrite(arena, line, &app->line_edit);
                    //__asm__("int3");
                }
                clear_line_edit(&app->line_edit);
                app->state = TODO_STATE_IDLE;
            }
        } break;
        default:
            assert(0 && "unreachable");
        }
    }

    limit_cursor(&list->offset, rect.h - 1, list->cursor);
    for (size_t i = 0; i < list->count - list->offset && i < rect.h; ++i) {
        String *entry = &list->items[i + list->offset];
        position_cursor(rect.x, rect.y + i);
        if (app->state == TODO_STATE_EDIT && list_index == app->list_index && i == list->cursor - list->offset) {
            continue;
        }
        if (app->state == TODO_STATE_IDLE && list_index == app->list_index && i == list->cursor - list->offset) {
            if (entry->count > rect.w) {
                if (app->scroll_effect >= entry->count - rect.w) {
                    app->wait_effect += delta_time;
                    if (app->wait_effect >= WAIT_EFFECT_TIME) {
                        app->scroll_effect = 0.0f;
                        app->wait_effect = 0.0f;
                    }
                } else {
                    app->scroll_effect += delta_time * SCROLL_EFFECT_SPEED_MULT;
                }
            } else {
                app->wait_effect = 0.0f;
            }
            set_bg_color(47, 30);
            printf("%.*s", (int) min(entry->count, rect.w), entry->items + (size_t) app->scroll_effect);
        } else {
            printf("%.*s", (int) min(entry->count, rect.w), entry->items);
        }
        reset_bg_color();
    }
}

void update_and_draw_todo_app(Arena *arena, TODO_App *app, Split split) {
    Rect todos_rect = draw_box(split.left, "TODO");
    Rect dones_rect = draw_box(split.right, "DONE");

    update_and_draw_list(arena, todos_rect, app, TODO_LIST_TODOS);
    update_and_draw_list(arena, dones_rect, app, TODO_LIST_DONES);

    fflush(stdout);
}

int main(void) {
#ifdef __linux__
    signal(SIGINT, sigint_handler);
#elif _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
#endif
    prepare_terminal();
    invisible_cursor();
    create_page();

    TODO_App app = {0};
    Arena arena = {0};

    while (true) {
        Term_Size term_size = get_terminal_size();
        Rect term_rect = { 1, 1, term_size.cols, term_size.rows };
        Split split = split_rect(term_rect);

        position_cursor(0, 0);
        update_and_draw_todo_app(&arena, &app, split);
#ifdef __linux__
        usleep(1000000.0f*delta_time);
#elif _WIN32
        Sleep((DWORD)(1000.0f*delta_time));
#endif
    }
    handle_exit();
    return 0;
}
