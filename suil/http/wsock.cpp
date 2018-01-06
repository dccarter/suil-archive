//
// Created by dc on 28/06/17.
//
#include <openssl/sha.h>
#include <suil/http/wsock.hpp>

#define WS_FRAME_HDR		2
#define WS_MASK_LEN		    4
#define WS_FRAME_MAXLEN		16384
#define WS_PAYLOAD_SINGLE	125
#define WS_PAYLOAD_EXTEND_1	126
#define WS_PAYLOAD_EXTEND_2	127
#define WS_OPCODE_MASK		0x0f
#define WS_SERVER_RESPONSE	"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

namespace suil {
    namespace http {

        static uint8_t api_index{0};
        static std::unordered_map<uint8_t, WebSockApi&> apis{};

        WebSockApi::WebSockApi()
        {
            id = api_index++;
            apis.emplace(id, *this);
        }

        Status WebSock::handshake(const Request &req, Response &res, WebSockApi &api, size_t size) {
            SHA_CTX         sctx;
            strview  key, version;

            key = req.header("Sec-WebSocket-Key");
            if (key.empty()) {
                // we could throw an error here
                return Status::BAD_REQUEST;
            }

            version = req.header("Sec-WebSocket-Version");
            if (version.empty() || version != "13") {
                // currently only version 13 supported
                res.header("Sec-WebSocket-Version", "13");
                return Status::BAD_REQUEST;
            }

            zbuffer     buf(127);
            uint8_t         digest[SHA_DIGEST_LENGTH];
            buf += key;
            buf.append(WS_SERVER_RESPONSE, sizeof(WS_SERVER_RESPONSE)-1);
            SHA1_Init(&sctx);
            SHA1_Update(&sctx, (const void*)buf, buf.size());
            SHA1_Final(digest, &sctx);
            buf.clear();

            zcstring base64 = base64::encode(digest, sizeof(digest));
            res.header("Upgrade", "WebSocket");
            res.header("Connection", "Upgrade");
            res.header("Sec-WebSocket-Accept", std::move(base64));

            // end the Response by the handler
            res.end([&api,&size](Request &rq, Response &rs) {
                // clear the Request to free resources
                rq.clear();

                // Create a web socket
                WebSock ws(rq.adator(), api, size);

                ws.handle();

                return true;
            });

            // return the switching protocols
            return Status::SWITCHING_PROTOCOLS;
        }

        bool WebSock::receive_opcode(header& h) {
            size_t  len{0};
            size_t  nbytes = WS_FRAME_HDR;

            if (!sock.receive(&h.u16All, nbytes, api.timeout)) {
                trace("%s - receiving op code failed: %s", sock.id(), errno_s);
                return false;
            }

            if (!h.mask) {
                idebug("%s - received frame does not have op code mask", sock.id());
                return false;
            }

            if (h.rsv1 || h.rsv2 || h.rsv3) {
                idebug("%s - receive has RSV bits set %d:%d:%d",
                      sock.id(), h.rsv1, h.rsv2, h.rsv3);
                return false;
            }

            switch (h.opcode) {
                case WsOp::CONT:
                case WsOp::TEXT:
                case WsOp::BINARY:
                    break;
                case WsOp::CLOSE:
                case WsOp::PING:
                case WsOp::PONG:
                    if (h.len > WS_PAYLOAD_SINGLE || !h.fin) {
                        idebug("%s - frame (%hX) to large or fragmented",
                              sock.id(), h.u16All);
                        return false;
                    }
                    break;
                default:
                    idebug("%s - unrecognised op code (%hX)",
                          sock.id(), h.u16All);
                    return false;
            }

            uint8_t extra_bytes{0};
            switch (h.len) {
                case WS_PAYLOAD_EXTEND_1:
                    extra_bytes = sizeof(uint16_t);
                    break;
                case WS_PAYLOAD_EXTEND_2:
                    extra_bytes = sizeof(uint64_t);
                    break;
                default:
                    break;
            }

            if (extra_bytes) {
                uint8_t buf[sizeof(uint64_t)] = {0};
                buf[0] = (uint8_t) h.len;
                size_t read = (size_t ) (extra_bytes-1);
                if (!sock.receive(&buf[1], read, api.timeout)) {
                    trace("%s - receiving length failed: %s", sock.id(), errno_s);
                    return false;
                }
                if (h.len == WS_PAYLOAD_EXTEND_1) {
                    len = utils::read<uint16_t>(buf);
                }
                else {
                    len = utils::read<uint64_t>(buf);
                }
            }
            else {
                len = (uint8_t) h.len;
            }
            h.payload_size = len;
            // receive the mask
            nbytes = WS_MASK_LEN;
            if (!sock.receive(h.v_mask, nbytes, api.timeout)) {
                idebug("%s - receiving mask failed: %s", sock.id(), errno_s);
                return false;
            }

            return true;
        }

        bool WebSock::receive_frame(header& h, zbuffer& b) {
            if (!receive_opcode(h)) {
                idebug("%s - receiving op code failed", sock.id());
                return false;
            }

            size_t len = h.payload_size;
            b.reserve(h.payload_size+2);
            uint8_t *buf = (uint8_t *)(void *)b;
            if (!sock.receive(buf, len, api.timeout) || len != h.payload_size) {
                trace("%s - receiving web socket frame failed: %s", sock.id(), errno_s);
                return false;
            }

            for (uint i = 0; i < len; i++)
                buf[i] ^= h.v_mask[i%WS_MASK_LEN];
            // advance to end of buffer
            b.seek(len);

            return true;
        }

        void WebSock::handle() {
            // first let the user know of the Connection
            if (api.on_connect) {
                if (api.on_connect(*this)) {
                    // Connection rejected
                    trace("%s - websocket Connection rejected", sock.id());
                    return;
                }
            }

            zcstring key = std::move(zcstring(sock.id()).dup());
            api.websocks.emplace(key, *this);
            api.nsocks++;

            idebug("%s - entering Connection loop %lu", key(), api.nsocks);

            zbuffer b(0);
            while (!end_session && sock.isopen()) {
                header h;
                b.clear();

                // while the adaptor is still open
                if (!receive_frame(h, b)) {
                    // receiving frame failed, abort Connection
                    trace("%s - receive frame failed", ipstr(sock.addr()));
                    end_session = true;
                }
                else {
                    switch (h.opcode) {
                        case PONG:
                        case CONT:
                            ierror("%s - web socket op (%02X) not supported",
                                  sock.id(), h.opcode);
                            end_session = true;

                        case WsOp::TEXT:
                        case WsOp::BINARY:
                            if (api.on_message) {
                                // one way of appending null at end of string
                                (char *)b;
                                api.on_message(*this, b, (WsOp) h.opcode);
                            }
                            break;
                        case WsOp::CLOSE:
                            end_session = true;
                            if (api.on_close) {
                                api.on_close(*this);
                            }
                            break;
                        case WsOp::PING:
                            send(b, WsOp::PONG);
                            break;
                        default:
                            trace("%s - unknown web socket op %02X",
                                  sock.id(), h.opcode);
                            end_session = true;
                    }
                }
            }

            // remove from list of know web sockets
            api.websocks.erase(key);
            api.nsocks--;

            trace("%s - done handling web socket %hhu",
                  sock.id(), api.nsocks);

            // definitely disconnecting
            if (api.on_disconnect) {
                api.on_disconnect();
            }
        }

        bool WebSock::send(const void *data, size_t size, WsOp op) {
            uint8_t payload_1;
            uint8_t hbuf[14] = {0};
            uint8_t hlen = WS_FRAME_HDR;

            if (end_session) {
                trace("%s - sending while Session is closing is not allow",
                      sock.id());
                return false;
            }

            if (size > WS_PAYLOAD_SINGLE) {
                payload_1 = (uint8_t) ((size < USHRT_MAX)?
                                       WS_PAYLOAD_EXTEND_1 : WS_PAYLOAD_EXTEND_2);
            }
            else {
                payload_1 = (uint8_t) size;
            }
            header& h = (header &) hbuf;
            h.u16All  = 0;
            h.opcode  = (op & WS_OPCODE_MASK);
            h.fin     = 1;
            h.len     = (payload_1 & (uint8_t)~(1<<7));

            if (payload_1 > WS_PAYLOAD_SINGLE) {
                uint8_t *p = hbuf + hlen;
                if (payload_1 == WS_PAYLOAD_EXTEND_1) {
                    utils::write<uint16_t>(p, (uint16_t) size);
                    hlen += sizeof(uint16_t);
                }
                else {
                    utils::write<uint64_t>(p, (uint64_t) size);
                    hlen += sizeof(uint64_t);
                }
            }

            // send header
            if (sock.send(hbuf, hlen, api.timeout) != hlen) {
                trace("%s - sending header of length %hhu failed: %s",
                      sock.id(), hlen, errno_s);
                return false;
            }

            // send the reset of the message
            if (sock.send(data, size, api.timeout) != size) {
                trace("%s - sending data of length %lu failed: %s",
                      sock.id(), size, errno_s);
                return false;
            }

            sock.flush(api.timeout);
            return true;
        }

        bool WebSock::bsend(const void *data, size_t len) {
            if (!sock.isopen()) {
                iwarn("attempting to send to a closed websocket");
                return false;
            }
            ssize_t nsent = 0, tsent = 0;
            do {
                nsent = sock.send(data, len, api.timeout);
                if (!nsent) {
                    trace("sending websocket data failed: %s", errno_s);
                    return false;
                }

                tsent += nsent;
            } while ((size_t)tsent < len);

            sock.flush(api.timeout);
            return true;
        }

        void WebSock::broadcast(const void *data, size_t sz, WsOp op) {
            trace("WebSock::broadcast data %p, sz %lu, op 0x%02X", data, sz, op);

            if (end_session) {
                trace("%s - sending while Session is closing is not allow",
                      ipstr(sock.addr()));
                return;
            }

            // only broadcast when there are other web socket clients
            if (api.nsocks > 1) {
                trace("broadcasting %lu web sockets", api.nsocks);
                uint8_t *copy = (uint8_t *) memory::alloc(
                                     sizeof(WsockBcastMsg) + sz +16);
                WsockBcastMsg *msg = (WsockBcastMsg *)copy;

                uint8_t payload_1;
                uint8_t hlen = WS_FRAME_HDR;

                if (sz > WS_PAYLOAD_SINGLE) {
                    payload_1 = (uint8_t) ((sz < USHRT_MAX)?
                                           WS_PAYLOAD_EXTEND_1 : WS_PAYLOAD_EXTEND_2);
                }
                else {
                    payload_1 = (uint8_t) sz;
                }
                header& h = (header &) msg->payload;
                h.u16All  = 0;
                h.opcode  = (op & WS_OPCODE_MASK);
                h.fin     = 1;
                h.len     = (payload_1 & (uint8_t)~(1<<7));

                if (payload_1 > WS_PAYLOAD_SINGLE) {
                    uint8_t *p = ((uint8_t *) msg->payload) + hlen;
                    if (payload_1 == WS_PAYLOAD_EXTEND_1) {
                        utils::write<uint16_t>(p, (uint16_t) sz);
                        hlen += sizeof(uint16_t);
                    }
                    else {
                        utils::write<uint64_t>(p, (uint64_t) sz);
                        hlen += sizeof(uint64_t);
                    }
                }
                uint8_t *tmp = msg->payload + hlen;
                memcpy(tmp, data, sz);

                msg->len = sz + hlen;
                msg->api_id = api.id;
                // the copied buffer now belongs to the go-routine
                // being scheduled
                size_t len = sizeof(WsockBcastMsg)+msg->len;
                go(broadcast(*this, api, copy, len));
            }
        }

        coroutine void WebSock::broadcast(WebSock& ws, WebSockApi &api, void *data, size_t size) {
            strace("WebSock::broadcast data %p size %lu", data, size);
            // use the api to broadcast to all connected web sockets
            api.broadcast(&ws, data, size);
            // free the allocated memory
            strace("done broadcasting message %p", data);
            memory::free(data);
        }

        void WebSockApi::bsend(Async<int>& ch, WebSock& ws, const void *data, size_t len) {
            bool result = ws.bsend(data, len);
            if (ch)
                ch << result;
        }

        void WebSockApi::broadcast(WebSock* src,const void *data, size_t size) {
            strace("WebSockApi::broadcast src %p, data %p, size %lu",
                   src, data, size);

            Async<int> async(-1);
            uint32_t wait = 0;

            const WsockBcastMsg *msg = (const WsockBcastMsg *)data;

            for(auto ws : websocks) {
                if (&ws.second != src) {
                    if (websocks.size() == 1) {
                        WebSock& wsock = ws.second;
                        // there is no need to spawn go-routines if there is only
                        // one other node
                        if (!wsock.send(msg->payload, msg->len)) {
                            ltrace(&wsock, "sending web socket message failed");
                        }
                    }
                    else {
                        wait++;
                        go(bsend(async, ws.second, msg->payload, msg->len));
                    }
                }
            }

            strace("waiting for %u-local to complete %ld", wait, mnow());
            // wait for transactions to complete
            async(wait) | Void;
            strace("web socket broadcast completed %ld", mnow());
        }

        void WebSockApi::send(chan ch, WebSock &ws, const void *data, size_t sz, WsOp op) {
            bool result = ws.send(data, sz, op);
            if (ch)
                chs(ch, bool, result);
        }
    }
}