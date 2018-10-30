//
// Created by dc on 28/10/18.
//

#include "docker.hpp"

namespace suil::docker {

    json::Object Networks::ls(const suil::docker::Filters &filters)
    {
        auto resource = utils::catstr(ref.apiBase, "/networks");
        trace("requesting resource at %s", resource());
        auto resp = http::client::get(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            Docker::arg(req, "filters", filters);
            return true;
        });

        trace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return respObj;
    }

    json::Object Networks::inspect(const suil::zcstring id, const NetworksInspectParams &params)
    {
        auto resource = utils::catstr(ref.apiBase, "/networks/", id);
        trace("requesting resource at %s", resource());
        auto resp = http::client::get(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            Docker::pack(req, params);
            return true;
        });

        trace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return respObj;
    }

    void Networks::remove(const suil::zcstring id)
    {
        auto resource = utils::catstr(ref.apiBase, "/networks/", id);
        trace("requesting resource at %s", resource());
        auto resp = http::client::del(ref.httpSession, resource());

        trace("request resource status %d", resp.status());
        if (resp.status() != http::Status::NO_CONTENT) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    json::Object Networks::create(const NetworksCreateReq &request)
    {
        auto resource = utils::catstr(ref.apiBase, "/networks/create");
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

        json::Object respObj;
        json::decode(resp(), respObj);
        return respObj;
    }

    void Networks::connect(const suil::zcstring id, const NetworksConnectReq &request)
    {
        auto resource = utils::catstr(ref.apiBase, "/networks/", id, "/connect");
        trace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            req << request;
            return true;
        });

        trace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    void Networks::disconnect(const suil::zcstring id, const NetworksDisconnectReq &request)
    {
        auto resource = utils::catstr(ref.apiBase, "/networks/", id, "/disconnect");
        trace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            req << request;
            return true;
        });

        trace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    json::Object Networks::prune(suil::docker::Filters filters)
    {
        auto resource = utils::catstr(ref.apiBase, "/networks/prune");
        trace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            Docker::arg(req, "filters", json::encode(filters));
            return true;
        });

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