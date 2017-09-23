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

    void cvprintf(uint8_t color, int bold, const char *fmt, va_list args);

    inline void cprintf(uint8_t color, int bold, const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        cvprintf(color, bold, fmt, args);
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

        using logformat_t = std::function<size_t(char *,level, const char *, const char*, va_list)>;

        struct default_handler {
            void operator()(const char *log, size_t, level);
        };

        using logsink_t = std::function<void(const char *, size_t, level)>;

#define define_log_tag(name) \
        struct name##_log_tag {\
            static constexpr char *TAG = (char *)#name; \
        }
#define dtag(name)   name##_log_tag
        define_log_tag(SYSTEM);

        template<class __T = dtag(SYSTEM)>
        struct logger {

            void log(level l, const char *fmt, ...) const;
        };

        struct __syslog : public logger<> {
            __syslog() {
                sink =
                [&](const char *msg, size_t sz, level l) {
                    default_handler()(msg, sz, l);
                };

                formatter =
                [&](char *out, level l, const char *tag, const char *fmt,va_list args) {
                   return default_formatter()(out, l, tag, fmt, args);
                };
            }

            level getLevel() const {
                return lvl;
            }

            inline void fwdlogs(const char *log, size_t sz, level l) {
                if (sink != nullptr) {
                    sink(log, sz, l);
                }
            }

            inline size_t format(char *out, level l, const char *tag, const char *fmt,va_list args) {
                if (formatter != nullptr) {
                    return formatter(out, l, tag, fmt, args);
                }

                return 0;
            }

            inline const char *app_name() const {
                return appname;
            }

        private:
            template<typename... __Opts>
            friend void setup(__Opts...);

            logsink_t sink{nullptr};
            level lvl{level::DEBUG};
            logformat_t formatter{nullptr};
            char *appname{nullptr};
        };
    }

    extern log::__syslog &__log;

    namespace log {

        template<typename __T>
        void logger<__T>::log(level l, const char *fmt, ...) const {
            char buf[SUIL_LOG_BUF_SIZE];
            va_list args;
            va_start(args, fmt);
            size_t sz = __log.format(buf, l, __T::TAG, fmt, args);
            va_end(args);

            if (sz > 0) {
                __log.fwdlogs(buf, sz, l);
            }

            if (l == level::CRITICAL) {
                throw std::runtime_error(buf);
            }
        }

        template<typename... __Opts>
        static void setup(__Opts... opts) {
            auto options = iod::D(opts...);

            int l = options.get(sym(verbose), -1);
            if (l >= 0) {
                /* set log level */
                level lvl;
                if (l > (uint8_t) level::CRITICAL) {
                    /* invalid log level */
                    lvl = level::ERROR;
                } else {
                    lvl = (level) ((uint8_t) level::CRITICAL - l);
                }
                __log.lvl = lvl;
            }

            logsink_t sink = options.get(sym(logsink), nullptr);
            if (sink != nullptr) {
                /* set log sink */
                __log.sink = std::move(sink);
            }

            logformat_t fmt = options.get(sym(logformat), nullptr);
            if (fmt != nullptr) {
                /* set log formatter */
                __log.formatter = std::move(fmt);
            }

            const char *name = options.get(sym(name), nullptr);
            if (name) {
                if (__log.appname) {
                    /* free duplicated name */
                    free(__log.appname);
                }
                __log.appname = strdup(name);
            }
        }
    }
}

#define LOGGER(...) suil::log::logger<__VA_ARGS__>
#define __LOG(sub, l, fmt, ...)                                 \
    if (suil::__log.getLevel() <= (suil::log::level:: l))       \
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
    if (suil::__log.getLevel() <= suil::log::level::TRACE)                   \
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
