//
// Created by dc on 27/06/17.
//

#ifndef SUIL_HTTP_REQUEST_HPP
#define SUIL_HTTP_REQUEST_HPP

#include <suil/http/parser.hpp>
#include <suil/sock.hpp>

namespace suil {

    namespace http {

        template <typename __H, typename... __Mws>
        struct connection;

        define_log_tag(HTTP_REQ);

        using form_data_it_t = std::function<bool(const zcstring&, const zcstring&)>;
        using form_file_it_t = std::function<bool(const upfile_t&)>;

        struct request;
        struct request_form_t {
            request_form_t(const request& req);
            void operator|(form_data_it_t f);
            void operator|(form_file_it_t f);
            const zcstring operator[](const char*);
            const upfile_t&operator()(const char*);
        private:
            bool find(zcstring& out, const char *name);

            const request& req;
        };

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
                // we dup the value here
                zcstring vv = zcstring(v.data(), v.size(), false);
                headers.emplace(std::move(h), std::move(vv.dup()));
            }

            void *middleware_context{};

            inline cookie_it operator()() {
                return cookie_it(cookies);
            }

            template <typename _F>
            void operator|(_F f) const {
                for(auto h: headers) {
                    f(h.first.cstr, h.second.cstr);
                }
            }

            virtual void clear();

            const route_attributes_t& route() const {
                return *params.attrs;
            }

            template <typename __T>
            __T query(const char *name) const {
                return qps.get<__T>(name);
            }

        private:
            template <typename __H, typename... __Mws>
            friend struct connection;
            friend struct system_attrs;
            friend struct request_form_t;

            status_t process_headers();
            virtual int handle_body_part(const char *at, size_t length);
            virtual int msg_complete();
            bool parse_cookies();
            bool parse_form();
            bool parse_url_encoded_form();
            bool parse_multipart_form(const zcstring& boundary);

            inline bool any_method() const {
                return false;
            }
            inline bool any_method(method_t m) const {
                return (uint8_t)m == method;
            }

            template <typename... __M>
            inline bool any_method(method_t m, __M... mm) const {
                return any_method(m) || any_method(mm...);
            }

            status_t receive_headers(server_stats_t& stats);
            status_t receive_body(server_stats_t& stats);

            zcstr_map_t<zcstring>    cookies;
            bool                     cookied{false};

            zcstr_map_t<upfile_t>    files;
            zcstr_map_t<zcstring>    form;
            zcstring                 form_str;
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

            sock_adaptor&       sock;
            http_config_t&      config;

            friend class router_t;
            request_params_t params;
        };
    }

}

#endif //SUIL_HTTP_REQUEST_HPP
