//
// Created by dc on 28/06/17.
//

#ifndef SUIL_RESPONSE_HPP
#define SUIL_RESPONSE_HPP

#include "http.hpp"
#include "log.hpp"

#include "iod/json.hh"

namespace suil {
    namespace http {

        struct websock_api;
        struct request;
        struct response;

        using proto_handler_t = std::function<bool(request&, response&)>;

        define_log_tag(HTTP_RESP);
        struct response : LOGGER(dtag(HTTP_RESP)) {
            response()
                : response(status_t::OK)
            {}

            response(const status_t status)
                : body(0),
                  status(status)
            {}

            response(const std::string& resp)
                : body(resp.size()+2),
                  status(status_t::OK)
            {
                body.append(resp.data(), resp.size());
            }

            response(const zcstring& resp)
                : body(resp.len+2),
                  status(status_t::OK)
            {
                body.append(resp.cstr, resp.len);
            }

            response(const char* resp)
                : body(0),
                  status(status_t::OK)
            {
                body << resp;
            }

            response(const int64_t& data)
                : body(15),
                  status(status_t::OK)
            {
                body << data;
            }

            response(const int& data)
                : body(15),
                  status(status_t::OK)
            {
                body << data;
            }

            response(const unsigned& data)
                : body(15),
                  status(status_t::OK)
            {
                body << data;
            }

            response(const uint64_t& data)
                : body(15),
                  status(status_t::OK)
            {
                body << data;
            }

            response(const double& data)
                : body(15),
                  status(status_t::OK)
            {
                body << data;
            }


            response(status_t status, buffer_t& body)
                : body(0),
                  status(status)
            {
                body = std::move(body);
            }

            template<typename _T>
            response(const _T& data)
                : body(0),
                  status(status_t::OK)
            {
                set_content_type("application/json");
                body << iod::json_encode(data).c_str();
            }

            response(response&&);

            response&operator=(response&&);

            void end(status_t status = status_t::OK);

            void end(status_t status, buffer_t& body);

            void set_content_type(const char *type) {
                header("Content-Type", type);
            }

            void clear();

            inline void append(const std::string &str) {
                body.append(str.data(), str.size());
            }

            inline void append(const void *data, size_t len) {
                body.append(data, (uint32_t)len);
            }

            inline void append(const char *cstr) {
                body.append(cstr);
            }

            inline void append(const zcstring& str) {
                body.append(str.str, str.len);
            }

            inline void appendf(const char *fmt, ...) {
                va_list  args;
                va_start(args, fmt);
                body.appendv(fmt, args);
                va_end(args);
            }

            inline void header(zcstring field, zcstring value) {
                headers.emplace(std::move(field), std::move(value));
            }

            inline void header(const char* field, const char* value) {
                zcstring h(field);
                zcstring v(value);
                header(std::move(h.dup()), std::move(v.dup()));
            }

            inline void header(const char* field, std::string& value) {
                zcstring h(field);
                zcstring v(value.data(), value.size(), false);
                header(std::move(h.dup()), std::move(v.dup()));
            }

            inline void header(const char* field, buffer_t& value) {
                zcstring h(field);
                zcstring v(value, true);
                header(std::move(h.dup()), std::move(v));
            }

            strview_t header(zcstring& field) const {
                auto it = headers.find(field);
                if (it != headers.end()) {
                    return it->second;
                }
                return strview_t();
            }

            strview_t header(const char *field) const {
                zcstring tmp(field);
                return header(field);
            }

            strview_t header(std::string& field) const {
                zcstring tmp(field.data(), field.size(), false);
                return header(field);
            }

            void cookie(cookie_t& ck) {
                if (ck) {
                    // even after we move the cookies name, the peeked
                    // name will still point to the right point
                    cookies.emplace(ck.name().peek(), std::move(ck));
                }
            }

            size_t length() const {
                if (body)
                    return body.size();
                else if (chunks.size())
                    return total_size_;
                else
                    return 0;
            }

            inline bool iscompleted() const {
                return completed;
            }

            void end(proto_handler_t p);

            inline void redirect(status_t status, const char *location) {
                header("Location", location);
                end(status);
            }

        private:
             proto_handler_t operator()() {
                 return proto;
             }

            void flush_cookies();

            struct chunk_t {
                union {
                    int     fd;
                    void    *data;
                };

                off_t       offset;

                size_t      len{0};

                bool     use_fd{0};

                chunk_t(int fd, off_t offset, size_t len)
                    : fd(fd), offset(offset), len(len), use_fd(1)
                {}
                chunk_t(int fd, size_t len)
                        : chunk_t(fd, 0, len)
                {}

                chunk_t(void *data, off_t offset, size_t len)
                    : data(data), offset(offset), len(len), use_fd(0)
                {}

                chunk_t(void *data, size_t len)
                    : chunk_t(data, 0, len)
                {}
            };

            template <typename __H, typename ...Mws>
            friend struct connection;
            friend struct file_server;


            void chunk(chunk_t chunk) {
                assert(!body);
                total_size_ += (chunk.len-chunk.offset);
                chunks.push_back(chunk);
            }

            std::vector<chunk_t>  chunks;
            size_t                total_size_{0};

#ifdef SUIL_UT_ENABLED
        public:
#endif
            zcstr_map_t<zcstring>   headers;
            cookie_jar_t            cookies;
            buffer_t                body;
            status_t                status;
            bool                    completed{false};
            proto_handler_t         proto{nullptr};
        };
    }
}

#endif //SUIL_RESPONSE_HPP
