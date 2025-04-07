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
// TODO(nic): abort todo when it's empty (or just whitespace)
// TODO(nic): add entry editing

#define TERM_IMPLEMENTATION
#include "./term.h"

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

#define TEXT_CAP 256
typedef struct {
    char text[TEXT_CAP];
    size_t text_len;
} Entry;

#define ENTRIES_CAP 256
typedef struct {
    Entry entries[ENTRIES_CAP];
    size_t entry_index;
    size_t entries_count;
} List;

typedef enum {
    TODO_STATE_IDLE = 0,
    TODO_STATE_ADD,
} TODO_State;

typedef enum {
    TODO_LIST_TODOS,
    TODO_LIST_DONES,
} TODO_List_Index;

typedef struct {
    char text[TEXT_CAP];
    size_t text_len;
    size_t cursor;

    size_t x;
    size_t width;
    size_t offset;
} Line_Edit;

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
    split.right.w = rect.w / 2.0f;
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

void draw_list(Rect rect, TODO_App *app, TODO_List_Index list_index) {
    List *list = &app->lists[list_index];
    for (size_t i = 0; i < list->entries_count && i < rect.h; ++i) {
        Entry *entry = &list->entries[i];
        position_cursor(rect.x, rect.y + i);
        if (app->state == TODO_STATE_IDLE && list_index == app->list_index && i == list->entry_index) {
            if (entry->text_len > rect.w) {
                if (app->scroll_effect >= entry->text_len - rect.w) {
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
            printf("%.*s", (int) min(entry->text_len, rect.w), entry->text + (size_t) app->scroll_effect);
        } else {
            printf("%.*s", (int) min(entry->text_len, rect.w), entry->text);
        }
        reset_bg_color();
    }
}

void draw_line_edit(Line_Edit *line, size_t y) {
    position_cursor(line->x, y);
    const char *a = line->text + line->offset;
    printf("%.*s", (int) min(line->text_len - line->offset, line->width - 1), a);

    position_cursor(line->x + line->cursor - line->offset, y);
    set_bg_color(47, 30);
    if (line->cursor < line->text_len) {
        char ch = line->text[line->cursor];
        printf("%c", ch);
    } else {
        printf(" ");
    }
    reset_bg_color();
}

void draw_todo_app(TODO_App *app, Split split) {
    Rect rects[2];
    rects[TODO_LIST_TODOS] = draw_box(split.left, "TODO");
    rects[TODO_LIST_DONES] = draw_box(split.right, "DONE");
    draw_list(rects[TODO_LIST_TODOS], app, TODO_LIST_TODOS);
    draw_list(rects[TODO_LIST_DONES], app, TODO_LIST_DONES);

    Line_Edit *line = &app->line_edit;
    if (app->state == TODO_STATE_ADD) {
        Rect rect = rects[app->list_index];
        List *list = &app->lists[app->list_index];
        draw_line_edit(line, rect.y + list->entries_count);
    }

    fflush(stdout);
}

void handle_exit(void) {
    visible_cursor();
    enable_terminal_interaction();
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

void app_add_entry(TODO_App *app, TODO_List_Index list_index, const char *todo, size_t todo_len) {
    List *list = &app->lists[list_index];
    Entry entry = {0};
    memcpy(entry.text, todo, todo_len);
    entry.text_len = todo_len;
    assert(list->entries_count < ENTRIES_CAP);
    list->entries[list->entries_count++] = entry;
}

Entry app_delete_entry(TODO_App *app, TODO_List_Index list_index, size_t entry_index) {
    List *list = &app->lists[list_index];
    if (list->entries_count <= 0) {
        return (Entry) {0};
    }
    assert(entry_index < list->entries_count);
    Entry entry = list->entries[entry_index];
    memmove(list->entries + entry_index, list->entries + entry_index + 1, (list->entries_count - entry_index) * sizeof(Entry));
    list->entries_count -= 1;
    list->entry_index = clamp(list->entry_index, 0, list->entries_count - 1);
    return entry;
}

void app_move_entry(TODO_App *app, TODO_List_Index from_list_index, size_t entry_index) {
    List *list = &app->lists[from_list_index];
    if (list->entries_count <= 0) {
        return;
    }
    assert(entry_index < list->entries_count);
    Entry entry = app_delete_entry(app, from_list_index, entry_index);
    TODO_List_Index to_entry_index = !from_list_index;
    app_add_entry(app, to_entry_index, entry.text, entry.text_len);
}

void app_reset_effects(TODO_App *app) {
    app->scroll_effect = 0.0f;
    app->wait_effect = 0.0f;
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

void line_edit_make_sure_cursor_inside_urmom(Line_Edit *line) {
    if (line->cursor > line->offset + line->width - 1) {
        line->offset += 1;
    } else if (line->cursor < line->offset) {
        line->offset -= 1;
    }
}

bool line_edit_all_whitespace(Line_Edit *line) {
    for (size_t i = 0; i < line->text_len; ++i) {
        if (!isspace(line->text[i])) {
            return false;
        }
    }
    return true;
}

int line_edit_handle_been(Line_Edit *line, int been) {
    if (been >= BEEN_PRINTABLE_LAST) {
        switch (been) {
        case BEEN_LEFT: {
            if (line->cursor > 0) {
                line->cursor -= 1;
            }
        } break;
        case BEEN_RIGHT: {
            if (line->cursor < line->text_len) {
                line->cursor += 1;
            }
        } break;
        case BEEN_ESC: {
            return -1;
        } break;
        case BEEN_ENTER: {
            if (line->text_len == 0) {
                return -1;
            }
            if (line_edit_all_whitespace(line)) {
                return -1;
            }
            return 1;
        } break;
        case BEEN_BACKSPACE: {
            if (line->cursor > 0) {
                memmove(line->text + line->cursor - 1, line->text + line->cursor, line->text_len - line->cursor);
                line->cursor -= 1;
                line->text_len -= 1;
            }
        } break;
        case DELETE: {
            if (line->cursor < line->text_len) {
                memmove(line->text + line->cursor, line->text + line->cursor + 1, line->text_len - line->cursor);
                line->text_len -= 1;
            }
        } break;
        case BEEN_UNKNOWN: {
            return 0;
        } break;
        }
    } else {
        if (line->text_len < TEXT_CAP) {
            memmove(line->text + line->cursor + 1, line->text + line->cursor, line->text_len - line->cursor);
            line->text[line->cursor++] = been;
            line->text_len += 1;
        }
    }
    line_edit_make_sure_cursor_inside_urmom(line);
    return 0;
}

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
                return DELETE;
            }
        }
        return BEEN_UNKNOWN;
    }
    return ch;
}

int main(void) {
#ifdef __linux__
    signal(SIGINT, sigint_handler);
#elif _WIN23
    SetConsoleCtrlHandler(console_handler, TRUE);
#endif
    disable_terminal_interaction();
    invisible_cursor();
    create_page();
    make_stdin_non_blocking();

    TODO_App app = {0};
    while (true) {
        Term_Size term_size = get_terminal_size();
        Rect term_rect = { 1, 1, term_size.cols, term_size.rows };
        Split split = split_rect(term_rect);

        int ch = fgetbeen(stdin);
        switch (app.state) {
        case TODO_STATE_IDLE: {
            if (ch == 'q') {
                handle_exit();
            } else if (ch == 'a') {
                app.state = TODO_STATE_ADD;
                app_reset_effects(&app);
            } if (ch == 'd') {
                List *list = &app.lists[app.list_index];
                app_delete_entry(&app, app.list_index, list->entry_index);
            } if (ch == BEEN_ENTER) {
                List *list = &app.lists[app.list_index];
                app_move_entry(&app, app.list_index, list->entry_index);
            } else if (ch == BEEN_UP) {
                List *list = &app.lists[app.list_index];
                if (list->entry_index > 0) {
                    list->entry_index -= 1;
                }
                app_reset_effects(&app);
            } else if (ch == BEEN_DOWN) {
                List *list = &app.lists[app.list_index];
                if (list->entry_index < list->entries_count - 1) {
                    list->entry_index += 1;
                }
                app_reset_effects(&app);
            } else if (ch == BEEN_RIGHT) {
                app.list_index = TODO_LIST_DONES;
                app_reset_effects(&app);
            } else if (ch == BEEN_LEFT) {
                app.list_index = TODO_LIST_TODOS;
                app_reset_effects(&app);
            }
        } break;
        case TODO_STATE_ADD: {
            Rect rect = (app.list_index == TODO_LIST_TODOS) ? split.left : split.right;
            app.line_edit.x = rect.x + 1;
            app.line_edit.width = rect.w - 2;
            line_edit_make_sure_cursor_inside_urmom(&app.line_edit);

            int state = line_edit_handle_been(&app.line_edit, ch);
            if (state != 0) {
                if (state > 0) {
                    app_add_entry(&app, app.list_index, app.line_edit.text, app.line_edit.text_len);
                }
                memset(&app.line_edit, 0, sizeof(Line_Edit));
                app.state = TODO_STATE_IDLE;
            }
        } break;
        }

        position_cursor(0, 0);
        draw_todo_app(&app, split);
#ifdef __linux__
        usleep(1000000.0f*delta_time);
#elif _WIN32
        Sleep(1000.0f*delta_time);
#endif
    }
    handle_exit();
    return 0;
}
