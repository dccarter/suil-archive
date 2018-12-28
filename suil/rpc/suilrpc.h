//
// Created by dc on 27/12/18.
//

#ifndef SUIL_SUILRPC_H
#define SUIL_SUILRPC_H

#include <suil/wire.h>
#include <suil/result.h>
#include <suil/rpc/common.h>

namespace suil::rpc {

    enum {
        SRPC_METHOD_NOT_FOUND = 0,
        SRPC_INTERNAL_ERROR,

        SRPC_API_ERROR        = 6000
    };

    typedef decltype(iod::D(
            prop(id,           int),
            prop(method,       int),
            prop(params,       suil::Data)
    )) SuilRpcRequest;

    typedef decltype(iod::D(
            prop(id,          int),
            prop(error,       RpcError),
            prop(data,        suil::Data)
    )) SuilRpcResponse;

    typedef decltype(iod::D(
        prop(id,             int),
        prop(name,           String)
    )) SuilRpcMethod;

    typedef decltype(iod::D(
        prop(version,         String),
        prop(methods,         std::vector<SuilRpcMethod>),
        prop(extensions,      std::vector<SuilRpcMethod>)
    )) SuilRpcMeta;

    struct SuilRpcHandler {
        virtual Result operator()(suil::Breadboard& results, const int method, suil::Breadboard& params, int id) {
            Result res(SRPC_METHOD_NOT_FOUND);
            res << "method 'id=" << method << "' does not exist";
            return std::move(res);
        };

        std::vector<SuilRpcMethod>& getMethodsRef() {
            return methodsMeta;
        }

        std::vector<SuilRpcMethod> getMethods() {
            return methodsMeta;
        }

    protected:
        std::vector<SuilRpcMethod> methodsMeta;
    };

    define_log_tag(SUIL_RPC);

    struct SuilRpcServerConnection : RpcTxRx, LOGGER(SUIL_RPC) {
        using suil::log::Logger<dtag(SUIL_RPC)>::log;

        struct Extensions : SuilRpcHandler {

            Extensions();

            Result operator()(suil::Breadboard& results, int method, suil::Breadboard& params, int id);
        };

        SuilRpcServerConnection() = default;

        void operator()(SocketAdaptor& sock, SuilRpcHandler *h);

    private:
        suil::Data handleRequest(const OBuffer &req);
        Result     handleExtension(suil::Breadboard& results, int method, suil::Breadboard& req, int id = 0);

    private:
        Extensions      extensionMethods{};
        SuilRpcHandler  *handler{nullptr};
        SuilRpcMeta     rpcMeta;
    };

    struct SuilRpcConfig: ServerConfig {
    };

    template <typename Sock = TcpSs>
    struct _SuilRpcServer: Server<SuilRpcServerConnection, Sock, SuilRpcHandler> {

        using Base = Server<SuilRpcServerConnection, Sock, SuilRpcHandler>;

        template <typename... Args>
        _SuilRpcServer(SuilRpcHandler& proto, Args... args)
                : Base(config, config, &proto)
        {
            utils::apply_config(Ego.config, std::forward<Args>(args)...);
        }

    private:
        SuilRpcConfig config;
    };

    struct __SuilRpcClient: RpcTxRx, LOGGER(SUIL_RPC) {
        using suil::log::Logger<dtag(SUIL_RPC)>::log;

        virtual bool connect(String&& host, int port);
        String getVersion();
        const SuilRpcMeta& getMeta();

    protected:
        template <typename T, typename... Params>
        T call(String&& method, Params... args) {
            suil::Stackboard<4096> sb;
            if constexpr(sizeof...(args)) {
                Ego.pack(sb, std::forward<Params>(args)...);
            }
            suil::Stackboard<> hb;
            SuilRpcResponse resp = Ego.call(hb, std::move(method), sb);
            if constexpr (!std::is_void<T>::value) {
                // only return a value for non-void types
                suil::Heapboard data(resp.data);
                T tmp{};
                data >> tmp;
                return std::move(tmp);
            }
        }

        __SuilRpcClient(SocketAdaptor& sock)
                : sock(sock)
        {}

        SocketAdaptor& sock;

    private:

        template <typename T, typename... Args>
        void pack(suil::Breadboard& params, T param, Args... args) {
            // pack parameters
            params << param;
            if constexpr (sizeof...(args)) {
                // pack rest of the parameters
                Ego.pack(params, std::forward<Args>(args)...);
            }
        }

        SuilRpcResponse call(suil::Breadboard& results, String&& method, suil::Breadboard& params);

        int          idGenerator{0};
        SuilRpcMeta  rpcMeta;
        Map<int>     extMethods;
        Map<int>     apiMethods;
    };

    template <typename Sock = TcpSock>
    struct _SuilRpcClient: __SuilRpcClient {
        _SuilRpcClient()
            : __SuilRpcClient(tcpProto)
        {}
    private:
        Sock  tcpProto;
    };


    using SuilRpcServer    = _SuilRpcServer<TcpSs>;
    using SuilRpcSslServer = _SuilRpcServer<SslSs>;

    using SuilRpcClient    = _SuilRpcClient<TcpSock>;
    using SuilRpcClientSsl = _SuilRpcClient<SslSock>;
}

#endif //SUIL_SUILRPC_H
