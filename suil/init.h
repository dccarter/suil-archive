//
// Created by dc on 12/11/18.
//

#ifndef SUIL_INIT_H
#define SUIL_INIT_H

#include <suil/utils.h>
#include <suil/logging.h>

namespace suil {

    using SignalHandler = std::function<void(int sig, siginfo_t *info, void *ctx)>;

    bool load(bool si = true);

    namespace __internal {
        void chwdir(const std::string& to);
        void daemonize(const std::string& wdir);
    }

    template <typename... __A>
    void init(__A... args) {
        static bool initialized{false};
        auto opts = iod::D(args...);
        bool showinfo = opts.get(var(printinfo), true);
        suil::load(showinfo);
        // setup logging
        log::setup(args...);

        if (!initialized) {

            std::string wdir = opts.get(var(wdir), "");

            if (opts.get(var(background), false)) {
                // put the process to background
                __internal::daemonize(wdir);
            } else if (!wdir.empty()) {
                // change directory in current process
                __internal::chwdir(wdir);
            }
        }
        initialized = true;
    }
}

#endif //SUIL_INIT_H
