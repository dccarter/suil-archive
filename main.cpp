//
// Created by dc on 20/06/17.
//

#include "http/endpoint.hpp"

int main(int argc, char *argv[])
{
    return suil::launch([&]() {
        using  namespace suil;

        http::endpoint<> ep;
        eproute(ep, "/hello/<string>")
        ("GET"_method)
        ([&](std::string name){
            return "Hello " + name;
        });

        ep.start();

    }, argc, argv);
}