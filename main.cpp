//
// Created by dc on 20/06/17.
//
#include "http/endpoint.hpp"
#include "http/fserver.hpp"
#include "sql/sqlite.hpp"
#include "sql/pgsql.hpp"

using  namespace suil;

struct middleware {
    struct Context {
    };

    void before(http::request& req, http::response&, Context&) {
        if (req.route().STATIC) {
            sinfo("handling static route: %s", req.url);
        }
    }

    void after(http::request&, http::response&, Context&)
    {
    }
};

typedef decltype(iod::D(
    s::_id(s::_AUTO_INCREMENT, s::_PRIMARY_KEY) = int(),
    prop(username,   std::string),
    prop(email,      std::string),
    prop(age,        int)
)) user_t;
typedef sql::orm<typename sql::pgsql_db::Connection, user_t> user_orm_t;

int main(int argc, const char *argv[])
{
    memory::init();

    auto opts = parse_cmd(argc, argv);
    /* set logging verbosity */
    log::setup(opt(verbose, 6));

    http::endpoint<http::system_attrs, middleware, sql::Postgres> ep("/api/",
            opt(port, opts.port));

    ep.middleware<sql::Postgres>().
    setup("dbname=test1 user=postgres password=admin123",
            opt(EXPIRES, 10000),
            opt(ASYNC, true));

    // setup file server
    http::file_server fs(ep,
        opt(root, "/home/dc/app/"));

    eproute(ep, "/hello/<string>")
    ("GET"_method)
    .attrs(opt(AUTHORIZE, false))
    ([&](std::string name) {
        return "Hello " + name;
    });

    eproute(ep, "/users")
    ("GET"_method)
    ([&](const http::request& req) {

        auto &conn = ep.middleware<sql::Postgres>().conn();
        user_orm_t user_orm("users", conn);
        std::vector<user_t> users;
        user_orm.forall([&](user_t& u){
            users.push_back(u);
        });

        // close the connection
        conn.close();

        return iod::json_encode(users);
    });

    eproute(ep, "/form")
    ("POST"_method)
    .attrs(opt(PARSE_FORM, true))
    ([&](const http::request& req) {
        http::request_form_t form(req);
        form |
        [&](const zcstring& name, const zcstring& val) {
            sinfo("argument: %s: %s", name.cstr, val.str);
            return false;
        };

        form |
        [&](const http::upfile_t& f) {
            sinfo("file: name=%s, size %ld", f.name().cstr, f.size());
            f.save("/home/dc/app");
            return false;
        };
        return http::status_t::OK;
    });

    ep.start();
}