//
// Created by dc on 28/06/17.
//

#ifndef SUIL_WSOCK_HPP
#define SUIL_WSOCK_HPP

#include <suil/http/request.hpp>
#include <suil/http/response.hpp>

namespace suil {
    namespace http {

        enum WsOp : uint8_t  {
            CONT    = 0x00,
            TEXT    = 0x01,
            BINARY  = 0x02,
            CLOSE   = 0x08,
            PING    = 0x09,
            PONG    = 0x0A
        };

        struct WebSock;
        struct WebSockApi {
            WebSockApi();
            typedef std::function<bool(WebSock&)> connect_handler_t;

            typedef std::function<void(WebSock&)> close_handler_t;

            typedef std::function<void()>         disconnect_handler_t;

            typedef std::function<void(WebSock&, const zbuffer&, WsOp)> msg_handler_t;

            connect_handler_t       on_connect{nullptr};

            disconnect_handler_t    on_disconnect{nullptr};

            msg_handler_t           on_message{nullptr};

            close_handler_t         on_close{nullptr};

            int64_t                 timeout{-1};

        private:
            friend struct WebSock;

            void broadcast(WebSock* src, const void *data, size_t size);

            static coroutine void   send(chan ch, WebSock& ws, const void *data, size_t sz, WsOp op);

            static coroutine void   bsend(Async<int>&, WebSock& ws, const void *data, size_t len);

            zmap<WebSock&>   websocks{};
            size_t           nsocks{0};
            uint8_t          id;
        };

        inline void ws_handshake(const Request&, Response&, WebSockApi& api);

        struct WsockBcastMsg {
            uint8_t         api_id;
            size_t          len;
            uint8_t         payload[0];
        } __attribute((packed));

        #define IPC_WSOCK_BCAST ipc_msg(1)
        #define IPC_WSOCK_CONN  ipc_msg(2)

        /**
         * The web socket Connection message. This message is sent to other
         * worker whenever a worker receives a new web socket Connection and
         * whenever a socket disconnects
         */
        struct WsockConnMsg {
            uint8_t         api_id;
            uint8_t         conn;
        } __attribute((packed));

        define_log_tag(WEB_SOCKET);
        struct WebSock : LOGGER(dtag(WEB_SOCKET)) {

            bool send(const void *, size_t, WsOp);
            bool send(const void *data, size_t size) {
                return send(data, size, WsOp::BINARY);
            }
            bool send(const zcstring& zc, WsOp op = WsOp::TEXT) {
                return send(zc.data(), zc.size(), op);
            }
            inline bool send(const char *str) {
                return send(str, strlen(str), WsOp::TEXT);
            }

            inline bool send(const zbuffer& b, WsOp op = WsOp::BINARY) {
                return send(b.data(), b.size(), op);
            }

            void broadcast(const void *data, size_t sz, WsOp op);

            inline void broadcast(const zcstring& zc, WsOp op = WsOp::TEXT) {
                broadcast(zc.data(), zc.size(), op);
            }

            inline void broadcast(const zbuffer& b, WsOp op = WsOp::BINARY) {
                broadcast(b.data(), b.size(), op);
            }

            inline void broadcast(const char *str) {
                broadcast(str, strlen(str), WsOp::TEXT);
            }

            void close();

            ~WebSock() {
                if (data_) {
                    memory::free(data_);
                    data_ = nullptr;
                }
            }

            template <typename __T> __T&data() {
                return (__T&) *((__T*)data);
            }

        protected:
            friend inline void
            ws_handshake(const Request&, Response&, WebSockApi& api);
            static Status handshake(const Request&, Response&, WebSockApi&, size_t);

            WebSock(SocketAdaptor& adaptor, WebSockApi& api, size_t size = 0)
                : sock(adaptor),
                  api(api)
            {
                if (size) {
                    /* allocate data memory */
                    data_ = memory::calloc(1, size);
                }
            }

            template <typename __H, typename ...Mws>
            friend struct Connection;
            struct  header {
                header()
                    : u16All(0),
                      payload_size(0)
                {
                    memset(v_mask, 0, sizeof(v_mask));
                }

                union {
                    struct {
                        struct {
                            uint8_t opcode   : 4;
                            uint8_t rsv3     : 1;
                            uint8_t rsv2     : 1;
                            uint8_t rsv1     : 1;
                            uint8_t fin      : 1;
                        } __attribute__((packed));
                        struct {
                            uint8_t len      : 7;
                            uint8_t mask     : 1;
                        } __attribute__((packed));
                    };
                    uint16_t u16All;
                };
                size_t          payload_size;
                uint8_t         v_mask[4];
            } __attribute__((packed));

            virtual bool receive_opcode(header& h);
            virtual bool receive_frame(header& h, zbuffer& b);

            SocketAdaptor&       sock;
            WebSockApi&        api;
            bool                end_session{false};
            void                *data_{nullptr};
        private:
            friend struct WebSockApi;
            void handle();
            bool bsend(const void *data, size_t len);
            static coroutine void broadcast(
                    WebSock& ws, WebSockApi& api, void *data, size_t size);
        };

        template <typename __T = __Void>
        inline void ws_handshake(const Request& req, Response& res, WebSockApi& api) {
            Status  s = WebSock::handshake(req, res, api, sizeof(__T));
            res.end(s);
        }

        template <typename... _Opts>
        static inline WebSockApi ws_api(_Opts... opts) {
            WebSockApi api{};
            utils::apply_config(api, opts...);
            return api;
        }

    }
}
#endif //SUIL_WSOCK_HPP
