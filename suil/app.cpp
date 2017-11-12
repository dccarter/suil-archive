//
// Created by dc on 9/26/17.
//
#include <sys/wait.h>
#include <suil/app.hpp>

namespace suil {

    static int    __notifyfd[2];
    unsigned long __sigmask{0};

    static void __sighandler(int sig) {
        sdebug("signal %d received", sig);
        if (write(__notifyfd[1], &sig, sizeof(sig)) < 0) {
            /* writing signal failed */
            serror("[%d] notifying signal %d failed: %s",
                   __notifyfd[0], sig, errno_s);
            exit(errno);
        }
    }

    bool application::check(const char *name) {
        zcstring tmp(name);
        return tasks.find(name) != tasks.end();
    }

    void application::killsig(int s) {
        unsigned long tag = (unsigned long) 1L<<s;
        if (!(tag&__sigmask)) {
            signal(s, __sighandler);
            __sigmask |= tag;
        }
    }

    int application::start() {
        debug("starting application with %d tasks", tasks.size());
        if (started) {
            throw suil_error::create("application aleady started");
        }

        if (pipe(__notifyfd)) {
            error("opening notification pipe failed: %s", errno_s);
            return EXIT_FAILURE;
        }

        /* register signal handlers */
        signal(SIGHUP,  __sighandler);
        signal(SIGQUIT, __sighandler);
        signal(SIGTERM, __sighandler);
        signal(SIGABRT, __sighandler);
        signal(SIGPIPE, SIG_IGN);

        started = 0;
        stopped = false;

        for (auto& it: tasks) {
            app_task& tmp = *it.second;
            // launch the tasks in a coroutine
            if (!tmp.running) {
                trace("launching coroutine for task-%s", tmp.name.cstr);
                started++;
                go(runtask(*this, it.second));
            }
            else {
                // running tasks shouldn't be started
                trace("task-%s already started", tmp.name.cstr);
            }
        }

        int code = EXIT_SUCCESS;
        if (started) {
            /* wait for tasks to exit */
            debug("%u application tasks started", started);
            wait_notify(*this);
        }

        debug("application exited, {code=%d}", code);
        return code;
    }

    void application::wait_notify(application& app) {
        ldebug(&app, "wait exit notification coroutine started");
        int ev = fdwait(__notifyfd[0], FDW_IN, -1);
        if (ev == FDW_IN) {
            ltrace(&app, "read notify_fd[0] %d", ev);
            /* signal ready to read */
            int sig{0};
            if (read(__notifyfd[0], &sig, sizeof(sig)) > 0) {
                /* received signal, notify main loop */
                ltrace(&app, "signal %d received, stopping application", sig);
                app.stop(sig);
                return;
            }
        }
        ltrace(&app, "waiting for notice signal signal failed: %s", errno_s);
    }

    void application::stop(int code) {
        trace("stopping application, code=%d", code);
        stopped = true;

        uint32_t requested{0};
        for (auto& it : tasks) {
            app_task& tmp = *(it.second);
            if (!tmp.running) {
                trace("task-%s is not running", tmp.name.cstr);
            }
            else {
                trace("stopping task-%s", tmp.name.cstr);
                tmp.stop(code);
                requested++;
            }
        }

        if (requested > 0) {
            debug("{%ld} waiting for stopped %u tasks",
                  mnow(), requested);
            bool status;
            status = stopsync[timeout](requested) |
            [&](bool, app_task *task) {
                debug("task-%s exited {exitcode:%d}",
                      task->name.cstr, task->exitcode);
            };

            debug("{%ld} %u tasks exited", mnow(), requested);
            if (!status) {
                throw suil_error::create(
                        "stopping tasks timed out");
            }
        }

        close(__notifyfd[0]);
        close(__notifyfd[1]);
    }

    void application::runtask(application &app, app_task *task) {
        ltrace(&app, "starting task-%s", task->name.cstr);
        app.running_tasks++;

        try {
            task->running = true;
            task->start();
        } catch (...) {
            lerror(&app, "unhandled error in task-%s", task->name.cstr);
        }

        task->running = false;
        app.running_tasks--;

        if (app.stopped) {
            // notify stop
            ltrace(&app, "sending stop signal for task-%s", task->name.cstr);
            app.stopsync << task;
        }

        if (app.running_tasks == 0) {
            // all tasks exited, close app
            close(__notifyfd[0]);
            close(__notifyfd[1]);
        }
    }

    application::~application() {
        if (!stopped) {
            trace("stopping application in destructor, {running:%d}", running_tasks);
            stop(0);
        }

        auto it = tasks.begin();
        while (it != tasks.end()) {
            // delete task
            try {
                trace("deleting task-%s",
                      it->second->name.cstr);
                delete it->second;
                tasks.erase(it++);
            }
            catch (suil_error& ex) {
                warn("delete task unhandled suil_error: %s",
                     ex.what());
            }
            catch (...) {
                error("delete task unknown error");
            }
        }
    }
}
