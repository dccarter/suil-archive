//
// Created by dc on 27/06/17.
//

#ifndef SUIL_HTTP_HPP
#define SUIL_HTTP_HPP

#include <ostream>

#include <suil/http/common.h>
#include <suil/net.h>

namespace suil {

    struct Cookie {

        Cookie(const char *name, const char *value)
            : name_(name? String(name).dup() : nullptr),
              value_(value? String(value).dup() : nullptr),
              path_(nullptr),
              domain_(nullptr),
              expires_(0)
        {}

        Cookie(const char *name)
            : Cookie(name, nullptr)
        {}

        Cookie(Cookie&& ck)
            : name_(std::move(ck.name_)),
              value_(std::move(ck.value_)),
              path_(std::move(ck.path_)),
              domain_(std::move(ck.domain_)),
              secure_(std::move(ck.secure_)),
              expires_(ck.expires_)
        {
            ck.expires_ = -1;
        }

        Cookie(const Cookie&) = delete;
        Cookie&operator=(const Cookie&) = delete;

        Cookie& operator=(Cookie&& ck) {
            name_   = std::move(ck.name_);
            value_  = std::move(ck.value_);
            path_   = std::move(ck.path_);
            domain_ = std::move(ck.domain_);
            secure_ = std::move(ck.secure_);
            expires_= ck.expires_;
            ck.expires_ = -1;
            return *this;
        }

        inline bool operator==(const Cookie& other) {
            return name_ == other.name_;
        }

        inline bool operator!=(const Cookie& other) {
            return name_ != other.name_;
        }

        inline operator bool() const {
            return !empty();
        }

        inline const String& name() const {
            return name_;
        }

        inline void value(String v) {
            if (v != value_) {
                value_ = std::move(v);
            }
        }

        inline void value(const char *v) {
            String zc(v);
            value(std::move(zc.dup()));
        }

        inline const String& value() const {
            return value_;
        }

        inline const String& path() const {
            return path_;
        }

        inline void path(String zc) {
            if (zc != path_) {
                path_ = std::move(zc);
            }
        }

        inline void path(const char *p) {
            String zc(p);
            path(std::move(zc.dup()));
        }

        inline const String& domain() const {
            return domain_;
        }

        inline void domain(String zc) {
            if (zc != domain_) {
                // this will destroy the current zc and copy the new one
                domain_ = std::move(zc);
            }
        }
        inline void domain(const char* d) {
            String zc(d);
            domain(std::move(zc).dup());
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

        ~Cookie() {
        }

    private:

        String    name_;
        String    value_;
        String    path_;
        String    domain_;
        bool        secure_{false};
        int64_t     maxage_{0};
        time_t      expires_{0};

        bool empty() const {
            return name_.empty();
        }
    };
    using cookie_jar_t = CaseMap<Cookie>;

    struct CookieIterator {
        typedef CaseMap<String> __jar_t;
        CookieIterator(const __jar_t& j)
            : jar(j)
        {}

        template<typename __F>
        void operator|(__F f) {
            for (const auto ck: jar) {
                f((const String&) ck.first, (const String&)ck.second);
            }
        }

        const String&& operator[](const String& key) {
            auto it = jar.find(key);
            if (it != jar.end()) {
                return std::move(it->second.peek());
            }
            return std::move(String(nullptr));
        }

        const String&& operator[](const char *name) {
            String key{name};
            return std::move((*this)[key]);
        }

    private:
        const __jar_t&  jar;
    };

    struct HttpConfig : public ServerConfig {
        int64_t         connection_timeout{5000};
        bool            disk_offload{false};
        size_t          disk_offload_min{2048};
        size_t          max_body_len{35648};
        size_t          send_chunk{35648000};
        uint64_t        keep_alive_time{3600};
        uint64_t        hsts_enable{3600};
        std::string     server_name{SUIL_SOFTWARE_NAME};
        std::string     offload_path{"./.body"};
    };

    namespace http {

        enum class Method : unsigned char {
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

        enum Status : int {
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
        status_text(Status status)
        {
            const char		*r;

            switch (status) {
                case Status::CONTINUE:
                    r = "HTTP/1.1 100 Continue";
                    break;
                case Status::SWITCHING_PROTOCOLS:
                    r = "HTTP/1.1 101 Switching Protocols";
                    break;
                case Status::OK:
                    r = "HTTP/1.1 200 OK";
                    break;
                case Status::CREATED:
                    r = "HTTP/1.1 201 Created";
                    break;
                case Status::ACCEPTED:
                    r = "HTTP/1.1 202 Accepted";
                    break;
                case Status::NON_AUTHORITATIVE:
                    r = "HTTP/1.1 203 Non-Authoritative Information";
                    break;
                case Status::NO_CONTENT:
                    r = "HTTP/1.1 204 No Content";
                    break;
                case Status::RESET_CONTENT:
                    r = "HTTP/1.1 205 Reset Content";
                    break;
                case Status::PARTIAL_CONTENT:
                    r = "HTTP/1.1 206 Partial Content";
                    break;
                case Status::MULTIPLE_CHOICES:
                    r = "HTTP/1.1 300 Multiple Choices";
                    break;
                case Status::MOVED_PERMANENTLY:
                    r = "HTTP/1.1 301 Moved Permanently";
                    break;
                case Status::FOUND:
                    r = "HTTP/1.1 302 Found";
                    break;
                case Status::SEE_OTHER:
                    r = "HTTP/1.1 303 See Other";
                    break;
                case Status::NOT_MODIFIED:
                    r = "HTTP/1.1 304 Not Modified";
                    break;
                case Status::USE_PROXY:
                    r = "HTTP/1.1 305 Use Proxy";
                    break;
                case Status::TEMPORARY_REDIRECT:
                    r = "HTTP/1.1 307 Temporary Redirect";
                    break;
                case Status::BAD_REQUEST:
                    r = "HTTP/1.1 400 Bad Request";
                    break;
                case Status::UNAUTHORIZED:
                    r = "HTTP/1.1 401 Unauthorized";
                    break;
                case Status::PAYMENT_REQUIRED:
                    r = "HTTP/1.1 402 Payment Required";
                    break;
                case Status::FORBIDDEN:
                    r = "HTTP/1.1 403 Forbidden";
                    break;
                case Status::NOT_FOUND:
                    r = "HTTP/1.1 404 Not Found";
                    break;
                case Status::METHOD_NOT_ALLOWED:
                    r = "HTTP/1.1 405 Method Not Allowed";
                    break;
                case Status::NOT_ACCEPTABLE:
                    r = "HTTP/1.1 406 Not Acceptable";
                    break;
                case Status::PROXY_AUTH_REQUIRED:
                    r = "HTTP/1.1 407 Proxy Authentication Required";
                    break;
                case Status::REQUEST_TIMEOUT:
                    r = "HTTP/1.1 408 Request Time-out";
                    break;
                case Status::CONFLICT:
                    r = "HTTP/1.1 409 Conflict";
                    break;
                case Status::GONE:
                    r = "HTTP/1.1 410 Gone";
                    break;
                case Status::LENGTH_REQUIRED:
                    r = "HTTP/1.1 411 Length Required";
                    break;
                case Status::PRECONDITION_FAILED:
                    r = "HTTP/1.1 412 Precondition Failed";
                    break;
                case Status::REQUEST_ENTITY_TOO_LARGE:
                    r = "HTTP/1.1 413 Request Entity Too Large";
                    break;
                case Status::REQUEST_URI_TOO_LARGE:
                    r = "HTTP/1.1 414 Request-URI Too Large";
                    break;
                case Status::UNSUPPORTED_MEDIA_TYPE:
                    r = "HTTP/1.1 415 Unsupported Media Type";
                    break;
                case Status::REQUEST_RANGE_INVALID:
                    r = "HTTP/1.1 416 Requested range not satisfiable";
                    break;
                case Status::EXPECTATION_FAILED:
                    r = "HTTP/1.1 417 Expectation Failed";
                    break;
                case Status::INTERNAL_ERROR:
                    r = "HTTP/1.1 500 Internal Server Error";
                    break;
                case Status::NOT_IMPLEMENTED:
                    r = "HTTP/1.1 501 Not Implemented";
                    break;
                case Status::BAD_GATEWAY:
                    r = "HTTP/1.1 502 Bad Gateway";
                    break;
                case Status::SERVICE_UNAVAILABLE:
                    r = "HTTP/1.1 503 Service Unavailable";
                    break;
                case Status::GATEWAY_TIMEOUT:
                    r = "HTTP/1.1 504 Gateway Time-out";
                    break;
                case Status::BAD_VERSION:
                    r = "HTTP/1.1 505 HTTP Version not supported";
                    break;
                default:
                    r = "HTTP/1.1 500  ";
                    break;
            }
            return (r);
        }

        static inline const char* method_name(Method method)
        {
            switch(method)
            {
                case Method::Delete:
                    return "DELETE";
                case Method::Get:
                    return "GET";
                case Method::Head:
                    return "HEAD";
                case Method::Post:
                    return "POST";
                case Method::Put:
                    return "PUT";
                case Method::Connect:
                    return "CONNECT";
                case Method::Options:
                    return "OPTIONS";
                case Method::Trace:
                    return "TRACE";
                default:
                    return "Invalid";
            }
        }

        static uint32_t method_from_string(const char *method) {
            switch (::toupper(method[0])) {
                case 'D':
                    return (uint32_t) Method::Delete;
                case 'G':
                    return (uint32_t) Method::Get;
                case 'H':
                    return (uint32_t) Method::Head;
                case 'P':
                    if (method[1] == 'O')
                        return (uint32_t) Method::Post;
                    else
                       return (uint32_t) Method::Put;
                case 'C':
                    return (uint32_t) Method::Connect;
                case 'O':
                    return (uint32_t) Method::Options;
                case 'T':
                    return (uint32_t) Method::Trace;
                default:
                    return (uint32_t) Method::Unknown;
            }
        }

        class QueryString
        {
        public:
            static const int MAX_KEY_VALUE_PAIRS_COUNT = 256;

            QueryString();

            QueryString(strview& sv);

            QueryString(QueryString&& qs);

            QueryString& operator = (QueryString&& qs);



            void clear();

            friend std::ostream& operator<<(std::ostream& os, const QueryString& qs)
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

            strview get (const char* name) const;

            std::vector<char*> get_all (const char* name) const;

            template <typename __T>
            __T get(const char* name, bool strict = false) const {
                __T tmp{};
                strview sv(get(name));
                if (!sv.empty()) {
                    String str(sv.data(), sv.size(), false);
                    utils::cast(str, tmp);
                }
                else if (strict) {
                    throw Exception::create("required query parameter '", name, "' missing");
                }

                return tmp;
            }

        private:
            int nparams_{0};
            char *url_{nullptr};
            char **params_{nullptr};
        };

        namespace Error {
#define HTTP_ERROR(code, err)                                                      \
            template <typename... Args>                                            \
            inline Exception err(Args... args) {                                   \
                if constexpr (sizeof...(args))                                     \
                return Exception::create((int) code, std::forward<Args>(args)...); \
                else return Exception::create(int (code), "");                     \
            }

            HTTP_ERROR(Status::BAD_REQUEST,     badRequest)
            HTTP_ERROR(Status::UNAUTHORIZED,    unauthorized)
            HTTP_ERROR(Status::FORBIDDEN,       forbiden)
            HTTP_ERROR(Status::NOT_FOUND,       notFound)
            HTTP_ERROR(Status::INTERNAL_ERROR,  internal)
            HTTP_ERROR(Status::NOT_IMPLEMENTED, notImplemented)

#undef  HTTP_ERROR
        }

        struct UploadedFile {

            void save(const char *dir, int64_t timeout = -1) const;

            inline const String& name() const {
                return name_;
            }

            inline const void* data() const {
                return data_;
            }

            inline size_t size() const {
                return len_;
            }

        private:
            friend struct Request;
            String name_{nullptr};
            void    *data_{nullptr};
            size_t  len_{0};
        };

        typedef decltype(iod::D(
            prop(pid, uint32_t),
            prop(rx_bytes, uint64_t),
            prop(tx_bytes, uint64_t),
            prop(total_requests, uint64_t),
            prop(open_requests, uint64_t)
        )) ServerStats;
    }

    static auto parse_cmd(int argc, const char *argv[]) {
        return iod::parse_command_line(argc, argv,
            iod::cl::description(
                   "Suil " SUIL_VERSION_STRING "\n",
                   opt(nworkers, "Number of parallel workers."),
                   opt(port,     "The TCP number to listen on."),
                   opt(root,     "Root directory for serving static files"),
                   opt(route,    "The route prefix to use when serving static files"),
                   opt(verbose,  "Verbosity level 0 (less severe), ... 4(more severe) | 5 (traces)")),
            opt(nworkers, int(0)),
            opt(port,     int(1080)),
            opt(root,     std::string("./www/")),
            opt(route, std::string("/www/")),
            opt(verbose, int(6))
        );
    }
}

constexpr suil::http::Method operator "" _method(const char* str, size_t /*len*/) {
    return
            suil::magic::is_equ_p(str, "GET", 3) ?     suil::http::Method::Get :
            suil::magic::is_equ_p(str, "DELETE", 6) ?  suil::http::Method::Delete :
            suil::magic::is_equ_p(str, "HEAD", 4) ?    suil::http::Method::Head :
            suil::magic::is_equ_p(str, "POST", 4) ?    suil::http::Method::Post :
            suil::magic::is_equ_p(str, "PUT", 3) ?     suil::http::Method::Put :
            suil::magic::is_equ_p(str, "OPTIONS", 7) ? suil::http::Method::Options :
            suil::magic::is_equ_p(str, "CONNECT", 7) ? suil::http::Method::Connect :
            suil::magic::is_equ_p(str, "TRACE", 5) ?   suil::http::Method::Trace :
            throw std::runtime_error("invalid http method");
}

#endif //SUIL_HTTP_HPP
