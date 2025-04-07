#ifndef TERM_H_
#define TERM_H_

#include <stdlib.h>

#ifdef __linux__
#    include <unistd.h>
#    include <signal.h>
#    include <termios.h>
#    include <fcntl.h>
#    include <sys/ioctl.h>
#elif _WIN32
#    include <windows.h>
#else
#    error "OS not supported"
#endif

#ifdef __linux__
    extern struct termios tio;
#elif _WIN32
    extern HANDLE console;
    extern DWORD mode;
#endif

typedef struct {
    size_t rows;
    size_t cols;
} Term_Size;

void visible_cursor(void);
void invisible_cursor(void);
void position_cursor(size_t s, size_t y);
void create_page(void);
void delete_page(void);
void set_bg_color(int bg, int fg);
void reset_bg_color(void);
Term_Size get_terminal_size(void);

void disable_terminal_interaction(void);
void enable_terminal_interaction(void);

void make_stdin_non_blocking(void);

#endif // TERM_H_

#ifdef TERM_IMPLEMENTATION

#ifdef __linux__
    struct termios tio = {0};
#elif _WIN32
    HANDLE console;
    DWORD mode;
#endif

void disable_terminal_interaction(void) {
#ifdef __linux__
    struct termios new_tio;
    tcgetattr(STDIN_FILENO, &tio);
    new_tio = tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
#elif _WIN32
    console = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(console, &mode);
    SetConsoleMode(console, mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
#endif
}

void enable_terminal_interaction(void) {
#ifdef __linux__
    tcsetattr(STDIN_FILENO, TCSANOW, &tio);
#elif _WIN32
    SetConsoleMode(console, mode);
#endif
}

void make_stdin_non_blocking(void) {
#ifdef __linux__
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
#endif
}

void visible_cursor(void) {
    printf("\033[?25h");
}

void invisible_cursor(void) {
    printf("\033[?25l");
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

Term_Size get_terminal_size(void) {
    Term_Size term_size = {0};
#ifdef __linux__
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) < 0) {
        fprintf(stderr, "Error: could not get terminal size: %s\n", strerror(errno));
        exit(1);
    }
    term_size.rows = w.ws_row;
    term_size.cols = w.ws_col;
#elif _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        fprintf(stderr, "Error: could not get terminal size\n");
        exit(1);
    }
    term_size.cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    term_size.rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#endif
    return term_size;
}

#endif // TERM_IMPLEMENTATION
