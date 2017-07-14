//
// Created by dc on 31/05/17.
//
#include <stdio.h>
#include <unistd.h>

#include "sys.hpp"
#include "log.hpp"

namespace suil {

    static log::logger<> SYS_LOGGER;
    log::logger<>& __log = SYS_LOGGER;

    namespace log {

        size_t default_formatter::operator()(
                char *out,
                level l,
                const char *tag,
                const char *fmt,
                va_list args)
        {
            const static char *LOGLVL_STR[] = {
                "TRACE", "DEBUG", "INFO", "NOTICE", "WARNING", "ERROR", "CRITICAL"
            };

            size_t sz = SUIL_LOG_BUF_SIZE;
            char   *tmp = out;
            int     wr = 0;

            // when printing notice, don't add date and notice
            if (l == level::NOTICE || l == level::TRACE) {
                wr = snprintf(tmp, sz, "suil-%hhu: ", spid);
            }
            else {
                wr = snprintf(tmp, sz, "suil-%hhu: %s [%8.8s] [%10.10s] ",
                            spid, datetime()(), LOGLVL_STR[(unsigned char)l], tag);
            }
            sz  -= wr;

            wr += vsnprintf(tmp + wr, sz, fmt, args);
            tmp[wr++] = '\n';
            tmp[wr]   = '\0';

            return (size_t) wr;
        }

        void default_handler::operator()(const char *log, size_t sz, level l) {
            if (l <= level::NOTICE)
                ::write(STDOUT_FILENO, log, sz);
            else
                ::write(STDERR_FILENO, log, sz);
        }
    }
}