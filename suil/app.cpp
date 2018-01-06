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

    bool Application::check(const char *name) {
        zcstring tmp(name);
        return tasks.find(tmp) != tasks.end();
    }

    void Application::killsig(int s) {
        unsigned long tag = (unsigned long) 1L<<s;
        if (!(tag&__sigmask)) {
            signal(s, __sighandler);
            __sigmask |= tag;
        }
    }

    int Application::start() {
        idebug("starting application with %d tasks", tasks.size());
        if (started) {
            throw SuilError::create("application aleady started");
        }

        if (pipe(__notifyfd)) {
            ierror("opening notification pipe failed: %s", errno_s);
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
            AppTask& tmp = *it.second;
            // launch the tasks in a coroutine
            if (!tmp.running) {
                trace("launching coroutine for task-%s", tmp.name.c_str());
                started++;
                go(runtask(*this, it.second));
            }
            else {
                // running tasks shouldn't be started
                trace("task-%s already started", tmp.name.c_str());
            }
        }

        int code = EXIT_SUCCESS;
        if (started) {
            /* wait for tasks to exit */
            idebug("%u application tasks started", started);
            wait_notify(*this);
        }

        idebug("application exited, {code=%d}", code);
        return code;
    }

    void Application::wait_notify(Application& app) {
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

    void Application::stop(int code) {
        trace("stopping application, code=%d", code);
        stopped = true;

        uint32_t requested{0};
        for (auto& it : tasks) {
            AppTask& tmp = *(it.second);
            if (!tmp.running) {
                trace("task-%s is not running", tmp.name.c_str());
            }
            else {
                trace("stopping task-%s", tmp.name.c_str());
                tmp.stop(code);
                requested++;
            }
        }

        if (requested > 0) {
            idebug("{%ld} waiting for stopped %u tasks",
                  mnow(), requested);
            bool status;
            status = stopsync[timeout](requested) |
            [&](bool, AppTask *task) {
                idebug("task-%s exited {exitcode:%d}",
                      task->name.c_str(), task->exitcode);
            };

            idebug("{%ld} %u tasks exited", mnow(), requested);
            if (!status) {
                throw SuilError::create(
                        "stopping tasks timed out");
            }
        }

        close(__notifyfd[0]);
        close(__notifyfd[1]);
    }

    void Application::runtask(Application &app, AppTask *task) {
        ltrace(&app, "starting task-%s", task->name.c_str());
        app.running_tasks++;

        try {
            task->running = true;
            task->start();
        } catch (...) {
            lerror(&app, "unhandled error in task-%s", task->name.c_str());
        }

        task->running = false;
        app.running_tasks--;

        if (app.stopped) {
            // notify stop
            ltrace(&app, "sending stop signal for task-%s", task->name.c_str());
            app.stopsync << task;
        }

        if (app.running_tasks == 0) {
            // all tasks exited, close app
            close(__notifyfd[0]);
            close(__notifyfd[1]);
        }
    }

    Application::~Application() {
        if (!stopped) {
            trace("stopping application in destructor, {running:%d}", running_tasks);
            stop(0);
        }

        auto it = tasks.begin();
        while (it != tasks.end()) {
            // delete task
            try {
                trace("deleting task-%s",
                      it->second->name.c_str());
                delete it->second;
                tasks.erase(it++);
            }
            catch (SuilError& ex) {
                iwarn("delete task unhandled SuilError: %s",
                     ex.what());
            }
            catch (...) {
                ierror("delete task unknown error");
            }
        }
    }
}
