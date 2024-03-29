//
// Created by dc on 27/06/17.
//

#ifndef SUIL_HTTP_REQUEST_HPP
#define SUIL_HTTP_REQUEST_HPP

#include <suil/http/parser.h>
#include <suil/sock.h>
#include <suil/file.h>

namespace suil {

    namespace http {

        template <typename H, typename... __Mws>
        struct Connection;

        define_log_tag(HTTP_REQ);

        using form_data_it_t = std::function<bool(const String&, const String&)>;
        using form_file_it_t = std::function<bool(const UploadedFile&)>;

        struct Request;
        struct RequestForm {
            RequestForm(const Request& req, std::vector<String>&& required = {});
            void operator|(form_data_it_t f);
            void operator|(form_file_it_t f);
            const String operator[](const char*);
            const UploadedFile&operator()(const char*);
            template <typename... O>
            String operator>>(iod::sio<O...>& o);
            template <typename T, typename  std::enable_if<std::is_base_of<iod::MetaType, T>::value>::type* = nullptr>
            String operator>>(T& o);
        private:

            template <typename T>
            inline static void cast(const String& s, T& v) {
                utils::cast(s, v);
            }

            template <typename T>
            inline static void cast(const String& s, std::vector<T>& v) {
            }

            bool find(String& out, const char *name);

            const Request& req;
            std::vector<suil::String> required{};
        };

        struct Request : public parser, LOGGER(HTTP_REQ) {

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

            inline strview header(const String& h) const {
                auto it = headers.find(h);
                if (it != headers.end()) {
                    const String& v = it->second;
                    return strview(v.data(), v.size());
                }
                return strview();
            }

            inline strview header(const char* h) const {
                String tmp(h);
                return header(tmp);
            }

            inline strview header(std::string& h) const {
                String tmp(h.data(), h.size(), false);
                return header(tmp);
            }

            inline void header(String&& h, const std::string v) {
                // we dup the value here
                String vv = String(v.data(), v.size(), false);
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

            virtual void clear(bool internal = false);

            const route_attributes_t& route() const {
                return *params.attrs;
            }

            const request_params_t& routeparams() const {
                return params;
            }

            template <typename T>
            T query(const char *name, bool strict = false) const {
                return qps.get<T>(name, strict);
            }

            template <typename T>
            T query(const char *name, OBuffer& msg) const {
                try {
                    return qps.get<T>(name, true);
                }
                catch (Exception& e) {
                    if (!msg.empty())
                        msg << "\n";
                    msg << e.what();
                    return T{};
                }
            }

            template <typename T>
            T toJson() const {
                T tmp;
                iod::json_decode(tmp, Ego.body);
                return std::move(tmp);
            }

        private:
            template <typename H, typename... Mws>
            friend struct Connection;
            friend struct SystemAttrs;
            friend struct RequestForm;

            Status process_headers();
            virtual int handle_body_part(const char *at, size_t length);
            virtual int msg_complete();
            bool parse_cookies();
            bool parseForm();
            bool parse_url_encoded_form();
            bool parse_multipart_form(const String& boundary);

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

            CaseMap<String>    cookies;
            bool                     cookied{false};

            CaseMap<UploadedFile>    files;
            CaseMap<String>          form;
            String                   form_str;
            bool                     formed{false};

            struct BodyOffload : File {
                BodyOffload(OBuffer& path);
                String  path;
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
            OBuffer                stage{0};

            SocketAdaptor&       sock;
            HttpConfig&      config;

            friend class Router;
            request_params_t params;
        };

        template <typename... O>
        String RequestForm::operator>>(iod::sio<O...>& o) {
            OBuffer ob{127};
            iod::foreach2(o) |
            [&](auto& m) {
                String name{m.symbol().name()};
                bool isRequired = std::find(required.begin(), required.end(), name) != required.end();
                auto it = req.form.find(name);
                if (it != req.form.end()) {
                    // for has value
                    if (isRequired && it->second.empty()) {
                        /* required field cannot be empty */
                        ob << (ob.empty()? "" : "\n") << "Required field '" << name << "' cannot be empty";
                    }
                    else {
                        /* parse value */
                        try {
                            RequestForm::cast(it->second, m.value());
                        }
                        catch (...) {
                            /* unhandled error, parsing failed */
                            ob << (ob.empty() ? "" : "\n") << "Field '" << name << "' has unsupported value.";
                        }
                    }
                }
                else if(isRequired) {
                    /* required field missing */
                    ob << (ob.empty() ? "" : "\n") << "Required field '" << name << "' was not provided";
                }
            };

            return ob.empty()? nullptr : String(ob);
        }

        template <typename T, typename  std::enable_if<std::is_base_of<iod::MetaType, T>::value>::type* = nullptr>
        String RequestForm::operator>>(T& o) {
            OBuffer ob{127};
            iod::foreach2(T::Meta) |
            [&](auto& m) {
                String name{m.symbol().name()};
                bool isRequired = std::find(required.begin(), required.end(), name) != required.end();
                auto it = req.form.find(name);
                if (it != req.form.end()) {
                    // for has value
                    if (isRequired && it->second.empty()) {
                        /* required field cannot be empty */
                        ob << (ob.empty()? "" : "\n") << "Required field '" << name << "' cannot be empty";
                    }
                    else {
                        /* parse value */
                        try {
                            RequestForm::cast(it->second, m.symbol().member_access(o));
                        }
                        catch (...) {
                            /* unhandled error, parsing failed */
                            ob << (ob.empty() ? "" : "\n") << "Field '" << name << "' has unsupported value.";
                        }
                    }
                }
                else if(isRequired) {
                    /* required field missing */
                    ob << (ob.empty() ? "" : "\n") << "Required field '" << name << "' was not provided";
                }
            };

            return ob.empty()? nullptr : String(ob);
        }
    }

}

#endif //SUIL_HTTP_REQUEST_HPP
