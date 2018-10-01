//
// Created by dc on 28/09/18.
//

#include "docker.hpp"

namespace suil::docker {

    Client::Client(suil::zcstring host, int port, suil::zcstring version)
        : httpSession{}
    {
        Ego.apiBase = utils::catstr('/', version);
        idebug("initialize docker session '%s:%d/%s'", host(), port,  Ego.apiBase());
    }

    void Client::reportFailure(suil::http::client::Response &resp)
    {
        if (resp().empty())
        {
            // no body to decode
            throw SuilError::create("docker request failed: %s", http::status_text(resp.status()));
        }
        try
        {
            RequestError e;
            iod::json_decode(e, resp());
            throw SuilError::create("docker request failed: %s", e.message());
        }
        catch (...)
        {
            throw SuilError::create("decoding docker error message failed: %", exmsg());
        }
    }

    ListResp Client::ps(bool all, int limit, bool size, bool useFilter, ListFilter &filter)
    {
        auto resource = utils::catstr(apiBase, "/list");
        trace("requesting resource at %s", resource());
        auto resp = http::client::get(httpSession, resource(), [&](http::client::Request& req) {
            // build custom request
            if (all)       req.args("all", "true");
            if (limit)     req.args("limit", limit);
            if (size)      req.args("size", "true");
            if (useFilter) req.args("filters", json::encode(filter));

            return true;
        });

        trace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            reportFailure(resp);
        }

        ListResp respObj;
        iod::json_decode(respObj, resp());
        return respObj;
    }
}
