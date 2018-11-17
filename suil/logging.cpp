//
// Created by dc on 10/11/18.
//

#ifdef SUIL_BACKTRACE
#include <execinfo.h> // for backtrace
#include <dlfcn.h>    // for dladdr
#include <cxxabi.h>   // for __cxa_demangle
#endif


#include <syslog.h>
#include "logging.h"
#include "console.h"

namespace suil {

#ifdef SUIL_BACKTRACE
    void backtrace(char *buf, size_t size) {
        void *callstack[128];
        const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
        if (size == 0 or buf == nullptr) {
            // use static buffer
            static char LOCAL[1024];
            buf = local; size = sizeof(LOCAL);
        }
        int nFrames = ::backtrace(callstack, nMaxFrames);
        char **symbols = backtrace_symbols(callstack, nFrames);

        std::ostringstream trace_buf;
        for (int i = 1; i < nFrames; i++) {
            printf("%s\n", symbols[i]);

            Dl_info info;
            if (dladdr(callstack[i], &info) && info.dli_sname) {
                char *demangled = NULL;
                int status = -1;
                if (info.dli_sname[0] == '_')
                    demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
                snprintf(buf, sizeof(buf), "%-3d %*p %s + %zd\n",
                         i, int(2 + sizeof(void*) * 2), callstack[i],
                         status == 0 ? demangled :
                         info.dli_sname == 0 ? symbols[i] : info.dli_sname,
                         (char *)callstack[i] - (char *)info.dli_saddr);
                free(demangled);
            } else {
                snprintf(buf, sizeof(buf), "%-3d %*p %s\n",
                         i, int(2 + sizeof(void*) * 2), callstack[i], symbols[i]);
            }
            trace_buf << buf;
        }
        free(symbols);
        printf("%s\n", trace_buf.str().data());
    }
#endif

    static log::__Logger SYS_LOGGER;
    log::__Logger& __Log = SYS_LOGGER;

    namespace log {

        size_t Formatter::operator()(
                char *out,
                Level l,
                const char *tag,
                const char *fmt,
                va_list args)
        {
            const static char *LOGLVL_STR[] = {
                    "TRC", "DBG", "INF", "NTC", "WRN", "ERR", "CRT"
            };

            size_t sz = SUIL_LOG_BUFFER_SIZE;
            char   *tmp = out;
            const char *name = __Log.app_name()? __Log.app_name() : "global";
            int     wr = 0;

            switch (l)
            {
                case log::DEBUG:
                case log::ERROR:
                case log::CRITICAL:
                case log::WARNING:
                    wr = snprintf(tmp, sz, "%s/%05d: [%s] [%3s] [%10.10s] ",
                                  name, getpid(), Datetime()(), LOGLVL_STR[(unsigned char)l], tag);
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

        void Handler::operator()(const char *log, size_t sz, Level l) {
            auto c = console::DEFAULT;
            int bold = 0;
            switch (l) {
                case log::CRITICAL:
                case log::ERROR:
                    c = console::RED; break;
                case log::WARNING:
                    c = console::YELLOW; break;
                case log::DEBUG:
                    c = console::MAGENTA; break;
                case log::INFO:
                    c = console::WHITE;
                    break;
                case log::NOTICE:
                    c = console::GREEN;
                    bold = 1;
                    break;
                default:
                    c = console::DEFAULT;
                    break;
            }
            cprintf(c, bold, log);
        }
    }

    Syslog::Syslog(const char *name) {
        openlog("suil", LOG_PID|LOG_CONS, LOG_USER);
    }

    void Syslog::close() {
        closelog();
    }

    void Syslog::log(const char *msg, size_t, log::Level l) {
        int prio{LOG_INFO};
        switch(l) {
            case log::Level::DEBUG:
            case log::Level::TRACE:
                prio = LOG_DEBUG; break;
            case log::Level::NOTICE:
                prio = LOG_NOTICE; break;
            case log::Level::ERROR:
                prio = LOG_ERR; break;
            case log::Level::WARNING:
                prio = LOG_WARNING; break;
            case log::Level::CRITICAL:
                prio = LOG_CRIT; break;
            default:
                break;
        }
        syslog(prio, msg);
    }
}