//
// Created by dc on 27/06/17.
//

#ifndef SUIL_HTTP_REQUEST_HPP
#define SUIL_HTTP_REQUEST_HPP

#include "parser.hpp"
#include "sock.hpp"

namespace suil {

    namespace http {

        template <typename __H, typename... __Mws>
        struct connection;

        define_log_tag(HTTP_REQ);

        struct request : public parser, LOGGER(dtag(HTTP_REQ)) {

            request(sock_adaptor& sock, http_config_t& config)
                : stage(0),
                  sock(sock),
                  config(config)
            {}

            const char *ip() const {
                return ipstr(sock.addr());
            }

            int port() {
                return sock.port();
            }

            sock_adaptor& adator() {
                return sock;
            }

            ~request() {
                clear();
            }

            ssize_t read_body(void *buf, size_t len);

            bool body_seek(off_t off = 0);

            strview_t get_body();

            bool isvalid() const {
                return body_error || offload_error;
            }

            inline strview_t header(const zcstring& h) const {
                auto it = headers.find(h);
                if (it != headers.end()) {
                    const zcstring& v = it->second;
                    return strview_t(v.cstr, v.len);
                }
                return strview_t();
            }

            inline strview_t header(const char* h) const {
                zcstring tmp(h);
                return header(tmp);
            }

            inline strview_t header(std::string& h) const {
                zcstring tmp(h.data(), h.size(), false);
                return header(tmp);
            }

            inline void header(zcstring&& h, const std::string v) {
                // we copy the value here
                zcstring vv = zcstring(v.data(), v.size(), false);
                headers.emplace(h, std::move(vv));
            }

            void *middleware_context{};

            cookie_it operator()() {
                if (!cookied) {
                    parse_cookies();
                }
                return cookie_it(cookies);
            }

            template <typename _F>
            void operator|(_F f) const {
                for(auto h: headers) {
                    f(h.first.cstr, h.second.cstr);
                }
            }

            virtual void clear();

        private:
            template <typename __H, typename... __Mws>
            friend struct connection;

            status_t process_headers();
            virtual int handle_body_part(const char *at, size_t length);
            bool parse_cookies();
            status_t receive_headers();
            status_t receive_body();

            zcstr_map_t<upfile_t>    files;
            bool                     filed{false};
            zcstr_map_t<zcstr<>>     cookies;
            bool                     cookied{false};
            zcstr_map_t<std::string> form;
            bool                     formed{false};

            struct body_offload : file_t {
                body_offload(buffer_t& path);
                zcstring  path;
                size_t    length;
                virtual ~body_offload()
                {}
            };

            struct {
                uint8_t has_body      : 1;
                uint8_t body_read     : 1;
                uint8_t body_error    : 1;
                uint8_t offload_error : 1;
                uint8_t _u8           : 4;
            };

            uint32_t                body_offset{0};
            body_offload            *offload{nullptr};
            buffer_t                stage{0};

            sock_adaptor&    sock;
            http_config_t&   config;
        };
    }

}

#endif //SUIL_HTTP_REQUEST_HPP
