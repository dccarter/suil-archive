//
// Created by dc on 31/05/17.
//

#ifndef SUIL_LOG_HPP
#define SUIL_LOG_HPP

#include <stdarg.h>
#include <functional>
#include <string.h>

#include <suil/symbols.h>
#include <suil/config.hpp>

#ifndef SUIL_LOG_BUF_SIZE
#define SUIL_LOG_BUF_SIZE (2048)
#endif

#define var(v) s::_##v
#define sym(v) var(v)
#define opt(o, v) var(o) = v
#define on(ev) s::_on_##ev
#define sizeofcstr(ch)   (sizeof(ch)-1)

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

    void backtrace();

    namespace console {

        inline void println(const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            vprintf(fmt, args);
            va_end(args);
            printf("\n");
        }

        inline void printred(const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            cvprintf(printf_colors::RED, 0, fmt, args);
            va_end(args);
        }

        inline void printblue(const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            cvprintf(printf_colors::BLUE, 0, fmt, args);
            va_end(args);
        }

        inline void printgreen(const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            cvprintf(printf_colors::GREEN, 0, fmt, args);
            va_end(args);
        }

        inline void printyellow(const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            cvprintf(printf_colors::YELLOW, 0, fmt, args);
            va_end(args);
        }

        inline void printmagenta(const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            cvprintf(printf_colors::MAGENTA, 0, fmt, args);
            va_end(args);
        }

        inline void printcyan(const char *fmt, ...) {
            va_list args;
            va_start(args, fmt);
            cvprintf(printf_colors::CYAN, 0, fmt, args);
            va_end(args);
        }
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

        using LogFormat = std::function<size_t(char *,level, const char *, const char*, va_list)>;

        struct DefaultHandler {
            void operator()(const char *log, size_t, level);
        };

        using LogSink = std::function<void(const char *, size_t, level)>;

#define define_log_tag(name) \
        struct name##_log_tag {\
            static constexpr char *TAG = (char *)#name; \
        }
#define dtag(name)   name##_log_tag
        define_log_tag(SYSTEM);

        template<class __T = dtag(SYSTEM)>
        struct Logger {
            Logger()
            {}
            Logger(const char *tag)
                : tag(::strdup(tag))
            {}

            void log(level l, const char *fmt, ...) const;

            virtual ~Logger() {
                if (tag) {
                    free(tag);
                    tag = nullptr;
                }
            }

        protected:
            LogSink  psink{nullptr};
        private:
            char *tag{nullptr};
        };

        struct __syslog : public Logger<> {
            __syslog() {
                sink =
                [&](const char *msg, size_t sz, level l) {
                    DefaultHandler()(msg, sz, l);
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
                if (psink = nullptr) {
                    psink(log, sz, l);
                }
                else if (sink != nullptr) {
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

            LogSink sink{nullptr};
            level lvl{level::DEBUG};
            LogFormat formatter{nullptr};
            char *appname{nullptr};
        };
    }

    extern log::__syslog &__log;

    namespace log {

        template<typename __T>
        void Logger<__T>::log(level l, const char *fmt, ...) const {
            char buf[SUIL_LOG_BUF_SIZE];
            va_list args;
            va_start(args, fmt);
            size_t sz = __log.format(buf, l, __T::TAG, fmt, args);
            va_end(args);

            if (sz > 0) {
                __log.fwdlogs(buf, sz, l);
            }

            if (l == level::CRITICAL) {
                // print stack trace
                backtrace();
                exit(EXIT_FAILURE);
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

            LogSink sink = options.get(sym(logsink), nullptr);
            if (sink != nullptr) {
                /* set log sink */
                __log.sink = std::move(sink);
            }

            LogFormat fmt = options.get(sym(logformat), nullptr);
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
                __log.appname = ::strdup(name);
            }
        }
    }
}

#define LOGGER(...) suil::log::Logger<__VA_ARGS__>
#define __LOG(sub, l, fmt, ...)                                 \
    if (suil::__log.getLevel() <= (suil::log::level:: l))       \
        (sub)->log(suil::log::level:: l , fmt , ##__VA_ARGS__)

#define ldebug(sub, fmt, ...)    __LOG(sub, DEBUG, fmt, ##__VA_ARGS__)
#define idebug(fmt, ...)         __LOG(this, DEBUG, fmt, ##__VA_ARGS__)
#define sdebug(fmt, ...)         __LOG(&suil::__log, DEBUG, fmt, ##__VA_ARGS__)
#define lwarn(sub, fmt, ...)     __LOG(sub, WARNING, fmt, ##__VA_ARGS__)
#define iwarn(fmt, ...)           __LOG(this, WARNING, fmt, ##__VA_ARGS__)
#define swarn(fmt, ...)          __LOG(&suil::__log, WARNING, fmt, ##__VA_ARGS__)
#define linfo(sub, fmt, ...)     __LOG(sub, INFO, fmt, ##__VA_ARGS__)
#define iinfo(fmt, ...)           __LOG(this, INFO, fmt, ##__VA_ARGS__)
#define sinfo(fmt, ...)          __LOG(&suil::__log, INFO, fmt, ##__VA_ARGS__)
#define lnotice(sub, fmt, ...)   __LOG(sub, NOTICE, fmt, ##__VA_ARGS__)
#define inotice(fmt, ...)         __LOG(this, NOTICE, fmt, ##__VA_ARGS__)
#define snotice(fmt, ...)        __LOG(&suil::__log, NOTICE, fmt, ##__VA_ARGS__)
#define lerror(sub, fmt, ...)    __LOG(sub, ERROR, fmt, ##__VA_ARGS__)
#define ierror(fmt, ...)          __LOG(this, ERROR, fmt, ##__VA_ARGS__)
#define serror(fmt, ...)         __LOG(&suil::__log, ERROR, fmt, ##__VA_ARGS__)
#define lcritical(sub, fmt, ...) __LOG(sub, CRITICAL, fmt, ##__VA_ARGS__)
#define icritical(fmt, ...)       __LOG(this, CRITICAL, fmt, ##__VA_ARGS__)
#define scritical(fmt, ...)      __LOG(&suil::__log, CRITICAL, fmt, ##__VA_ARGS__)

#if SUIL_ENABLE_TRACE

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
