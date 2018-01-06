//
// Created by dc on 20/06/17.
//
#include <suil/http/endpoint.hpp>
#include <suil/http/fserver.hpp>
#include <suil/sql/sqlite.hpp>
#include <suil/sql/pgsql.hpp>
#include <suil/app.hpp>
#include <suil/http/clientapi.hpp>
#include <suil/email.hpp>

using  namespace suil;
using  namespace suil::http;

struct Middleware {
    struct Context {
    };

    void before(http::Request& req, http::Response&, Context&) {
        if (req.route().STATIC) {
            sinfo("handling static route: %s", req.url);
        }
    }

    void after(http::Request&, http::Response&, Context&)
    {}
};

typedef decltype(iod::D(
    s::_id(s::_AUTO_INCREMENT, s::_PRIMARY_KEY) = int(),
    prop(username,   std::string),
    prop(email,      std::string),
    prop(age,        int)
)) User;
typedef sql::Orm<typename sql::PgSqlDb::Connection, User> user_orm_t;


struct http_task : public AppTask {
    template <typename... __Args>
    http_task(const char *name, __Args... args)
        : AppTask(name),
          ep("/api/v1", args...)
    {
//        ep.Middleware<sql::Postgres>().
//                setup("dbname=test1 user=Postgres password=*******",
//                      opt(EXPIRES, 10000),
//                      opt(ASYNC, true));

        // setup file server
        http::FileServer fs(ep,
                             opt(root, "/home/dc/app/"));

        eproute(ep, "/hello/<string>")
        ("GET"_method)
        .attrs(opt(AUTHORIZE, Auth{false}))
        ([&](std::string name) {
            return "Hello " + name;
        });

//        eproute(ep, "/users")
//        ("GET"_method)
//        ([&](const http::Request& req) {
//
//            auto &conn = ep.Middleware<sql::Postgres>().conn();
//            user_orm_t user_orm("users", conn);
//            std::vector<User> users;
//            user_orm.forall([&](User& u){
//                users.push_back(u);
//            });
//
//            // close the Connection
//            conn.close();
//
//            return iod::json_encode(users);
//        });

        eproute(ep, "/form")
        ("POST"_method)
        .attrs(opt(PARSE_FORM, true))
        ([&](const http::Request& req) {
            http::request_form_t form(req);
            form |
            [&](const zcstring& name, const zcstring& val) {
                sinfo("argument: %s: %s", name(), val());
                return false;
            };

            form |
            [&](const http::UploadedFile& f) {
                sinfo("file: name=%s, size %ld", f.name()(), f.size());
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
    http::TcpEndpoint<http::SystemAttrs, Middleware/*, sql::Postgres*/> ep;
};

coroutine void do_start(Application& app) {
    app.start();
}

int main(int argc, const char *argv[])
{
    auto opts = parse_cmd(argc, argv);
    suil::init();

//    typedef decltype(iod::D(
//            prop(name, json::Object),
//            prop(age,  int)
//    )) Dynamic;
//
//    std::string jstr = "{\"name\":{\"num\":1, \"boolean\":true}, \"age\":28}";
//    Dynamic  d{};
//    try {
//        iod::json_decode(d, jstr);
//    }
//    catch(...) {
//        sinfo("error: %s", suil_error::getmsg(std::current_exception()));
//    }
//    json::Object& obj = d.name;
//    double num = obj["num"].number_value();
//    double b   = obj["boolean"].bool_value();
//
//    auto out = iod::json_encode(d);
//
//    Application app("demo");
//    /* setup logging options */
//    log::setup(opt(name, "demo"));


    /*auto browser = client::load("http://browser.dc1.suilteam.com");
    {
        client::Response resp = client::get(browser, "/api/v1/gsearch/basic",
        [&](client::Request& req) {
            req.args("q", "Barack Obama");
            return true;
        });
        strace("Request honored");
    }*/
    app.regtask<http_task>("http", opt(port, opts.port));
    //go(do_start(app));
    //msleep(utils::after(10000));
    app.start();
}
