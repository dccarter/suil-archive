//
// Created by dc on 31/05/17.
//
#include <stdio.h>
#include <unistd.h>

#include <suil/sys.hpp>
#include <suil/log.hpp>

namespace suil {

    static log::__syslog SYS_LOGGER;
    log::__syslog& __log = SYS_LOGGER;

    void cvprintf(uint8_t color, int bold, const char *fmt, va_list args) {
        if (color > 0 && color <= printf_colors::CYAN)
            printf("\033[%s3%dm", (bold? "1;" : ""), color);
        (void) vprintf(fmt, args);
        if (color > 0 && color <= printf_colors::CYAN)
            printf("\033[0m");
    }

    namespace log {

        size_t default_formatter::operator()(
                char *out,
                level l,
                const char *tag,
                const char *fmt,
                va_list args)
        {
            const static char *LOGLVL_STR[] = {
                "TRC", "DBG", "INF", "NTC", "WRN", "ERR", "CRT"
            };

            size_t sz = SUIL_LOG_BUF_SIZE;
            char   *tmp = out;
            const char *name = __log.app_name()? __log.app_name() : "global";
            int     wr = 0;

            switch (l)
            {
                case level::DEBUG:
                case level::ERROR:
                case level::CRITICAL:
                case level::WARNING:
                    wr = snprintf(tmp, sz, "%s: [%s] [%3s] [%10.10s] ",
                                  name, datetime()(), LOGLVL_STR[(unsigned char)l], tag);
                    break;
                default:
                    wr = snprintf(tmp, sz, "%s: ",name);
                    break;
            }

            sz  -= wr;
            wr += vsnprintf(tmp + wr, sz, fmt, args);
            tmp[wr++] = '\n';
            tmp[wr]   = '\0';

            return (size_t) wr;
        }

        void default_handler::operator()(const char *log, size_t sz, level l) {
            printf_colors c = printf_colors::DEFAULT;
            int bold = 0;
            switch (l) {
                case level::CRITICAL:
                case level::ERROR:
                    c = printf_colors::RED; break;
                case level::WARNING:
                    c = printf_colors::YELLOW; break;
                case level::DEBUG:
                    c = printf_colors::MAGENTA; break;
                case level::INFO:
                    c = printf_colors::WHITE;
                    break;
                case level::NOTICE:
                    c = printf_colors::GREEN;
                    bold = 1;
                    break;
                default:
                    c = printf_colors::DEFAULT;
                    break;
            }
            cprintf(c, bold, log);
        }
    }
}