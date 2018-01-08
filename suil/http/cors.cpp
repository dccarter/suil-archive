//
// Created by dc on 07/01/18.
//
#include "cors.hpp"

namespace suil {
    namespace http {

        void Cors::after(Request &req, http::Response &res, Context &) {
            if (!req.header("Access-Control-Allow-Origin").empty() &&
                !Ego.allow_origin.empty())
            {
                res.header("Access-Control-Allow-Origin", Ego.allow_origin);
            }

            if (!req.header("Access-Control-Allow-Headers").empty() &&
                !Ego.allow_headers.empty())
            {
                res.header("Access-Control-Allow-Headers", Ego.allow_headers);
            }
        }

        void Cors::before(Request &req, Response &res, Context &) {
            if ((req.method == (int)Method::Options)) {
                /* Requesting options */
                auto req_method = req.header("Access-Control-Request-Method");
                if (!req_method.empty()) {
                    auto cors_method = http::method_from_string(req_method.data());
                    if ((req.routeparams().methods & (1 << cors_method))) {
                        // only if route supports requested method
                        zcstring allow{req_method.data(), req_method.size(), false};
                        res.header("Access-Control-Allow-Methods", std::move(allow));
                        res.end();
                    }
                }
            }
        }
    }
}