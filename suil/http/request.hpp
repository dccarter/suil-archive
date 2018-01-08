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
        struct Connection;

        define_log_tag(HTTP_REQ);

        using form_data_it_t = std::function<bool(const zcstring&, const zcstring&)>;
        using form_file_it_t = std::function<bool(const UploadedFile&)>;

        struct Request;
        struct RequestForm {
            RequestForm(const Request& req);
            void operator|(form_data_it_t f);
            void operator|(form_file_it_t f);
            const zcstring operator[](const char*);
            const UploadedFile&operator()(const char*);
            template <typename O>
            void operator>>(O& o);
        private:
            bool find(zcstring& out, const char *name);

            const Request& req;
        };

        struct Request : public parser, LOGGER(dtag(HTTP_REQ)) {

            Request(SocketAdaptor& sock, HttpConfig& config)
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

            SocketAdaptor& adator() {
                return sock;
            }

            ~Request() {
                clear();
            }

            ssize_t read_body(void *buf, size_t len);

            bool body_seek(off_t off = 0);

            strview get_body();

            bool isvalid() const {
                return body_error || offload_error;
            }

            inline strview header(const zcstring& h) const {
                auto it = headers.find(h);
                if (it != headers.end()) {
                    const zcstring& v = it->second;
                    return strview(v.data(), v.size());
                }
                return strview();
            }

            inline strview header(const char* h) const {
                zcstring tmp(h);
                return header(tmp);
            }

            inline strview header(std::string& h) const {
                zcstring tmp(h.data(), h.size(), false);
                return header(tmp);
            }

            inline void header(zcstring&& h, const std::string v) {
                // we dup the value here
                zcstring vv = zcstring(v.data(), v.size(), false);
                headers.emplace(std::move(h), std::move(vv.dup()));
            }

            void *middleware_context{};

            inline CookieIterator operator()() {
                return CookieIterator(cookies);
            }

            template <typename _F>
            void operator|(_F f) const {
                for(auto h: headers) {
                    f(h.first.data(), h.second.data());
                }
            }

            virtual void clear();

            const route_attributes_t& route() const {
                return *params.attrs;
            }

            const request_params_t& routeparams() const {
                return params;
            }

            template <typename __T>
            __T query(const char *name) const {
                return qps.get<__T>(name);
            }

        private:
            template <typename __H, typename... __Mws>
            friend struct Connection;
            friend struct SystemAttrs;
            friend struct RequestForm;

            Status process_headers();
            virtual int handle_body_part(const char *at, size_t length);
            virtual int msg_complete();
            bool parse_cookies();
            bool parse_form();
            bool parse_url_encoded_form();
            bool parse_multipart_form(const zcstring& boundary);

            inline bool any_method() const {
                return false;
            }
            inline bool any_method(Method m) const {
                return (uint8_t)m == method;
            }

            template <typename... __M>
            inline bool any_method(Method m, __M... mm) const {
                return any_method(m) || any_method(mm...);
            }

            Status receive_headers(ServerStats& stats);
            Status receive_body(ServerStats& stats);

            zmap<zcstring>    cookies;
            bool                     cookied{false};

            zmap<UploadedFile>    files;
            zmap<zcstring>    form;
            zcstring                 form_str;
            bool                     formed{false};

            struct BodyOffload : File {
                BodyOffload(zbuffer& path);
                zcstring  path;
                size_t    length;
                virtual ~BodyOffload()
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
            BodyOffload            *offload{nullptr};
            zbuffer                stage{0};

            SocketAdaptor&       sock;
            HttpConfig&      config;

            friend class Router;
            request_params_t params;
        };

        template <typename O>
        void RequestForm::operator>>(O& o) {
            iod::foreach2(o) |
            [&](auto& m) {
                zcstring name{m.symbol().name()};
                auto it = req.form.find(name);
                if (it != req.form.end()) {
                    // cast value to correct type
                    utils::cast(it->second, m.value());
                }
            };
        }
    }

}

#endif //SUIL_HTTP_REQUEST_HPP
