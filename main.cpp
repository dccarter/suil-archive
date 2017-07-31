//
// Created by dc on 20/06/17.
//

#include "http/endpoint.hpp"

using  namespace suil;

struct middleware {
    struct Context {
    };

    void before(http::request&, http::response&, Context&)
    {
        snotice("before...");
    }

    void after(http::request&, http::response&, Context&)
    {
        snotice("after...");
    }
};

int main(int argc, char *argv[])
{
    return suil::launch([&]() {

        http::endpoint<middleware> ep;

        eproute(ep, "/hello/<string>")
        ("GET"_method)
        ([&](std::string name){
            return "Hello " + name;
        });

        ep.start();

    }, argc, argv);
}