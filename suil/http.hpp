//
// Created by dc on 27/06/17.
//

#ifndef SUIL_HTTP_HPP
#define SUIL_HTTP_HPP

#include <ostream>

#include "config.hpp"
#include "http/common.hpp"
#include "net.hpp"

namespace suil {

    struct cookie_t {

        cookie_t(const char *name, const char *value)
            : name_(name? zcstring(name).copy() : nullptr),
              value_(value? zcstring(value).copy() : nullptr),
              path_(nullptr),
              domain_(nullptr),
              expires_(0)
        {}

        cookie_t(const char *name)
            : cookie_t(name, nullptr)
        {}

        cookie_t(cookie_t&& ck)
            : name_(std::move(ck.name_)),
              value_(std::move(ck.value_)),
              path_(std::move(ck.path_)),
              domain_(std::move(ck.domain_)),
              secure_(std::move(ck.secure_)),
              expires_(ck.expires_)
        {
            ck.expires_ = -1;
        }

        cookie_t(const cookie_t&) = delete;
        cookie_t&operator=(const cookie_t&) = delete;

        cookie_t& operator=(cookie_t&& ck) {
            name_   = std::move(ck.name_);
            value_  = std::move(ck.value_);
            path_   = std::move(ck.path_);
            domain_ = std::move(ck.domain_);
            secure_ = std::move(ck.secure_);
            expires_= ck.expires_;
            ck.expires_ = -1;
            return *this;
        }

        inline bool operator==(const cookie_t& other) {
            return name_ == other.name_;
        }

        inline bool operator!=(const cookie_t& other) {
            return name_ != other.name_;
        }

        inline operator bool() const {
            return !empty();
        }

        inline const zcstring& name() const {
            return name_;
        }

        inline void value(zcstring&& v) {
            if (v != value_) {
                value_ = std::move(v);
            }
        }

        inline void value(const char *v) {
            zcstring zc(v);
            value(std::move(zc.copy()));
        }

        inline const zcstring& value() const {
            return value_;
        }

        inline const zcstring& path() const {
            return path_;
        }

        inline void path(const char *p) {
            zcstring zc(p);
            if (zc != path_) {
                path_ = zc.copy();
            }
        }

        inline const zcstring& domain() const {
            return domain_;
        }

        inline void domain(const char *d) {
            zcstring zc(d);
            if (zc != domain_) {
                // this will destroy the current zc and copy the new one
                domain_ = zc.copy();
            }
        }

        inline bool secure() const {
            return secure_;
        }

        inline void secure(bool val) {
            secure_ = val;
        }

        inline time_t expires() const {
            return expires_;
        }

        inline void expires(int64_t secs) {
            if (secs > 0) {
                expires_ = time(NULL) + secs;
            }
            else {
                expires_ = secs;
            }
        }

        inline void maxage(int64_t t) {
            maxage_ = t;
        }

        inline int64_t maxage() const {
            return maxage_;
        }

        ~cookie_t() {
        }

    private:

        zcstring    name_;
        zcstring    value_;
        zcstring    path_;
        zcstring    domain_;
        bool        secure_{false};
        int64_t     maxage_{0};
        time_t      expires_{0};

        bool empty() const {
            return name_.empty();
        }
    };
    using cookie_jar_t = zcstr_map_t<cookie_t>;

    struct cookie_it {
        typedef zcstr_map_t<zcstring> __jar_t;
        cookie_it(const __jar_t& j)
            : jar(j)
        {}

        template<typename __F>
        void operator|(__F f) {
            for (const auto ck: jar) {
                f((const zcstring&) ck.first, (const zcstring&)ck.second);
            }
        }

        const zcstring&& operator[](const char *name) {
            zcstring key(name);
            auto it = jar.find(key);
            if (it != jar.end()) {
                return std::move(it->second.peek());
            }
            return std::move(zcstring(nullptr));
        }

    private:
        const __jar_t&  jar;
    };

    struct http_config_t : public server_config {
        int64_t         connection_timeout{5000};
        bool            disk_offload{false};
        size_t          disk_offload_min{2048};
        size_t          max_body_len{35648};
        size_t          send_chunk{35648};
        uint64_t        keep_alive_time{3600};
        uint64_t        hsts_enable{3600};
        std::string     server_name{SUIL_SOFTWARE_NAME};
        std::string     offload_path{"./.body"};
    };

    namespace http {

        enum class method_t : unsigned char {
            Delete = 0,
            Get,
            Head,
            Post,
            Put,
            Connect,
            Options,
            Trace,
            Unknown
        };

        enum class status_t : int {
            CONTINUE			        = 100,
            SWITCHING_PROTOCOLS		    = 101,
            OK				            = 200,
            CREATED			            = 201,
            ACCEPTED			        = 202,
            NON_AUTHORITATIVE		    = 203,
            NO_CONTENT			        = 204,
            RESET_CONTENT		        = 205,
            PARTIAL_CONTENT		        = 206,
            MULTIPLE_CHOICES		    = 300,
            MOVED_PERMANENTLY		    = 301,
            FOUND			            = 302,
            SEE_OTHER			        = 303,
            NOT_MODIFIED		        = 304,
            USE_PROXY			        = 305,
            TEMPORARY_REDIRECT		    = 307,
            BAD_REQUEST			        = 400,
            UNAUTHORIZED		        = 401,
            PAYMENT_REQUIRED		    = 402,
            FORBIDDEN			        = 403,
            NOT_FOUND			        = 404,
            METHOD_NOT_ALLOWED		    = 405,
            NOT_ACCEPTABLE		        = 406,
            PROXY_AUTH_REQUIRED		    = 407,
            REQUEST_TIMEOUT		        = 408,
            CONFLICT			        = 409,
            GONE			            = 410,
            LENGTH_REQUIRED		        = 411,
            PRECONDITION_FAILED		    = 412,
            REQUEST_ENTITY_TOO_LARGE	= 413,
            REQUEST_URI_TOO_LARGE	    = 414,
            UNSUPPORTED_MEDIA_TYPE	    = 415,
            REQUEST_RANGE_INVALID	    = 416,
            EXPECTATION_FAILED		    = 417,
            INTERNAL_ERROR		        = 500,
            NOT_IMPLEMENTED		        = 501,
            BAD_GATEWAY			        = 502,
            SERVICE_UNAVAILABLE	    	= 503,
            GATEWAY_TIMEOUT		        = 504,
            BAD_VERSION			        = 505
        };

        static inline const char *
        status_text(status_t status)
        {
            const char		*r;

            switch (status) {
                case status_t::CONTINUE:
                    r = "HTTP/1.1 100 Continue";
                    break;
                case status_t::SWITCHING_PROTOCOLS:
                    r = "HTTP/1.1 101 Switching Protocols";
                    break;
                case status_t::OK:
                    r = "HTTP/1.1 200 OK";
                    break;
                case status_t::CREATED:
                    r = "HTTP/1.1 201 Created";
                    break;
                case status_t::ACCEPTED:
                    r = "HTTP/1.1 202 Accepted";
                    break;
                case status_t::NON_AUTHORITATIVE:
                    r = "HTTP/1.1 203 Non-Authoritative Information";
                    break;
                case status_t::NO_CONTENT:
                    r = "HTTP/1.1 204 No Content";
                    break;
                case status_t::RESET_CONTENT:
                    r = "HTTP/1.1 205 Reset Content";
                    break;
                case status_t::PARTIAL_CONTENT:
                    r = "HTTP/1.1 206 Partial Content";
                    break;
                case status_t::MULTIPLE_CHOICES:
                    r = "HTTP/1.1 300 Multiple Choices";
                    break;
                case status_t::MOVED_PERMANENTLY:
                    r = "HTTP/1.1 301 Moved Permanently";
                    break;
                case status_t::FOUND:
                    r = "HTTP/1.1 302 Found";
                    break;
                case status_t::SEE_OTHER:
                    r = "HTTP/1.1 303 See Other";
                    break;
                case status_t::NOT_MODIFIED:
                    r = "HTTP/1.1 304 Not Modified";
                    break;
                case status_t::USE_PROXY:
                    r = "HTTP/1.1 305 Use Proxy";
                    break;
                case status_t::TEMPORARY_REDIRECT:
                    r = "HTTP/1.1 307 Temporary Redirect";
                    break;
                case status_t::BAD_REQUEST:
                    r = "HTTP/1.1 400 Bad Request";
                    break;
                case status_t::UNAUTHORIZED:
                    r = "HTTP/1.1 401 Unauthorized";
                    break;
                case status_t::PAYMENT_REQUIRED:
                    r = "HTTP/1.1 402 Payment Required";
                    break;
                case status_t::FORBIDDEN:
                    r = "HTTP/1.1 403 Forbidden";
                    break;
                case status_t::NOT_FOUND:
                    r = "HTTP/1.1 404 Not Found";
                    break;
                case status_t::METHOD_NOT_ALLOWED:
                    r = "HTTP/1.1 405 method_t Not Allowed";
                    break;
                case status_t::NOT_ACCEPTABLE:
                    r = "HTTP/1.1 406 Not Acceptable";
                    break;
                case status_t::PROXY_AUTH_REQUIRED:
                    r = "HTTP/1.1 407 Proxy Authentication Required";
                    break;
                case status_t::REQUEST_TIMEOUT:
                    r = "HTTP/1.1 408 Request Time-out";
                    break;
                case status_t::CONFLICT:
                    r = "HTTP/1.1 409 Conflict";
                    break;
                case status_t::GONE:
                    r = "HTTP/1.1 410 Gone";
                    break;
                case status_t::LENGTH_REQUIRED:
                    r = "HTTP/1.1 411 Length Required";
                    break;
                case status_t::PRECONDITION_FAILED:
                    r = "HTTP/1.1 412 Precondition Failed";
                    break;
                case status_t::REQUEST_ENTITY_TOO_LARGE:
                    r = "HTTP/1.1 413 Request Entity Too Large";
                    break;
                case status_t::REQUEST_URI_TOO_LARGE:
                    r = "HTTP/1.1 414 Request-URI Too Large";
                    break;
                case status_t::UNSUPPORTED_MEDIA_TYPE:
                    r = "HTTP/1.1 415 Unsupported Media Type";
                    break;
                case status_t::REQUEST_RANGE_INVALID:
                    r = "HTTP/1.1 416 Requested range not satisfiable";
                    break;
                case status_t::EXPECTATION_FAILED:
                    r = "HTTP/1.1 417 Expectation Failed";
                    break;
                case status_t::INTERNAL_ERROR:
                    r = "HTTP/1.1 500 Internal Server Error";
                    break;
                case status_t::NOT_IMPLEMENTED:
                    r = "HTTP/1.1 501 Not Implemented";
                    break;
                case status_t::BAD_GATEWAY:
                    r = "HTTP/1.1 502 Bad Gateway";
                    break;
                case status_t::SERVICE_UNAVAILABLE:
                    r = "HTTP/1.1 503 Service Unavailable";
                    break;
                case status_t::GATEWAY_TIMEOUT:
                    r = "HTTP/1.1 504 Gateway Time-out";
                    break;
                case status_t::BAD_VERSION:
                    r = "HTTP/1.1 505 HTTP Version not supported";
                    break;
                default:
                    r = "";
                    break;
            }
            return (r);
        }

        static inline const char* method_name(method_t method)
        {
            switch(method)
            {
                case method_t::Delete:
                    return "DELETE";
                case method_t::Get:
                    return "GET";
                case method_t::Head:
                    return "HEAD";
                case method_t::Post:
                    return "POST";
                case method_t::Put:
                    return "PUT";
                case method_t::Connect:
                    return "CONNECT";
                case method_t::Options:
                    return "OPTIONS";
                case method_t::Trace:
                    return "TRACE";
                default:
                    return "Invalid";
            }
        }

        class query_string
        {
        public:
            static const int MAX_KEY_VALUE_PAIRS_COUNT = 256;

            query_string();

            query_string(strview_t& sv);

            query_string(query_string&& qs);

            query_string& operator = (query_string&& qs);



            void clear();

            friend std::ostream& operator<<(std::ostream& os, const query_string& qs)
            {
                os << "[ ";
                for(int i = 0; i < qs.nparams_; ++i) {
                    if (i)
                        os << ", ";
                    os << qs.params_[i];
                }
                os << " ]";
                return os;
            }

            strview_t get (const char* name) const;

            std::vector<char*> get_all (const char* name) const;

        private:
            int nparams_{0};
            char *url_{nullptr};
            char **params_{nullptr};
        };

        struct exception {

            exception(status_t status, const std::string& what)
                : status_(status),
                  what_(what.size())
            {
                what_ << what;
            }

            exception(status_t status, buffer_t& what)
                : status_(status),
                  what_(std::move(what))
            {}
            
            exception(status_t status, const char* what)
                : status_(status),
                  what_(0)
            {
                what_ << what;
            }

            exception(const exception&) = delete;
            exception&operator=(const exception&) = delete;

            exception(exception&& ex)
                : status_(ex.status_),
                  what_(std::move(ex.what_))
            {}

            exception&operator=(exception&& ex) {
                what_ = std::move(ex.what_);
                status_ = ex.status_;
                return *this;
            }

            status_t status() const {
                return status_;
            }
            
            const buffer_t& what() const {
                return what_;
            }

        private:
            status_t    status_;
            buffer_t    what_;
        };

        namespace error {
#define HTTP_ERROR(code, err)                               \
            inline exception&& err(const char *fmt, ...) {  \
                buffer_t what(0);                           \
                va_list args;                               \
                va_start(args, fmt);                        \
                what.appendv(fmt, args);                    \
                va_end(args);                               \
                return std::move(exception(code, what));    \
            }                                               \
            inline auto err() { return exception(code, ""); }

            HTTP_ERROR(status_t::BAD_REQUEST, bad_request)
            HTTP_ERROR(status_t::UNAUTHORIZED, unauthorized)
            HTTP_ERROR(status_t::FORBIDDEN, forbiden)
            HTTP_ERROR(status_t::NOT_FOUND, not_found)
            HTTP_ERROR(status_t::INTERNAL_ERROR, internal)
            HTTP_ERROR(status_t::NOT_IMPLEMENTED, not_implemented)

#undef  HTTP_ERROR
        }

        struct upfile_t : file_t {
            zcstring name;
            off_t   pos;
            off_t   offset;
            size_t  len;
        };

        typedef decltype(iod::D(
                prop(pid, uint32_t),
                prop(rx_bytes, uint64_t),
                prop(tx_bytes, uint64_t),
                prop(total_requests, uint64_t),
                prop(open_requests, uint64_t)
        )) server_stats_t;
    }

    static auto parse_cmd(int argc, const char *argv[]) {
        return iod::parse_command_line(argc, argv,
            iod::cl::description(
                   "Suil " SUIL_VERSION_STRING "\n",
                   opt(nworkers, "Number of parallel workers."),
                   opt(port, "The TCP number to listen on."),
                   opt(root, "Root directory for serving static files"),
                   opt(route, "The route prefix to use when serving static files")),
            opt(nworkers, int(0)),
            opt(port, int(1080)),
            opt(root, std::string("./www/")),
            opt(route, std::string("/www/"))
        );
    }
}

constexpr suil::http::method_t operator "" _method(const char* str, size_t /*len*/) {
    return
            suil::magic::is_equ_p(str, "GET", 3) ?     suil::http::method_t::Get :
            suil::magic::is_equ_p(str, "DELETE", 6) ?  suil::http::method_t::Delete :
            suil::magic::is_equ_p(str, "HEAD", 4) ?    suil::http::method_t::Head :
            suil::magic::is_equ_p(str, "POST", 4) ?    suil::http::method_t::Post :
            suil::magic::is_equ_p(str, "PUT", 3) ?     suil::http::method_t::Put :
            suil::magic::is_equ_p(str, "OPTIONS", 7) ? suil::http::method_t::Options :
            suil::magic::is_equ_p(str, "CONNECT", 7) ? suil::http::method_t::Connect :
            suil::magic::is_equ_p(str, "TRACE", 5) ?   suil::http::method_t::Trace :
            throw std::runtime_error("invalid http method");
}

#endif //SUIL_HTTP_HPP
