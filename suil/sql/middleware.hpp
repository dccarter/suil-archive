//
// Created by dc on 8/2/17.
//

#ifndef SUIL_MIDDLEWARE_HPP
#define SUIL_MIDDLEWARE_HPP

#include "sys.hpp"
#include "orm.hpp"
#include "http/response.hpp"
#include "http/request.hpp"

namespace suil {
    namespace sql {

        template <typename __Db>
        struct middleware {
            template <typename __O>
            using orm = sql::orm<typename __Db::Connection, __O>;

            struct Context{
            };

            template <typename... __Opts>
            void setup(const char *conn, __Opts... opts) {
                db.init(conn, opts...);
                expires = iod::D(opts...).get(sym(expires), 5000);
            }

            typename __Db::Connection& conn() {
                return db.connection();
            }

            void before(http::request&, http::response&, Context&) {
            }

            void after(http::request&, http::response&, Context&) {
            }

        private:
            __Db    db;
            int     expires{-1};
        };
    }
}
#endif //SUIL_MIDDLEWARE_HPP
