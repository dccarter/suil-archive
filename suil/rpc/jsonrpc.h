//
// Created by dc on 12/12/18.
//

#ifndef SUIL_JSONRPC_H
#define SUIL_JSONRPC_H

#include <suil/json.h>
#include <suil/rpc/common.h>

#ifndef JSON_RPC_VERSION
#define JSON_RPC_VERSION "2.0"
#endif

namespace suil::rpc {

    define_log_tag(JSON_RPC);

    enum : int {
        JRPC_API_ERROR         = -32000,
        JRPC_PARSE_ERROR       = -32700,
        JRPC_INVALID_REQUEST   = -32600,
        JRPC_METHOD_NOT_FOUND  = -32601,
        JRPC_INVALID_PARAMS    = -32002,
        JRPC_INTERNAL_ERROR    = -32603,
    };

    typedef decltype(iod::D(
            prop(jsonrpc,                            String),
            prop(method,                             String),
            prop(id,                                 iod::Nullable<int>),
            prop(params(var(optional), var(ignore)), iod::Nullable<json::Object>)
    )) JrpcRequest;

    typedef decltype(iod::D(
            prop(jsonrpc,                            String),
            prop(result(var(optional), var(ignore)), iod::Nullable<json::Object>),
            prop(error(var(optional), var(ignore)),  iod::Nullable<RpcError>),
            prop(id,                                 iod::Nullable<int>)
    )) JrpcResponse;

    using ReturnType = std::pair<int,json::Object>;
    struct JsonRpcHandler {
        virtual ReturnType operator()(const String& method, const json::Object& params, int id) {
            return std::make_pair(JRPC_METHOD_NOT_FOUND, json::Object("Method not implemented"));
        };
    };

    struct JsonRpcServerConnection : RpcTxRx, LOGGER(JSON_RPC) {
        using suil::log::Logger<dtag(JSON_RPC)>::log;

        JsonRpcServerConnection();

        void operator()(SocketAdaptor& sock, JsonRpcHandler *h);

    private:
        json::Object getVersion();
        json::Object rpcConfigure(json::Object &obj);
        String parse_Request(std::vector<JrpcRequest>& req, const OBuffer& ob);
        std::string handleRequest(const OBuffer &req);
        JrpcResponse handle_Extension(const String& method, const json::Object& req, int id = 0);
        JrpcResponse handle_WithHandler(JsonRpcHandler& h, const String& method, const json::Object& req, int id);

    private:
        using ExtensionMethod = std::function<ReturnType(const json::Object& params)>;
        JsonRpcHandler      *handler;
        Map<ExtensionMethod> extensionMethods{};
    };

    struct JsonRpcConfig: ServerConfig {
    };

    template <typename Sock = TcpSs>
    struct _JsonRpcServer: Server<JsonRpcServerConnection, Sock, JsonRpcHandler>, LOGGER(JSON_RPC) {

        using Base = Server<JsonRpcServerConnection, Sock, JsonRpcHandler>;

        template <typename... Args>
        _JsonRpcServer(JsonRpcHandler& proto, Args... args)
            : Base(config, config, &proto)
        {
           utils::apply_config(Ego.config, std::forward<Args>(args)...);
        }

    private:
        JsonRpcConfig config;
    };

    struct __JsonRpcClient: RpcTxRx, LOGGER(JSON_RPC) {
        using suil::log::Logger<dtag(JSON_RPC)>::log;

        virtual bool connect(String&& host, int port);
        String rpc_Version();

        template <typename... Params>
        ReturnType call(String&& method, Params... args) {
            if constexpr(sizeof...(args)) {
                json::Object params(json::Obj);
                params.set(std::forward<Params>(args)...);
                return Ego.call(std::move(method), std::move(params));
            }
            return Ego.call(std::move(method), json::Object(nullptr));
        }

        ReturnType call(String&& method, json::Object&& params);

        template <typename... Args>
        std::vector<ReturnType> batch(String&& method, json::Object&& params, Args... args) {
            std::vector<JrpcRequest> package;
            Ego.pack(package, std::move(method), std::move(params), std::forward<Args>(args)...);
            return Ego.call(package);
        }

    protected:
        __JsonRpcClient(SocketAdaptor& sock)
            : sock(sock)
        {}

        SocketAdaptor& sock;

    private:

        template <typename... Args>
        void pack(std::vector<JrpcRequest>& package, String&& method, json::Object&& params, Args... args) {
            JrpcRequest req;
            iod::zero(req);
            req.method  = std::move(method);
            req.id      = idGenerator++;
            req.jsonrpc = JSON_RPC_VERSION;
            if (!params.empty())
                req.params  = std::move(params);
            package.push_back(std::move(req));
            if constexpr(sizeof...(args)) {
                /*call with next request */
                Ego.pack(package, std::forward<Args>(args)...);
            }
        }

        std::vector<ReturnType> call(std::vector<JrpcRequest>& package);

        std::vector<ReturnType> transformResponses(std::vector<JrpcResponse>&& resps);

        int idGenerator{0};
    };

    template <typename Sock = TcpSock>
    struct _JsonRpcClient: __JsonRpcClient {
        _JsonRpcClient()
            : __JsonRpcClient(tcpProto)
        {}
    private:
        Sock  tcpProto;
    };

    using JsonRpcServer    = _JsonRpcServer<TcpSs>;
    using JsonRpcSslServer = _JsonRpcServer<SslSs>;

    using JsonRpcClient   = _JsonRpcClient<TcpSock>;
    using JsonRpcSslClient = _JsonRpcClient<SslSock>;
}
#endif //SUIL_JSONRPC_H
