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

coroutine void do_start(Application& app) {
    app.start();
}

void memory_test()
{
    auto start = mnow();
    for (int j = 0; j < 20; j++) {
        size_t sz = 1LU << j;
        snotice("\ttesting size %lu", sz);
        auto js = mnow();
        for (int i = 0; i < 1000000; i++) {
            auto ptr = memory::alloc(sz+sizeof(uint32_t));
            memory::free(ptr);
        }
        auto jel = mnow() - js;
        snotice("\telapsed: %lu ms", jel);
    }
    auto elapsed = mnow() - start;
    snotice("elapsed: %lu ms", elapsed);
}

int main(int argc, const char *argv[])
{
    suil::init(opt(use_pool, argc == 3));
    suil::log::setup(opt(verbose, 3));
    Endpoint<> ep("/api", opt(port, 1024));
    // simple route
    ep("/hello")
    ("GET"_method)
    ([]() {
        return "Hello World";
    });

    ep.start();
    //snotice("Starting test %s", (argc >= 3? "Pool": "Non Pool"));
    //memory_test();
}
