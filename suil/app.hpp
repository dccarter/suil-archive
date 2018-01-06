//
// Created by dc on 9/25/17.
//

#ifndef SUIL_APP_HPP
#define SUIL_APP_HPP

#include <suil/sys.hpp>

namespace suil {

    define_log_tag(APP_TASK);

    struct AppTask : LOGGER(dtag(APP_TASK)) {
        AppTask(const char *name)
            : name(zcstring(name).dup())
        {}

        virtual ~AppTask() {
            if (running)
                stop(EXIT_SUCCESS);
        }

    protected:
        friend struct Application;
        virtual int start() = 0;
        virtual void stop(int code = EXIT_SUCCESS) {
        }

        bool running{false};
        int  exitcode{EXIT_SUCCESS};
        zcstring name;
    };

    define_log_tag(APPLICATION);

    struct Application : LOGGER(dtag(APPLICATION)) {

        template <typename... __Opts>
        Application(const char *name, __Opts... opts)
            : name(zcstring(name).dup())
        {
            log::setup(opt(name, name));
        }

        template <typename __T, typename... __Args>
        __T& regtask(const char *name, __Args... args) {
            if (check(name)) {
                throw suil_error::create("application task '",
                            name, "' already registered.");
            }
            __T* task(new __T(name, args...));
            if (task == nullptr) {
                throw suil_error::create("unable to allocate memory for task '",
                            name, "'");
            }
            zcstring tmp(name);
            tasks.emplace(tmp.dup(), task);
            trace("task '%s' created", name);
            return *task;
        }

        int start();
        void stop(int code = EXIT_SUCCESS);

        inline void killsig(){}
        void killsig(int s);
        template <typename... __S>
        inline void killsig(int s, __S... o) {
            killsig(s);
            killsig(o...);
        }

        ~Application();

    private:
        void flush();
        bool check(const char *name);
        static coroutine void runtask(Application& app, AppTask *task);
        static coroutine void wait_notify(Application& app);

        uint32_t started{0};
        bool stopped{true};

        uint32_t running_tasks{0};
        int64_t timeout{-1};
        zcstring name;
        zmap<AppTask*>  tasks;
        Async<AppTask*> stopsync{nullptr};
        Async<int>      startwait{-EINVAL};
    };
}
#endif //SUIL_APP_HPP
