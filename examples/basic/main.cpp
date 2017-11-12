//
// Created by dc on 11/10/17.
//
#include <suil/http/endpoint.hpp>

#ifndef IOD_SYMBOL_A
    #define IOD_SYMBOL_A
    iod_define_symbol(a)
#endif

using namespace suil;
int main(int argc, const char *argv[])
{
    suil::init();
    http::endpoint<http::system_attrs> ep("/api/v1");

    eproute(ep, "/hello")
    ("GET"_method)
    ([&]() {
        return "Hello World!!!";
    });

    eproute(ep, "/hello/<string>")
    ("GET"_method)
    ([&](const http::request&, std::string name) {
        std::string resp = "Hello " + name + "!!!";
        return resp;
    });

    eproute(ep, "/add/<int>/<float>")
    ("GET"_method)
    ([&](const http::request&, int a, float b) {
        return a+b;
    });

    eproute(ep, "/divide")
    ("GET"_method)
    ([&](const http::request& req) {
        auto a = req.query<int>("a");
        auto b = req.query<float>("b");
        return a / b;
    });

    eproute(ep, "/multiply")
    ("POST"_method)
    ([&](const http::request& req) {
        typedef decltype(iod::D(prop(a, int), prop(b, float))) js_t;
        js_t js;
        iod::json_decode(js, req.body);
        return js.a * js.b;
    });

    // Posting a form
    eproute(ep, "/form")
    ("POST"_method)
    .attrs(opt(PARSE_FORM, true)) /* Only works when the sys_attrs middleware is enabled, allows form to be parsed */
    ([&](const http::request& req, http::response& resp) {
        // Create a form iterator
        http::request_form_t form(req);

        resp << "****Form Fields\n";
        // Iterate form fields
        form
        | [&resp](const zcstring& field, const zcstring& value) {
            resp << "\t" << field << ": " << value << "\n";
            return false;
        };

        // Iterate form files
        resp << "****Form Files\n";
        form
        |[&resp](const http::upfile_t& up) {
            resp << "\t" << up.name() << "\n";
            return false;
        };
    });

    return ep.start();
}