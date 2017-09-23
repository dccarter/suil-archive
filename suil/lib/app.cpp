//
// Created by dc on 8/30/17.
//
#include <signal.h>
#include <runapp.hpp>
#include <sys/param.h>
#include "app.hpp"

namespace suil {

    int __notify_fd[2];

    void __app_signal(int sig) {
        sdebug("signal %d received", sig);
        if (write(__notify_fd[1], &sig, 1) < 0) {
            /* writing signal failed */
            serror("notifying signal %d failed: %s", sig, errno_s);
            exit(errno);
        }
    }

    int application::start() {
        info("application starting...");

        if (pipe(__notify_fd)) {
            error("opening notification pipe failed: %s", errno_s);
            return EXIT_FAILURE;
        }

        /* register signal handlers */
        signal(SIGHUP,  __app_signal);
        signal(SIGQUIT, __app_signal);
        signal(SIGTERM, __app_signal);
        if (!mgr.cmd_args.background) {
            /* application can be stopped by CTR-C or SIGINT */
            signal(SIGINT, __app_signal);
        }
        else {
            /* application running in background, cannot be stopped by CTR-C or SIGINT*/
            signal(SIGINT, SIG_IGN);
        }
        signal(SIGPIPE, SIG_IGN);

        int started = 0;
        /* start all the task asynchronously */
        for (auto& it : tasks) {
            apptask *task = it.second;
            debug("scheduling task {%s:%d}", task->name.cstr, task->tid);

            go(async_task(*this, task));
            started++;
        }

        /* wait for notification in a loop */
        int rc{0};
        if (started > 0) {
            debug("application started, executing %d tasks", started);
            /* go wait for exit signal */
            go (wait_notify(*this));

            if (!(exitchan >> rc)) {
                error("waiting for application to exit failed: %s", errno_s);
                rc = EXIT_FAILURE;
            }
        }

        return rc;
    }

    void application::stop(int code) {
        info("stop application requested", name.cstr, aid);
        int stopped = 0;
        stopping = true;

        for (auto& it: tasks) {
            apptask *t = it.second;
            if (t->running) {
                info("stopping task {%s}", t->name.cstr);
                t->stop();
                stopped++;
            }
            else {
                debug("task {%s} already stopped", t->name.cstr);
            }
        }

        /* send a stop notification */
        close(__notify_fd[0]);
        close(__notify_fd[1]);

        if (stopped) {
            /* wait for tasks to exit */
            debug("waiting for {%d} tasks to exit", stopped);
            bool status =
                    waiter[stop_timeout](stopped) |
                    [&](bool, apptask *t) {
                        /* task exited */
                        info("task {%s} exited", t->name.cstr);
                    };

            if (!status) {
                /*some tasks might not have exited */
                warn("some tasks might not have exited");
            }
            exitchan << code;
        }
    }

    void application::wait_notify(application& app) {
        ldebug(&app, "wait exit notification coroutine started");
        int ev = fdwait(__notify_fd[0], FDW_IN, -1);
        if (ev == FDW_IN) {
            ltrace(&app, "read notify_fd[0] %d", ev);
            /* signal ready to read */
            int sig{0};
            if (read(__notify_fd[0], &sig, sizeof(sig)) > 0) {
                /* received signal, notify main loop */
                ltrace(&app, "signal %d received, stopping application", sig);
                app.stop(sig);
                return;
            }
        }
        ltrace(&app, "waiting for notice signal signal failed: %s", errno_s);
    }

    void application::async_task(application &app, apptask *t) {
        ltrace(&app, "running {%s:%d} in coroutine", t->name.cstr, t->tid);
        int appid = app.aid, taskid = t->tid;
        app.running_tasks++;
        try {
            t->running = true;
            t->start();
        } catch (...) {
            /* unhandled exception */
            serror("unhandled exception in task {app:%d, task: %d}", appid, taskid);
        }
        t->running = false;
        app.running_tasks--;

        strace("stopping task {%d:%d} in coroutine {%d:%d}",
                      appid, taskid, app.stopping, app.running_tasks);

        if (app.stopping) {
            /* notify application that task exited */
            app.waiter << t;
        }
        else if (app.running_tasks == 0) {
            /* special handling case, all tasks exited, exit */
            close(__notify_fd[0]);
            close(__notify_fd[1]);
            /* notify application */
            app.exitchan << 0;
        }
    }

    int application::verbosity() const {
        return MIN(6, mgr.cmd_args.verbose);
    }

    const std::string& application::wdir() const {
        return mgr.cmd_args.base;
    }

    const std::string& application::binary() const {
        return mgr.cmd_args.base;
    }

    application::~application() {
        /* destroy application */
        if (running_tasks > 0) {
            /* stop the application */
            stop(0);
        }

        /* delete all tasks*/
        auto it = tasks.begin();
        while (it != tasks.end()) {
            /* delete task memory and erase from map */
            delete it->second;
            tasks.erase(it++);
        }
    }
}