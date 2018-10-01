//
// Created by dc on 28/09/18.
//

#ifndef SUIL_DOCKER_HPP
#define SUIL_DOCKER_HPP

#include <suil/http/clientapi.hpp>
#include <suil/docker/types.hpp>

namespace suil::docker {

    define_log_tag(DOCKER);

    struct Client : LOGGER(dtag(DOCKER)) {
        sptr(Client)

        Client(zcstring host, int port, zcstring apiVersion);

        template <typename... Opts>
        ListResp ps(Opts... options) {
            ListFilter  filter;
            bool useFilter{false};
            auto opts = iod::D(options...);

            if (opts.has(var(filters))) {
                // use filters only when they are available
                useFilter = true;
                filter = opts.get(var(filters), filter);
            }
            bool all   = opts.get(var(all), false);
            int  limit = opts.get(var(limit), 0);
            bool size  = opts.get(var(size), false);

            return Ego.ps(all, limit, size, useFilter, filter);
        }

        template <typename... Args>
        bool connect(Args... args) {
            auto opts = iod::D(options...);
            if (opts.has(var(loginAuth))) {
                // login using login credentials
                LoginReq params;
                params = opts.get(var(loginAuth), params);
                return connect(params);
            }
            else if (opts.has(var(tokenAuth))) {
                // login using token authentication
                AuthToken token;
                params = opts.get(var(loginAuth), token);
                return connect(token);
            }
            else {
                // connect without authentication
                return connect();
            }
        }

    private:
        ListResp ps(bool all, int limit, bool size, bool useFilter, ListFilter& filter);
        bool connect(LoginReq& params);
        bool connect(AuthToken& token);
        bool connect();
        void reportFailure(http::client::Response& resp);
        zcstring              apiBase;
        http::client::Session httpSession;
    };
}

#endif //SUIL_DOCKER_HPP
