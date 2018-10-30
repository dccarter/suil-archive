//
// Created by dc on 28/10/18.
//

#include "docker.hpp"

namespace suil::docker {

    json::Object Volumes::ls(const suil::docker::Filters &filters)
    {
        auto resource = utils::catstr(ref.apiBase, "/volumes");
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

    json::Object Volumes::create(const VolumesCreateReq &request)
    {
        auto resource = utils::catstr(ref.apiBase, "/volumes/create");
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

    json::Object Volumes::inspect(const suil::zcstring id)
    {
        auto resource = utils::catstr(ref.apiBase, "/volumes/", id);
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

    void Volumes::remove(const suil::zcstring id, bool force)
    {
        auto resource = utils::catstr(ref.apiBase, "/volumes/", id);
        trace("requesting resource at %s", resource());
        auto resp = http::client::del(ref.httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            Docker::arg(req, "force", force);
            return true;
        });

        trace("request resource status %d", resp.status());
        if (resp.status() != http::Status::NO_CONTENT) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    json::Object Volumes::prune(suil::docker::Filters filters)
    {
        auto resource = utils::catstr(ref.apiBase, "/volumes/prune");
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