//
// Created by dc on 30/10/18.
//

#include "docker.hpp"

namespace suil::docker {

    void Exec::start(const suil::zcstring id, const ExecStartReq &request)
    {
        auto resource = utils::catstr(ref.apiBase, "/exec/", id, "/start");
        trace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            req << request;
            return true;
        });

        trace("request resource status %d", resp.status());
        if (resp.status() != http::Status::CREATED) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    void Exec::resize(const suil::zcstring id, uint32_t h, uint32_t w)
    {
        auto resource = utils::catstr(ref.apiBase, "/exec/", id, "/resize");
        trace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            Docker::arg(req, "h", h);
            Docker::arg(req, "w", w);
            return true;
        });

        trace("request resource status %d", resp.status());
        if (resp.status() != http::Status::CREATED) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    json::Object Exec::inspect(const suil::zcstring id)
    {
        auto resource = utils::catstr(ref.apiBase, "/exec/", id, "/json");
        trace("requesting resource at %s", resource());
        auto resp = http::client::get(ref.httpSession, resource());

        trace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return respObj;
    }
}