//
// Created by dc on 20/06/17.
//
#include <suil/http/endpoint.hpp>
#include <suil/http/fserver.hpp>
#include <suil/sql/sqlite.hpp>
#include <suil/sql/pgsql.hpp>
#include <suil/app.hpp>
#include <suil/http/client.hpp>

using  namespace suil;
using  namespace suil::http;

struct middleware {
    struct Context {
    };

    void before(http::request& req, http::response&, Context&) {
        if (req.route().STATIC) {
            sinfo("handling static route: %s", req.url);
        }
    }

    void after(http::request&, http::response&, Context&)
    {}
};

typedef decltype(iod::D(
    s::_id(s::_AUTO_INCREMENT, s::_PRIMARY_KEY) = int(),
    prop(username,   std::string),
    prop(email,      std::string),
    prop(age,        int)
)) user_t;
typedef sql::orm<typename sql::pgsql_db::Connection, user_t> user_orm_t;


struct http_task : public app_task {
    template <typename... __Args>
    http_task(const char *name, __Args... args)
        : app_task(name),
          ep("/api/v1", args...)
    {
//        ep.middleware<sql::Postgres>().
//                setup("dbname=test1 user=postgres password=*******",
//                      opt(EXPIRES, 10000),
//                      opt(ASYNC, true));

        // setup file server
        http::file_server fs(ep,
                             opt(root, "/home/dc/app/"));

        eproute(ep, "/hello/<string>")
        ("GET"_method)
        .attrs(opt(AUTHORIZE, Auth{false}))
        ([&](std::string name) {
            return "Hello " + name;
        });

//        eproute(ep, "/users")
//        ("GET"_method)
//        ([&](const http::request& req) {
//
//            auto &conn = ep.middleware<sql::Postgres>().conn();
//            user_orm_t user_orm("users", conn);
//            std::vector<user_t> users;
//            user_orm.forall([&](user_t& u){
//                users.push_back(u);
//            });
//
//            // close the connection
//            conn.close();
//
//            return iod::json_encode(users);
//        });

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
            return http::Status::OK;
        });
    }

protected:
    virtual int start()  {
        ep.start();
    }

    virtual void stop(int code = EXIT_SUCCESS) {
        ep.stop();
    }

private:
    http::endpoint<http::system_attrs, middleware/*, sql::Postgres*/> ep;
};

coroutine void do_start(application& app) {
    app.start();
}

int main(int argc, const char *argv[])
{
    auto opts = parse_cmd(argc, argv);
    suil::init();

    application app("demo");
    /* setup logging options */
    log::setup(opt(verbose, opts.verbose),
               opt(name, "demo"));

    /*auto browser = client::load("http://browser.dc1.suilteam.com");
    {
        client::response resp = client::get(browser, "/api/v1/gsearch/basic",
        [&](client::request& req) {
            req.args("q", "Barack Obama");
            return true;
        });
        strace("request honored");
    }*/
    app.regtask<http_task>("http", opt(port, opts.port));
    //go(do_start(app));
    //msleep(utils::after(10000));
    app.start();
}