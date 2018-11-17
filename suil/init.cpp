//
// Created by dc on 12/11/18.
//
#include <syslog.h>

#include "console.h"
#include "init.h"
#include "file.h"

namespace suil {

    namespace version {
        const uint8_t MAJOR  = SUIL_MAJOR_VERSION;
        const uint8_t MINOR  = SUIL_MINOR_VERSION;
        const uint16_t PATCH = SUIL_PATCH_VERSION;
        const uint32_t BUILD = SUIL_BUILD_NUMBER;
        const char *TAG      = SUIL_BUILD_TAG;
        const char *STRING   = SUIL_VERSION_STRING;
        const char *SWNAME   = SUIL_SOFTWARE_NAME;
    };

    Version  g_version = {
            version::MAJOR,
            version::MINOR,
            version::PATCH,
            version::BUILD,
            SUIL_BUILD_TAG,
            SUIL_VERSION_STRING,
            SUIL_SOFTWARE_NAME
    };

    const Version& ver_json = g_version;

    uint8_t g_spid{0};
    const uint8_t spid = g_spid;

    SignalHandler g_sah{nullptr};

    static void suil_sah(int sig, siginfo_t *info, void *ctx) {
        if (g_sah)
            g_sah(sig, info, ctx);
        else
            swarn("received signal %d while signal handler is null");
        exit(0);
    }

    void Process_sa_init() {
        struct sigaction sa{0};
        sa.sa_sigaction = &suil_sah;
        sa.sa_flags = SA_SIGINFO | SA_NOCLDWAIT | SA_NOCLDSTOP;
        if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
            // error subscribing to child process exit
            serror("sigaction failed: %s", errno_s);
        }
        memset(&sa, '\0', sizeof(sa));
        sa.sa_sigaction = &suil_sah;
        sa.sa_flags = SA_SIGINFO | SA_NOCLDWAIT | SA_NOCLDSTOP;
        if (sigaction(SIGTERM, &sa, nullptr) == -1) {
            // error subscribing to child process exit
            serror("sigaction failed: %s", errno_s);
        }
        memset(&sa, '\0', sizeof(sa));
        sa.sa_sigaction = &suil_sah;
        sa.sa_flags = SA_SIGINFO | SA_NOCLDWAIT | SA_NOCLDSTOP;
        if (sigaction(SIGINT, &sa, nullptr) == -1) {
            // error subscribing to child process exit
            serror("sigaction failed: %s", errno_s);
        }
        memset(&sa, '\0', sizeof(sa));
        sa.sa_sigaction = &suil_sah;
        sa.sa_flags = SA_SIGINFO | SA_NOCLDWAIT | SA_NOCLDSTOP;
        if (sigaction(SIGQUIT, &sa, nullptr) == -1) {
            // error subscribing to child process exit
            serror("sigaction failed: %s", errno_s);
        }
        memset(&sa, '\0', sizeof(sa));
        sa.sa_sigaction = &suil_sah;
        sa.sa_flags = SA_SIGINFO | SA_NOCLDWAIT | SA_NOCLDSTOP;
        if (sigaction(SIGABRT, &sa, nullptr) == -1) {
            // error subscribing to child process exit
            serror("sigaction failed: %s", errno_s);
        }
        memset(&sa, '\0', sizeof(sa));
        sa.sa_sigaction = &suil_sah;
        sa.sa_flags = SA_SIGINFO | SA_NOCLDWAIT | SA_NOCLDSTOP;
        if (sigaction(SIGHUP, &sa, nullptr) == -1) {
            // error subscribing to child process exit
            serror("sigaction failed: %s", errno_s);
        }
    }

    bool load(bool si) {
        static bool loaded{false};
        if (loaded) return false;
        //signal(SIGPIPE, SIG_IGN);
        if (si) {
            // display version only if explicitly requested
            console::println("");
            console::println("Powered by suil C++1y web framework");
            console::red("v" SUIL_VERSION_STRING "\n");
            console::blue("http://suil.suilteam.com\n");
            console::println("---------------------------------------\n");
        }
        Process_sa_init();
        loaded = true;

        return loaded;
    }

    namespace __internal {

        void chwdir(const std::string &to) {
            if (!utils::fs::isdir(to.c_str())) {
                throw Exception::create("working directory: '", to, "' does not exist");
            }

            if (::chdir(to.c_str())) {
                throw Exception::create("setting working dir to('", to, "') failed: ", errno_s);
            }
        }

        void daemonize(const std::string &wdir) {
            pid_t pid, sid;
            pid = fork();
            if (pid < 0) {
                console::red("error - daemonizing failed: %s\n", errno_s);
                exit(EXIT_FAILURE);
            }

            if (pid > 0) {
                // close parent
                exit(EXIT_SUCCESS);
            }

            /*Change file mode mask */
            umask(0);

            openlog("suil", LOG_PID | LOG_CONS, LOG_USER);
            // temporarily redirect logs to syslogs
            log::setup(var(sink) = [](const char *d, size_t s, log::Level l) {
                syslog(LOG_INFO, "%s", d);
            });

            sid = setsid();
            if (sid < 0) {
                serror("Creating Session ID for daemon failed: %s", errno_s);
                closelog();
                exit(EXIT_FAILURE);
            }
            sinfo("Created Session ID %d for suil daemon", sid);

            std::string tmp(wdir);
            if (wdir.empty())
                tmp = "/";
            try {
                chwdir(tmp);
            }
            catch (...) {
                serror("changing work director to '%s' failed: %s", tmp.c_str(), errno_s);
                closelog();
                exit(EXIT_FAILURE);
            }

            sinfo("suil application demonized %d", sid);
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            /* set pid file */
            pid = getpid();
            if (utils::fs::exists("suil.pid")) {
                utils::fs::remove("suil.pid");
            }
            utils::fs::append("suil.pid", pid);
        }
    }
}
