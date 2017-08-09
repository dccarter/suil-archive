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

    void before(http::request&, http::response&, Context&)
    {
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
    auto opts = parse_cmd(argc, argv);

    return suil::launch([&]() {

        http::endpoint<middleware, sql::Postgres> ep(
                opt(nworkers, opts.nworkers),
                opt(port, opts.port));

        ep.middleware<sql::Postgres>().
        setup("dbname=test1 user=postgres password=admin123",
                opt(EXPIRES, 10000),
                opt(ASYNC, true));

        // setup file server
        http::file_server fs(ep,
            opt(root, "/home/dc/app"));

        eproute(ep, "/hello/<string>")
        ("GET"_method)
        ([&](std::string name) {
            return "Hello " + name;
        });

        eproute(ep, "/users")
        ("GET"_method)
        ([&]() {
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

        ep.start();
    });
}