#include <suil/sys.hpp>
#include <suil/cmdl.hpp>
#include <suil/http/endpoint.hpp>

using namespace suil;

int main(int argc, char *argv[])
{
    suil::init(opt(printinfo, false));
    log::setup(opt(verbose, 0));
    // create a new HTTP endpoint listening on port 80
    http::TcpEndpoint<http::SystemAttrs> ep("/api",
            opt(name, "0.0.0.0"),
            opt(port, 1080));

    // accessed via http://0.0.0.0:1080/api/hello
    ep("/hello")
    ("GET"_method)
    ([]() {
        // return just a string saying hello world
        return "Hello World";
    });

    // accessed via GET http://0.0.0.0:1080/api/hello
    ep("/about")
    ("GET"_method)
    ([]() {
        // Simple About sting
        return APP_NAME " " APP_VERSION " Copyright (c) 2018";
    });

    // accessed via GET http://0.0.0.0:1080/api/json
    ep("/json")
    ("GET"_method)
    ([]() {
        // returning a json object will add the Content-Type: application/json header
        typedef decltype(iod::D(
                prop(name, zcstring),
                prop(version, zcstring)
        )) JsonType;

        return JsonType{zcstring{APP_NAME}, zcstring{APP_VERSION}};
    });

    // <int> parameter via GET http://0.0.0.0:1080/api/check/integer
    ep("/check/<int>")
    ("GET"_method)
    ([](int num) {
        // demonstrates use of parameters and returning status codes
        sdebug("checking number %d", num);
        if (num > 100) {
            // number out of range
            return http::Status::BAD_REQUEST;
        }

        return http::Status::OK;
    });

    // routes accept url parameters, e.g http://0.0.0.0:1080/api/add/3/1
    ep("/add/<int>/<int>")
    ("GET"_method)
    ([](const http::Request& req, http::Response& resp, int a, int b) {
        // demonstrates use of parameters and returning status codes
        sdebug("adding numbers a=%d, b=%d", a, b);
        resp.appendf("a + b = %d\n", a+b);
        resp.end(http::Status::OK);
    });

    ep("/post_json")
    ("POST"_method)
    ([](const http::Request& req, http::Response& resp) {
        // demonstrate parsing request body into a json object
        typedef decltype(iod::D(
            prop(name, zcstring),
            prop(email, zcstring)
        )) JsonType;

        try {
            auto data = req.toJson<JsonType>();
            resp << "Hello " << data.name << ", we have registered your email '"
                 << data.email << "' :-)";
            resp.end(http::Status::OK);
        }
        catch(...) {
            // invalid json data
            resp << "Invalid JSON data: " << exmsg();
            resp.end(http::Status::BAD_REQUEST);
        }
    });

    ep("/post_form")
    ("POST"_method)
    .attrs(opt(PARSE_FORM, true)) // required for http to parse request as a form
    ([](const http::Request& req, http::Response& resp) {
        // demonstrate parsing request body into a json object
        typedef decltype(iod::D(
                prop(name, zcstring),
                prop(email, zcstring)
        )) FormType;

        try {
            // extract parse form
            FormType data;
            http::RequestForm form(req);
            form >> data;
            resp << "Hello " << data.name << ", we have registered your email '"
                 << data.email << "' :-)";
            resp.end(http::Status::OK);
        }
        catch(...) {
            // invalid json data
            resp << "Invalid form data: " << exmsg();
            resp.end(http::Status::BAD_REQUEST);
        }
    });

    // start the server
    return ep.start();
}

