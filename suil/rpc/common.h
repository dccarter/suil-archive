//
// Created by dc on 27/12/18.
//

#ifndef SUIL_COMMON_H
#define SUIL_COMMON_H

#include <suil/net.h>

namespace suil::rpc {

    typedef decltype(iod::D(
        prop(code,    int),
        prop(message, String),
        prop(data,    String)
    )) RpcError;

    define_log_tag(RPC);

    struct RpcTxRx: LOGGER(RPC) {

        virtual bool receiveRaw(SocketAdaptor &sock, OBuffer &rxb);

        virtual bool sendRaw(SocketAdaptor &sock, const std::string &resp) {
            suil::Data tmp{resp.c_str(), resp.size(), false};
            return Ego.sendRaw(sock, tmp);
        }

        virtual bool sendRaw(SocketAdaptor &sock, const suil::Data& resp);

    protected:
        bool protoUseSize{false};
    };

}
#endif //SUIL_COMMON_H
