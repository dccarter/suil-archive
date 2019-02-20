//
// Created by dc on 27/12/18.
//

#include "common.h"

namespace suil::rpc {

    bool RpcTxRx::receiveRaw(SocketAdaptor &sock, suil::OBuffer &rxb)
    {
        rxb.reserve(512);
        size_t  size{rxb.capacity()};
        if (Ego.protoUseSize) {
            /* connection configured to prepend data size to payload */
            size_t _{sizeof(size)};
            if (!sock.receive(&size, _, -1)) {
                /* failed to receive size */
                trace("RPC server failed to receive request size: %s", errno_s);
                return false;
            }

            /* size received */
            size_t nread = le64toh(size);
            size = nread;
            rxb.reserve(size);
            if (!sock.receive(&rxb[0], nread, 10000)) {
                /* failed to receive the specified number of bytes */
                ierror("failed to receive %lu bytes from client: %s", size, errno_s);
                return false;
            }
            rxb.seek(nread);
            return true;
        }
        else {
            /* general JSON protocol, not size */
            size_t  nread = 0, tread = 0;
            int64_t timeout{-1};
            do {
                nread = rxb.capacity();
                if (!sock.read(&rxb[tread], nread, timeout) ||(nread == 0 && tread == 0)) {
                    /* reading failed*/
                    trace("reading request failed: %s", errno_s);
                    return errno == ETIMEDOUT? !rxb.empty() : false;
                }
                tread += nread;
                if (nread)
                    rxb.seek(nread);
                if (nread == rxb.capacity()) {
                    /* if we read up to capacity, try again */
                    timeout = 250_ms;
                    rxb.reserve(1024);
                }
                else {
                    /* done reading */
                    break;
                }
            } while (true);

            return true;
        }
    }

    bool RpcTxRx::sendRaw(suil::SocketAdaptor &sock, const suil::Data &resp)
    {
        if (Ego.protoUseSize) {
            /* We need to send the size first */
            size_t  size = htole64(resp.size());
            if (!sock.send(&size, sizeof(size), 5000)) {
                /* sending failed */
                iwarn("sending request/response size failed: %s", errno_s);
                return false;
            }
        }

        if (!sock.send(resp.cdata(), resp.size(), 5000)) {
            /* sending failed */
            iwarn("sending request/response of size %lu failed: %s", resp.size(), errno_s);
            return false;
        }

        sock.flush(1500);
        return true;
    }

}
