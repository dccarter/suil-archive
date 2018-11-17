//
// Created by dc on 23/10/18.
//


#include "docker.hpp"

namespace suil::docker {

    json::Object Images::ls(const ImagesFilter &filters, bool all, bool digests) {
        auto resource = utils::catstr(ref.apiBase, "/images/json");
        trace("requesting resource at %s", resource());
        auto resp = http::client::get(ref.httpSession, resource(), [&](http::client::Request &req) {
            // build custom request
            Docker::arg(req, "all", all);
            Docker::arg(req, "digests", digests);
            req.args("filters", json::encode(filters));
            return true;
        });

        trace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return std::move(respObj);
    }

    void Images::build(
            const suil::String archive, const suil::String contentType, const XRegistryConfig &registries,
            const BuildParams &params)
    {
        if (!utils::fs::exists(archive())) {
            // archive does not exits
            throw Exception::create("build archive '", archive, "' does not exist");
        }

        auto resource = utils::catstr(ref.apiBase, "/build");
        trace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request &req) {
            // build custom request
            req.hdr("Content-type", contentType());
            req.hdr("X-Registry-Config", json::encode(registries).c_str());
            // request parameters
            Docker::pack(req, params);
            // request body
            req << File(archive(), O_RDONLY, 0666);
            return true;
        });

        trace("request resource status %d", resp.status());
        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    json::Object Images::buildPrune()
    {
        auto resource = utils::catstr(ref.apiBase, "/build/prune");

        trace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource());
        trace("request resource status %d", resp.status());

        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object  respObj;
        json::decode(resp(), respObj);
        return std::move(respObj);
    }

    void Images::create(ImagesCreateParams &params)
    {
        auto resource = utils::catstr(ref.apiBase, "/images/create");

        trace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request &req) {
            if (!params.fromSrc.empty() && utils::fs::exists(params.fromSrc())) {
                // fromSrc parameter is a local file with the image content
                req << File(params.fromSrc(), O_RDONLY, 0666);
                params.fromSrc = "-";
            }
            Docker::pack(req, params);
            return true;
        });
        trace("request resource status %d", resp.status());

        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    json::Object Images::inspect(const suil::String name)
    {
        auto resource = utils::catstr(ref.apiBase, "/images/", name, "/json");

        trace("requesting resource at %s", resource());
        auto resp = http::client::get(ref.httpSession, resource());
        trace("request resource status %d", resp.status());

        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return std::move(respObj);
    }

    json::Object Images::history(const suil::String name)
    {
        auto resource = utils::catstr(ref.apiBase, "/images/", name, "/history");

        trace("requesting resource at %s", resource());
        auto resp = http::client::get(ref.httpSession, resource());
        trace("request resource status %d", resp.status());

        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return std::move(respObj);
    }

    void Images::push(const suil::String name, const suil::String tag)
    {
        auto resource = utils::catstr(ref.apiBase, "/images/", name, "/push");

        trace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request &req) {
            Docker::arg(req, "tag", tag);
            return true;
        });
        trace("request resource status %d", resp.status());

        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    void Images::tag(const suil::String name, const ImagesTagParams &params)
    {
        auto resource = utils::catstr(ref.apiBase, "/images/", name, "/tag");

        trace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request &req) {
            Docker::pack(req, params);
            return true;
        });
        trace("request resource status %d", resp.status());

        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    json::Object Images::remove(const suil::String name, const ImagesRemoveParams &params)
    {
        auto resource = utils::catstr(ref.apiBase, "/images/", name);

        trace("requesting resource at %s", resource());
        auto resp = http::client::del(ref.httpSession, resource(), [&](http::client::Request &req) {
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
        return std::move(respObj);
    }

    json::Object Images::search(ImagesSearchParams &params)
    {
        auto resource = utils::catstr(ref.apiBase, "/images/search");

        trace("requesting resource at %s", resource());
        auto resp = http::client::get(ref.httpSession, resource(), [&](http::client::Request &req) {
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
        return std::move(respObj);
    }

    json::Object Images::prune(suil::docker::Filters filters)
    {
        auto resource = utils::catstr(ref.apiBase, "/images/prune");

        trace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request &req) {
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
        return std::move(respObj);
    }

    json::Object Images::commit(const ContainerConfig &container, const ImagesCommitParams &params)
    {
        auto resource = utils::catstr(ref.apiBase, "/commit");

        trace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request &req) {
            Docker::pack(req, params);
            req << container;
            return true;
        });
        trace("request resource status %d", resp.status());

        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }

        json::Object respObj;
        json::decode(resp(), respObj);
        return std::move(respObj);
    }

    void Images::get(const suil::String name, const char *output)
    {
        auto resource = utils::catstr(ref.apiBase, "/images/", name, "/get");

        trace("requesting resource at %s", resource());
        if (utils::fs::exists(output)) {
            // cannot override existing file
            throw Exception::create("file '", output, "' already exists");
        }

        http::client::FileOffload offload(output);
        auto resp = http::client::get(offload, ref.httpSession, resource());
        trace("request resource status %d", resp.status());

        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    void Images::gets(const std::vector<suil::String> names, const char *output)
    {
        auto resource = utils::catstr(ref.apiBase, "/images/get");

        trace("requesting resource at %s", resource());
        if (utils::fs::exists(output)) {
            // cannot override existing file
            throw Exception::create("file '", output, "' already exists");
        }

        OBuffer tmp(64);
        for (auto& name: names) {
            tmp << name;
            if (name != names.back())
                tmp << ",";
        }

        String param(tmp);
        http::client::FileOffload offload(output);
        auto resp = http::client::get(offload, ref.httpSession, resource(), [&](http::client::Request &req) {
            Docker::arg(req, "names", param);
            return true;
        });
        trace("request resource status %d", resp.status());

        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }
    }

    void Images::load(const char *input, bool quiet)
    {
        if (!utils::fs::exists(input)) {
            // input file must exist
            throw Exception::create("Images::load input file '", input, "' does not exist");
        }

        auto resource = utils::catstr(ref.apiBase, "/images/load");

        trace("requesting resource at %s", resource());
        auto resp = http::client::post(ref.httpSession, resource(), [&](http::client::Request &req) {
            Docker::arg(req, "quiet", quiet);
            req << File(input, O_RDONLY, 0666);
            return true;
        });
        trace("request resource status %d", resp.status());

        if (resp.status() != http::Status::OK) {
            // request failed
            Docker::reportFailure(resp);
        }
    }
}