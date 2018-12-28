//
// Created by dc on 10/11/18.
//

#include "console.h"

namespace suil {

    void console::cprintv(uint8_t color, int bold, const char *fmt, va_list args) {
        if (color > 0 && color <= console::CYAN)
            printf("\033[%s3%dm", (bold? "1;" : ""), color);
        (void) vprintf(fmt, args);
        if (color > 0 && color <= console::CYAN)
            printf("\033[0m");
    }

    void console::cprintf(uint8_t color, int bold, const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        cprintv(color, bold, fmt, args);
        va_end(args);
    }


    void console::println(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
    }

    void console::red(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        cprintv(console::RED, 0, fmt, args);
        va_end(args);
    }

    void console::blue(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        cprintv(console::BLUE, 0, fmt, args);
        va_end(args);
    }

    void console::green(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        cprintv(console::GREEN, 0, fmt, args);
        va_end(args);
    }

    void console::yellow(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        cprintv(console::YELLOW, 0, fmt, args);
        va_end(args);
    }

    void console::magenta(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        cprintv(console::MAGENTA, 0, fmt, args);
        va_end(args);
    }

    void console::cyan(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        cprintv(console::CYAN, 0, fmt, args);
        va_end(args);
    }

    void console::log(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        printf(fmt, args); printf("\n");
        printf("\n");
        va_end(args);
    }

    void console::info(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        cprintv(console::GREEN, 0, fmt, args);
        printf("\n");
        va_end(args);
    }

    void console::error(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        cprintv(console::RED, 0, fmt, args);
        printf("\n");
        va_end(args);
    }

    void console::warn(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        cprintv(console::YELLOW, 0, fmt, args);
        printf("\n");
        va_end(args);
    }
}