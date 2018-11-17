//
// Created by dc on 28/09/18.
//

#include <suil/base64.h>
#include "docker.hpp"

namespace suil::docker {

    Docker::Docker(suil::String host, int port)
        : httpSession{},
          host(host.dup()),
          port(port),
          Images(*this),
          Container(*this),
          Networks(*this),
          Volumes(*this),
          Exec(*this)
    {
        idebug("initialize docker session '%s:%d/%s'", host(), port,  Ego.apiBase());
    }

    void Docker::reportFailure(suil::http::client::Response &resp)
    {
        if (resp().empty())
        {
            // no body to decode
            throw Exception::create("docker request failed: ", http::status_text(resp.status()));
        }

        RequestError e;
        try
        {
            iod::json_decode(e, resp());
        }
        catch (...)
        {
            throw Exception::create("decoding docker error message failed: ", Exception::fromCurrent().what());
        }

        throw Exception::create("docker request failed: ", e.message());
    }

    bool Docker::connect(LoginReq &params)
    {
        if (!apiBase.empty()) {
            // client already connected
            ierror("docker client already connected to docker{version=%s}", apiBase());
            return false;
        }

        // load the http session
        Ego.httpSession = http::client::load(Ego.host(), Ego.port);
        // base64 encode json string
        auto paramsStr = json::encode(params);
        httpSession.header("X-Registry-Auth", utils::base64::encode(paramsStr));

        return Ego.connect();
    }

    bool Docker::connect(AuthToken& token)
    {
        if (!apiBase.empty()) {
            // client already connected
            ierror("docker client already connected to docker{version=%s}", apiBase());
            return false;
        }

        // load the http session
        Ego.httpSession = http::client::load(Ego.host(), Ego.port);

        // base64 encode json string
        auto tokenStr = json::encode(token);
        httpSession.header("X-Registry-Auth", utils::base64::encode(tokenStr));

        return Ego.connect(true);
    }

    bool Docker::connect(bool loaded)
    {
        if (!loaded) {
            // load the http session
            Ego.httpSession = http::client::load(Ego.host(), Ego.port);
        }

        try {
            // the get the version
            auto apiVersion = Ego.version();
            iinfo("connected to docker: '%s:%d', version: '%s'",
                  Ego.host(), Ego.port, apiVersion.Version());
            // create the base url
            Ego.apiBase = utils::catstr("/v", apiVersion.ApiVersion);
            return true;
        }
        catch (...) {
            // failed to load API version
            ierror("failed to load API version: %s", Exception::fromCurrent().what());
            return false;
        }
    }

    VersionResp Docker::version()
    {
        String resource{"/version"};
        trace("requesting resource at %s", resource());
        auto resp = http::client::get(httpSession, resource());
        trace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            reportFailure(resp);
        }

        trace("get resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            reportFailure(resp);
        }

        VersionResp version;
        json::decode(resp(), version);
        return std::move(version);
    }
}
