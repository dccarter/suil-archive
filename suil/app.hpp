//
// Created by dc on 9/25/17.
//

#ifndef SUIL_APP_HPP
#define SUIL_APP_HPP

#include <suil/sys.hpp>

namespace suil {

    define_log_tag(APP_TASK);

    struct app_task : LOGGER(dtag(APP_TASK)) {
        app_task(const char *name)
            : name(zcstring(name).dup())
        {}

        virtual ~app_task() {
            if (running)
                stop(EXIT_SUCCESS);
        }

    protected:
        friend struct application;
        virtual int start() = 0;
        virtual void stop(int code = EXIT_SUCCESS) {
        }

        bool running{false};
        int  exitcode{EXIT_SUCCESS};
        zcstring name;
    };

    define_log_tag(APPLICATION);

    struct application : LOGGER(dtag(APPLICATION)) {

        template <typename... __Opts>
        application(const char *name, __Opts... opts)
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

        ~application();

    private:
        void flush();
        bool check(const char *name);
        static coroutine void runtask(application& app, app_task *task);
        static coroutine void wait_notify(application& app);

        uint32_t started{0};
        bool stopped{true};

        uint32_t running_tasks{0};
        int64_t timeout{-1};
        zcstring name;
        zcstr_map_t<app_task*>  tasks;
        async_t<app_task*> stopsync{nullptr};
        async_t<int> startwait{-EINVAL};
    };
}
#endif //SUIL_APP_HPP
