//
// Created by dc on 28/06/17.
//

#ifndef SUIL_RESPONSE_HPP
#define SUIL_RESPONSE_HPP

#include <iod/json.hh>

#include <suil/http.h>
#include <suil/logging.h>

namespace suil {
    namespace http {

        struct WebSockApi;
        struct Request;
        struct Response;

        using ProtocolHandler = std::function<bool(Request&, Response&)>;

        define_log_tag(HTTP_RESP);
        struct Response : LOGGER(HTTP_RESP) {
            Response()
                : Response(Status::OK)
            {}

            Response(const Status status)
                : body(0),
                  status(status)
            {}

            Response(const std::string& resp)
                : body(resp.size()+2),
                  status(Status::OK)
            {
                body.append(resp.data(), resp.size());
            }

            Response(const String& resp)
                : body(resp.size()+2),
                  status(Status::OK)
            {
                body.append(resp.data(), resp.size());
            }

            Response(const char* resp)
                : body(0),
                  status(Status::OK)
            {
                body << resp;
            }

            Response(const int64_t& data)
                : body(15),
                  status(Status::OK)
            {
                body << data;
            }

            Response(const int& data)
                : body(15),
                  status(Status::OK)
            {
                body << data;
            }

            Response(const unsigned& data)
                : body(15),
                  status(Status::OK)
            {
                body << data;
            }

            Response(const uint64_t& data)
                : body(15),
                  status(Status::OK)
            {
                body << data;
            }

            Response(const double& data)
                : body(15),
                  status(Status::OK)
            {
                body << data;
            }

            Response(const float& data)
                    : body(15),
                      status(Status::OK)
            {
                body << data;
            }


            Response(Status status, OBuffer& body)
                : body(0),
                  status(status)
            {
                body = std::move(body);
            }

            template<typename _T>
            Response(const _T& data)
                : body(0),
                  status(Status::OK)
            {
                setContentType("application/json");
                body << json::encode(data).c_str();
            }

            Response(Response&&);

            Response&operator=(Response&&);

            void end(Status status = Status::OK);

            void end(Status status, OBuffer& body);

            void setContentType(const char *type) {
                header("Content-Type", type);
            }

            Response& operator()(http::Status status) {
                // set the response status
                if (Ego.status == Status::OK)
                    this->status = status;
                return Ego;
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

            inline void append(const String& str) {
                body.append(str.data(), str.size());
            }

            template <typename __T>
            inline Response& operator<<(const __T data) {
                body << data;
                return *this;
            }

            inline void appendf(const char *fmt, ...) {
                va_list  args;
                va_start(args, fmt);
                body.appendv(fmt, args);
                va_end(args);
            }

            inline void header(String field, String value) {
                headers.emplace(std::move(field), std::move(value));
            }

            inline void header(const char* field, const char* value) {
                String h(field);
                String v(value);
                header(std::move(h.dup()), std::move(v.dup()));
            }

            inline void header(const char* field, std::string& value) {
                String h(field);
                String v(value.data(), value.size(), false);
                header(std::move(h.dup()), std::move(v.dup()));
            }

            inline void header(const char* field, OBuffer& value) {
                String h(field);
                String v(value, true);
                header(std::move(h.dup()), std::move(v));
            }

            strview header(String& field) const {
                auto it = headers.find(field);
                if (it != headers.end()) {
                    return it->second;
                }
                return strview();
            }

            strview header(const char *field) const {
                String tmp(field);
                return header(field);
            }

            strview header(std::string& field) const {
                String tmp(field.data(), field.size(), false);
                return header(field);
            }

            void cookie(Cookie& ck) {
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

            void end(ProtocolHandler p);

            inline void redirect(Status status, const char *location) {
                header("Location", location);
                end(status);
            }

            inline OBuffer& operator()(int) { return Ego.body; }

        private:
             ProtocolHandler operator()() {
                 return proto;
             }

            void flush_cookies();

            struct Chunk {
                union {
                    int     fd;
                    void    *data;
                };

                off_t       offset;

                size_t      len{0};

                bool     use_fd{0};

                Chunk(int fd, off_t offset, size_t len)
                    : fd(fd), offset(offset), len(len), use_fd(1)
                {}
                Chunk(int fd, size_t len)
                        : Chunk(fd, 0, len)
                {}

                Chunk(void *data, off_t offset, size_t len)
                    : data(data), offset(offset), len(len), use_fd(0)
                {}

                Chunk(void *data, size_t len)
                    : Chunk(data, 0, len)
                {}
            };

            template <typename __H, typename ...Mws>
            friend struct Connection;
            friend struct FileServer;


            void chunk(Chunk chunk) {
                assert(!body);
                total_size_ += (chunk.len-chunk.offset);
                chunks.push_back(chunk);
            }

            std::vector<Chunk>  chunks;
            size_t                total_size_{0};

#ifdef SUIL_UT_ENABLED
        public:
#endif
            CaseMap<String>   headers;
            cookie_jar_t            cookies;
            OBuffer                body;
            Status                status;
            bool                    completed{false};
            ProtocolHandler         proto{nullptr};
        };

        inline Response mkresp(http::Status status, String msg) {
            Response resp(status);
            resp << msg.peek();
            return std::move(resp);
        }
    }
}

#endif //SUIL_RESPONSE_HPP
