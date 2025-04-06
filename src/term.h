#ifndef TERM_H_
#define TERM_H_

void visible_cursor(void);
void invisible_cursor(void);
void clear_term(void);
void position_cursor(size_t s, size_t y);
void create_page(void);
void delete_page(void);
void set_bg_color(int bg, int fg);
void reset_bg_color(void);

#endif // TERM_H_

#ifdef TERM_IMPLEMENTATION

void visible_cursor(void) {
    printf("\033[?25h");
}

void invisible_cursor(void) {
    printf("\033[?25l");
}

void clear_term(void) {
    printf("\033[2J");
    printf("\033[0;0H");
}

void position_cursor(size_t x, size_t y) {
    printf("\033[%zu;%zuH", y, x);
}

void create_page(void) {
    printf("\033[?1049h");
}

void delete_page(void) {
    printf("\033[?1049l");
}

void set_bg_color(int bg, int fg) {
    printf("\033[%d;%dm", bg, fg);
}

void reset_bg_color(void) {
    printf("\033[0m");
}

#endif // TERM_IMPLEMENTATION
