//
// Created by dc on 07/01/18.
//

#ifndef SUIL_CORS_HPP
#define SUIL_CORS_HPP

#include <suil/http/routing.h>

namespace suil {
    namespace http {

        struct Cors {
            struct Context{
            };

            void before(Request& req, Response&, Context&);

            void after(Request&, http::Response&, Context&);

            template<typename __T>
            void configure(__T& opts) {
                if (opts.has(sym(allow_origin))) {
                    Ego.allow_origin = opts.has(sym(allow_origin), "");
                }

                if (opts.has(sym(allow_headers))) {
                    Ego.allow_headers = opts.has(sym(allow_headers), "");
                }
            }

            template <typename E, typename...__Opts>
            void setup(E& ep, __Opts... args) {
                auto opts = iod::D(args...);
                configure(opts);
            }

        private:
            std::string    allow_origin{"*"};
            std::string    allow_headers{"Origin, X-Requested-With, Content-Type, Accept"};
        };
    }
}
#endif //SUIL_CORS_HPP
