//
// Created by dc on 12/12/18.
//

#include "jsonrpc.h"

namespace suil::rpc {

    JsonRpcServerConnection::JsonRpcServerConnection()
        : RpcTxRx()
    {
        extensionMethods.emplace(String{"rpc_Version"},
        [&](const json::Object& _) -> ReturnType {
            /* just return library version*/
            return std::make_pair(0, json::Object(SUIL_VERSION_STRING));
        });
    }

    void JsonRpcServerConnection::operator()(suil::SocketAdaptor &sock, JsonRpcHandler *h)
    {
        Ego.handler = h;
        try {
            OBuffer ob{1024};
            do {
                ob.reset(1024, true);
                if (!Ego.receiveRaw(sock, ob))
                    break;

                auto resp = handleRequest(ob);
                if (!Ego.sendRaw(sock, resp))
                    break;

            } while (sock.isopen());
        }
        catch (...) {
            /* Unhandled JSONRPC error */
            ierror("un handled JSON RPC processing error: %s", Exception::fromCurrent().what());
            sock.close();
        }
    }

    String JsonRpcServerConnection::parse_Request(std::vector<JrpcRequest> &req, const suil::OBuffer &ob)
    {
        try {
            json::decode(ob, req);
            return nullptr;
        }
        catch (...) {
            /* Parsing JSON request failed */
            return String{Exception::fromCurrent().what()}.dup();
        }
    }

    std::string JsonRpcServerConnection::handleRequest(const OBuffer &buf)
    {
        std::vector<JrpcResponse> resps{};
        std::vector<JrpcRequest>  reqs;
        auto parseStatus = Ego.parse_Request(reqs, buf);
        if (parseStatus) {
            /* error parsing request */
            ierror("parsing request failed: %s", parseStatus());
            RpcError tmp{JRPC_PARSE_ERROR, "ParseError", std::move(parseStatus)};
            JrpcResponse resp;
            iod::zero(resp);
            resp.jsonrpc = String{JSON_RPC_VERSION};
            resp.error = std::move(tmp);
            resps.push_back(std::move(resp));
        }
        else {
            for (auto &req: reqs) {
                /* handle all requests in */
                if (req.jsonrpc != JSON_RPC_VERSION) {
                    /* Only version 2.0 is supported */
                    RpcError tmp{JRPC_INVALID_REQUEST, "InvalidRequest",
                                  utils::catstr("Unsupported JSON RPC version '", req.jsonrpc, "'")};
                    JrpcResponse resp;
                    resp.jsonrpc = String{JSON_RPC_VERSION};
                    resp.error = std::move(tmp);
                    resps.push_back(std::move(resp));
                }

                static json::Object __{nullptr};
                json::Object &obj = (*req.params).empty() ? __ : *req.params;
                if (req.method.substr(0, 4) == "rpc_") {
                    /* system extension method */
                    resps.push_back(handle_Extension(req.method, obj, req.id));
                } else {
                    /* parse to service handler */
                    resps.push_back(handle_WithHandler(*handler, req.method, obj, req.id));
                }
            }
        }

        return json::encode(resps);
    }

    JrpcResponse JsonRpcServerConnection::handle_WithHandler(
            suil::rpc::JsonRpcHandler &h, const suil::String &method, const suil::json::Object &params, int id)
    {
        JrpcResponse  resp;
        iod::zero(resp);
        resp.jsonrpc = JSON_RPC_VERSION;
        resp.id      = std::move(id);

        try {
            auto[code, ret] = h(method, params, id);
            if (code) {
                /* Service handler returned error code */
                RpcError err{JRPC_API_ERROR, "ServiceReturnedError", (String) ret};
                resp.error = std::move(err);
            }
            else {
                /* Result returned */
                resp.result = std::move(ret);
            }
        }
        catch (...) {
            /* unhandled error in service handler*/
            RpcError tmp{JRPC_API_ERROR, "UnhandledError", String{Exception::fromCurrent().what()}.dup()};
            resp.error = std::move(tmp);
        }

        return std::move(resp);
    }

    JrpcResponse JsonRpcServerConnection::handle_Extension(
            const suil::String &method, const suil::json::Object &params, int id)
    {
        JrpcResponse  resp;
        iod::zero(resp);
        resp.jsonrpc = JSON_RPC_VERSION;
        resp.id      = std::move(id);

        /* handle extension methods */
        auto extMethod = extensionMethods.find(method);
        if (extMethod != extensionMethods.end()) {
            /* invoke extension method if it exists*/
            try {
                auto [code, ret] = extMethod->second(params);
                if (code) {
                    /* Service handler returned error code */
                    RpcError err{JRPC_INTERNAL_ERROR, "InternalError", (String) ret};
                    resp.error = std::move(err);
                }
                else {
                    /* Result returned */
                    resp.result = std::move(ret);
                }
            }
            catch (...) {
                /* unhandled error in service handler*/
                RpcError tmp{JRPC_INTERNAL_ERROR, "InternalError", String{Exception::fromCurrent().what()}.dup()};
                resp.error = std::move(tmp);
            }
        }
        else {
            /* method not found */
            RpcError err{JRPC_METHOD_NOT_FOUND, "MethodNotFound",
                          utils::catstr("method '", method, "' is not an extension method")};
            resp.error = std::move(err);
        }

        return std::move(resp);
    }

    bool __JsonRpcClient::connect(String&& host, int port)
    {
        idebug("JSON RPC client connecting to server %s:%d", host(), port);
        ipaddr addr = iplocal(host(), port, 0);
        if (!sock.connect(addr, 1500)) {
            /* failed to connect to given host */
            ierror("JSON RPC connection to %s:%d failed: %s", host(), port, errno_s);
            return false;
        }

        auto verString = Ego.rpc_Version();
        if (!verString) {
            /* communication with service failed */
            ierror("JSON RPC client failed to communicate with service");
            sock.close();
            return false;
        }

        idebug("Successfully connected to service, running suil version %s", verString());
        return true;
    }

    String __JsonRpcClient::rpc_Version()
    {
        auto&& [ok, ver] = Ego.call("rpc_Version");
        if (ok) {
            /* an error was returned */
            ierror("service returned error %s", (const char*) ver);
            return nullptr;
        }
        else {
            /* version found */
            return ((String) ver).dup();
        }
    }

    ReturnType __JsonRpcClient::call(suil::String &&method, suil::json::Object &&params)
    {
        std::vector<ReturnType> resps = Ego.batch(std::move(method), std::move(params));
        auto resp = std::move(resps.back());
        resps.pop_back();
        return std::move(resp);
    }

    std::vector<ReturnType> __JsonRpcClient::call(std::vector<JrpcRequest> & package)
    {
        std::vector<JrpcResponse> resps;
        /* encode request and send */
        auto raw = json::encode(package);
        if (!Ego.sendRaw(sock, raw)) {
            /* sending failed */
            throw Exception::create(JRPC_INTERNAL_ERROR, "Sending requests JSON RPC server failed - ", errno_s);
        }

        OBuffer rxb{0};
        if (!Ego.receiveRaw(sock, rxb)) {
            /* receiving response failed */
            throw Exception::create(JRPC_INTERNAL_ERROR, "Failed to receive response from JSON RPC server - ", errno_s);
        }

        try {
            /* decode received responses */
            json::decode(rxb, resps);
        }
        catch (...) {
            /* server returned junk */
            throw Exception::create(JRPC_INTERNAL_ERROR, "Failed to decode received JSON RPC response - ",
                    Exception::fromCurrent().what());
        }

        /* transform responses into return types */
        return Ego.transformResponses(std::move(resps));
    }

    std::vector<ReturnType> __JsonRpcClient::transformResponses(std::vector<JrpcResponse>&& resps)
    {
        std::vector<ReturnType> res;
        while (!resps.empty())
        {
            JrpcResponse resp = std::move(resps.back());
            resps.pop_back();
            if (resp.error && resp.result) {
                /* a response can either be an error or a result */
                throw Exception::create(JRPC_INTERNAL_ERROR,
                        "Response {id=", resp.id, "} is invalid because its payload has a result and an error");
            }

            if (resp.error) {
                /* transform error */
                auto& err = *resp.error;
                if (err.code >= -32099 && err.code <= JRPC_API_ERROR) {
                    /* Considered API error */
                    json::Object obj(utils::catstr("API_ERROR-", err.code, " ", err.data));
                    res.emplace_back(err.code, std::move(obj));
                }
                else {
                    /* system error */
                    throw Exception::create(err.code,
                          utils::catstr("internal server error {", err.message, ", ", err.data, "}"));
                }
            }
            else {
                /* we have a result */
                res.emplace_back(0, std::move(*resp.result));
            }
        }

        return std::move(res);
    }

}