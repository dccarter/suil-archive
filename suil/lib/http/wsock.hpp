//
// Created by dc on 28/06/17.
//

#ifndef SUIL_WSOCK_HPP
#define SUIL_WSOCK_HPP

#include "request.hpp"
#include "response.hpp"

namespace suil {
    namespace http {

        enum ws_op_t : uint8_t  {
            CONT    = 0x00,
            TEXT    = 0x01,
            BINARY  = 0x02,
            CLOSE   = 0x08,
            PING    = 0x09,
            PONG    = 0x0A
        };

        struct websock;
        struct websock_api {
            websock_api();
            typedef std::function<bool(websock&)> connect_handler_t;

            typedef std::function<void(websock&)> close_handler_t;

            typedef std::function<void()>         disconnect_handler_t;

            typedef std::function<void(websock&, const buffer_t&, ws_op_t)> msg_handler_t;

            connect_handler_t       on_connect{nullptr};

            disconnect_handler_t    on_disconnect{nullptr};

            msg_handler_t           on_message{nullptr};

            close_handler_t         on_close{nullptr};

            int64_t                 timeout{-1};

        private:
            friend struct websock;

            void broadcast(websock* src, const void *data, size_t size);

            void notify_conn(bool conn);

            static coroutine void   send(chan ch, websock& ws, const void *data, size_t sz, ws_op_t op);

            static coroutine void   bsend(async_t<int>&, websock& ws, const void *data, size_t len);

            zcstr_map_t<websock&>   websocks{};

            size_t                  nsocks{0};

            uint8_t                 id;
        };

        inline void ws_handshake(const request&, response&, websock_api& api);

        struct wsock_bcast_msg {
            uint8_t         api_id;
            size_t          len;
            uint8_t         payload[0];
        } __attribute((packed));

        #define IPC_WSOCK_BCAST ipc_msg(1)
        #define IPC_WSOCK_CONN  ipc_msg(2)

        /**
         * The web socket connection message. This message is sent to other
         * worker whenever a worker receives a new web socket connection and
         * whenever a socket disconnects
         */
        struct wsock_conn_msg {
            uint8_t         api_id;
            uint8_t         conn;
        } __attribute((packed));

        define_log_tag(WEB_SOCKET);
        struct websock : LOGGER(dtag(WEB_SOCKET)) {

            bool send(const void *, size_t, ws_op_t);
            bool send(const void *data, size_t size) {
                return send(data, size, ws_op_t::BINARY);
            }
            bool send(const zcstr<>& zc, ws_op_t op = ws_op_t::TEXT) {
                return send(zc.str, zc.len, op);
            }
            inline bool send(const char *str) {
                return send(str, strlen(str), ws_op_t::TEXT);
            }

            inline bool send(const buffer_t& b, ws_op_t op = ws_op_t::BINARY) {
                return send(b.data(), b.size(), op);
            }

            void broadcast(const void *data, size_t sz, ws_op_t op);

            inline void broadcast(const zcstr<>& zc, ws_op_t op = ws_op_t::TEXT) {
                broadcast(zc.cstr, zc.len, op);
            }

            inline void broadcast(const buffer_t& b, ws_op_t op = ws_op_t::BINARY) {
                broadcast(b.data(), b.size(), op);
            }

            inline void broadcast(const char *str) {
                broadcast(str, strlen(str), ws_op_t::TEXT);
            }

            void close();

            ~websock() {
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
            ws_handshake(const request&, response&, websock_api& api);
            static status_t handshake(const request&, response&, websock_api&, size_t);

            websock(sock_adaptor& adaptor, websock_api& api, size_t size = 0)
                : sock(adaptor),
                  api(api)
            {
                if (size) {
                    /* allocate data memory */
                    data_ = memory::calloc(1, size);
                }
            }

            template <typename __H, typename ...Mws>
            friend struct connection;
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
            virtual bool receive_frame(header& h, buffer_t& b);

            sock_adaptor&       sock;
            websock_api&        api;
            bool                end_session{false};
            void                *data_{nullptr};
        private:
            friend struct websock_api;
            void handle();
            bool bsend(const void *data, size_t len);
            static coroutine void broadcast(
                    websock& ws, websock_api& api, void *data, size_t size);
        };

        template <typename __T = __Void>
        inline void ws_handshake(const request& req, response& res, websock_api& api) {
            status_t  s = websock::handshake(req, res, api, sizeof(__T));
            res.end(s);
        }

        template <typename... _Opts>
        static inline websock_api ws_api(_Opts... opts) {
            websock_api api{};
            utils::apply_config(api, opts...);
            return api;
        }

    }
}
#endif //SUIL_WSOCK_HPP
