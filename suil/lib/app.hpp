//
// Created by dc on 8/30/17.
//

#ifndef SUIL_APP_HPP
#define SUIL_APP_HPP

#include "sys.hpp"
#include "log.hpp"

namespace suil {

    define_log_tag(APPLICATION);
    struct appmanager;

    struct apptask {
        apptask(const char *name)
            : name(zcstring(name).dup())
        {}

        virtual int start() = 0;
        virtual void stop() {}

    protected:
        friend struct application;
        bool     running;
        int      tid{0};
        zcstring name;
    };

    struct application : LOGGER(dtag(APPLICATION)) {
        template <typename __Task, typename... __O>
        __Task& reg_task(const char *name, __O... opts) {
            /* register a new task */
            __Task *task(new __Task(name, opts...));
            if (!task) {
                /* could not register task */
                throw suil_error::create("registering task ", name, "failed.");
            }
            task->tid = GEN_TID++;
            auto it = tasks.insert(tasks.end(),
                                   std::make_pair(task->name.dup(), task));
            /* return the inserted task */
            return *task;
        }

        ~application();

        int verbosity() const;
        const std::string& wdir() const;
        const std::string& binary() const;

    private:
        friend struct appmanager;
        application(appmanager& mgr, const char *name)
            : mgr(mgr),
              name(zcstring(name).dup())
        {}

        int start();
        void stop(int);
        static coroutine void async_task(application& app, apptask* task);
        static coroutine void wait_notify(application& app);

        appmanager& mgr;
        int64_t   stop_timeout{2000};
        int       aid{0};
        int       GEN_TID{0};
        zcstring  name;
        zcstr_map_t<apptask*> tasks;
        int       running_tasks{0};
        bool                  stopping{false};
        async_t<apptask*>     waiter{nullptr};
        async_t<int>          exitchan{-1};
    };
    using apphandle = application*;
}

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*onload_t)  (suil::apphandle, int, const char**);
typedef void(*onunload_t)(suil::apphandle);

#ifdef __cplusplus
};
#endif

#define APP(h) *(h)

#endif //SUIL_APP_HPP
