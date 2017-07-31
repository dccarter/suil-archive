//
// Created by dc on 31/05/17.
//

#ifndef SUIL_LOG_HPP
#define SUIL_LOG_HPP

#include <stdarg.h>
#include <functional>
#include <string>

#ifndef SUIL_LOG_BUF_SIZE
#define SUIL_LOG_BUF_SIZE (2048)
#endif

namespace suil {

    enum printf_colors : uint8_t {
        WHITE = 0,
        RED,
        GREEN,
        YELLOW,
        BLUE,
        MAGENTA,
        CYAN,
        DEFAULT
    };

    void cvprintf(uint8_t color, const char *fmt, va_list args);

    inline void cprintf(uint8_t color, const char *fmt, ...) {
        va_list    args;
        va_start(args, fmt);
        cvprintf(color, fmt, args);
        va_end(args);
    }

    namespace log {
        enum struct level : unsigned char {
            TRACE,
            DEBUG,
            INFO,
            NOTICE,
            WARNING,
            ERROR,
            CRITICAL
        };

        struct default_formatter {
            size_t operator()(char *out,
                            level l,
                            const char *tag,
                            const char *fmt,
                            va_list args);
        };

        struct default_handler {
            void operator()(const char *log, size_t, level);
        };

        #define define_log_tag(name) \
        struct name##_log_tag {\
            static constexpr char *TAG = (char *)#name; \
        }
        #define dtag(name)   name##_log_tag
        define_log_tag(SYSTEM);

        template <class __T = dtag(SYSTEM),
                  class __H = default_handler,
                  class __F = default_formatter>
        struct logger {
            logger(level l)
            {
                setLevel(l);
            }

            logger()
                : logger(level::INFO)
            {}

            void log(level l, const char *fmt, ...) const {
                char buf[SUIL_LOG_BUF_SIZE];
                va_list  args;
                va_start(args, fmt);
                size_t  sz = __F()(buf, l, __T::TAG, fmt, args);
                va_end(args);

                __H()(buf, sz, l);

                if (l == level::CRITICAL) {
                    throw std::runtime_error(buf);
                }
            }

            void setLevel(level l);

            inline level getLevel() const {
                return lvl;
            }

        private:
            level lvl;
        };
    };

    extern suil::log::logger<>& __log;
    template <class __T, class __F, class __H>
    void log::logger<__T,__F,__H>::setLevel(log::level l) {
        if ((void*)this == (void*)&__log || __log.getLevel() <= l) {
            lvl = l;
        } else {
            lvl = __log.getLevel();
        }
    }
}

#define LOGGER(...) suil::log::logger<__VA_ARGS__>
#define __LOG(sub, l, fmt, ...)                                 \
    if ((sub)->getLevel() <= (suil::log::level:: l))            \
        (sub)->log(suil::log::level:: l , fmt , ##__VA_ARGS__)

#define ldebug(sub, fmt, ...)    __LOG(sub, DEBUG, fmt, ##__VA_ARGS__)
#define debug(fmt, ...)          __LOG(this, DEBUG, fmt, ##__VA_ARGS__)
#define sdebug(fmt, ...)         __LOG(&suil::__log, DEBUG, fmt, ##__VA_ARGS__)
#define lwarn(sub, fmt, ...)     __LOG(sub, WARNING, fmt, ##__VA_ARGS__)
#define warn(fmt, ...)           __LOG(this, WARNING, fmt, ##__VA_ARGS__)
#define swarn(fmt, ...)          __LOG(&suil::__log, WARNING, fmt, ##__VA_ARGS__)
#define linfo(sub, fmt, ...)     __LOG(sub, INFO, fmt, ##__VA_ARGS__)
#define info(fmt, ...)           __LOG(this, INFO, fmt, ##__VA_ARGS__)
#define sinfo(fmt, ...)          __LOG(&suil::__log, INFO, fmt, ##__VA_ARGS__)
#define lnotice(sub, fmt, ...)   __LOG(sub, NOTICE, fmt, ##__VA_ARGS__)
#define notice(fmt, ...)         __LOG(this, NOTICE, fmt, ##__VA_ARGS__)
#define snotice(fmt, ...)        __LOG(&suil::__log, NOTICE, fmt, ##__VA_ARGS__)
#define lerror(sub, fmt, ...)    __LOG(sub, ERROR, fmt, ##__VA_ARGS__)
#define error(fmt, ...)          __LOG(this, ERROR, fmt, ##__VA_ARGS__)
#define serror(fmt, ...)         __LOG(&suil::__log, ERROR, fmt, ##__VA_ARGS__)
#define lcritical(sub, fmt, ...) __LOG(sub, CRITICAL, fmt, ##__VA_ARGS__)
#define critical(fmt, ...)       __LOG(this, CRITICAL, fmt, ##__VA_ARGS__)
#define scritical(fmt, ...)      __LOG(&suil::__log, CRITICAL, fmt, ##__VA_ARGS__)

#ifndef SUIL_ENABLE_TRACE

#define ltrace(l, fmt, ...)                                           \
    if ((l)->getLevel() <= suil::log::level::TRACE)                   \
        (l)->log(suil::log::level::TRACE, "%s:%d " fmt, __FILE__,     \
                    __LINE__, ##__VA_ARGS__)

#define trace(fmt, ...)  ltrace(this, fmt, ##__VA_ARGS__)
#define strace(fmt, ...) ltrace(&suil::__log, fmt, ##__VA_ARGS__)

#else

#define ltrace(l, fmt, ...)
#define trace(fmt, ...)
#define strace(fmt, ...)

#endif

#endif //SUIL_LOG_HPP
