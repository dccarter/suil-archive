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

int main(int argc, const char *argv[])
{
    suil::init();
}
