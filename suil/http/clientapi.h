//
// Created by dc on 9/7/17.
//

#ifndef SUIL_CLIENT_HPP
#define SUIL_CLIENT_HPP

#include <deque>
#include <vector>
#include <fcntl.h>

#include <suil/file.h>
#include <suil/http.h>
#include <suil/http/parser.h>

#ifndef SUIL_HTTP_USER_AGENT
#define SUIL_HTTP_USER_AGENT SUIL_SOFTWARE_NAME "/" SUIL_VERSION_STRING
#endif

namespace suil {

    namespace http {
        define_log_tag(HTTP_CLIENT);

        namespace client {

#define CRLF "\r\n"
            struct Form {
                static constexpr int URL_ENCODED      = 0;
                static constexpr int MULTIPART_FORM   = 1;
                static constexpr int MULTIPART_OTHERS = 2;
                struct file_t {
                    file_t(const char *name, const char *path, const char *ctype = nullptr)
                        : name(std::move(String(name).dup())),
                          path(std::move(String(path).dup())),
                          ctype(std::move(String(ctype).dup()))
                    {}

                    file_t(const file_t&) = delete;
                    file_t& operator=(const file_t&) = delete;
                    file_t(file_t&& o) noexcept
                        : name(std::move(o.name)),
                          path(std::move(o.path)),
                          ctype(std::move(o.ctype)),
                          size(o.size)
                    { o.size = 0; }

                    file_t& operator=(file_t&& o) noexcept {
                        if (this != &o) {
                            name = std::move(o.name);
                            path = std::move(o.path);
                            ctype = std::move(o.ctype);
                            size = o.size;
                        }
                        return *this;
                    }

                    String name;
                    String path;
                    String ctype;
                    size_t   size{0};
                };

                Form() = default;

                template <typename... F>
                Form(int type, F... ff)
                    : encoding(type),
                      boundary(std::move(utils::catstr("-----", utils::randbytes(8))))
                {
                    append_args(ff...);
                }

                Form(Form&& o) noexcept
                    : files(std::move(o.files)),
                      encoding(o.encoding),
                      boundary(std::move(o.boundary)),
                      uploads(std::move(o.uploads)),
                      data(std::move(o.data))
                {}

                Form&operator=(Form&& o) noexcept {
                    if (this != &o) {
                        files = std::move(o.files);
                        encoding = o.encoding;
                        boundary = std::move(o.boundary),
                        uploads = std::move(o.uploads),
                        data = std::move(o.data);
                    }
                    return *this;
                }

                Form(const Form&) = delete;
                Form&operator=(const Form&) = delete;

                template <typename... F>
                Form(F... ff)
                    : Form(URL_ENCODED, ff...)
                {}

                operator bool() {
                    return  !data.empty() || !files.empty();
                }

                size_t encode(OBuffer& b) const;

                int encoding{URL_ENCODED};
                std::vector<file_t> files;

                ~Form() {
                    clear();
                }

                void clear() {
                    data.clear();
                    uploads.clear();
                    files.clear();
                }

            private:
                struct Upload {
                    Upload(const file_t *f, String&& head)
                        : file(f),
                          head(std::move(head))
                    {}

                    Upload(const Upload&) = delete;
                    Upload&operator=(const Upload&) = delete;

                    Upload(Upload&& u) noexcept
                        : fd(u.fd),
                          file(u.file),
                          head(std::move(u.head))
                    {}

                    Upload&operator=(Upload&& u) noexcept {
                        if (this != &u) {
                            fd = u.fd;
                            file = u.file;
                            head = std::move(head);
                        }
                        return *this;
                    }


                    int open();

                    void close();

                    ~Upload() {

                        if (file) {
                            file = nullptr;
                        }

                        close();
                    }

                    int          fd{-1};
                    const file_t *file;
                    String head;
                };

                friend struct Request;

                template <typename V>
                void append_args(const char* name, V& val) {
                    String key = String(name).dup();
                    String value = utils::tostr(val);
                    if (encoding == URL_ENCODED) {
                        key = utils::urlencode(key);
                        value = utils::urlencode(value);
                    }
                    data.emplace(std::move(key), std::move(value));
                }

                void append_args() {
                }

                void append_args(file_t&& file) {
                    file.size = utils::fs::size(file.path.data());
                    files.push_back(std::move(file));
                }

                template <typename V, typename... A>
                void append_args(const char* key, V& val, A&... aa) {
                    append_args(key, val);
                    append_args(aa...);
                };

                template <typename... A>
                void append_args(file_t& file, A&... aa) {
                    append_args(std::move(file));
                    append_args(aa...);
                };

                String boundary{};
                mutable std::vector<Upload>   uploads;
                mutable Map<String> data;
            };
            using UpFile = Form::file_t;
            using ResponseWriter = std::function<size_t(const char*, size_t)>;

            struct Response : protected http::parser {
                Response()
                    : http::parser(http_parser_type::HTTP_RESPONSE)
                {}

                template <typename __R>
                bool read(__R& res) {
                    if (body.empty()) {
                        return false;
                    }

                    body >> res;
                }

                inline Status status() const {
                    return (Status) status_code;
                }

                String redirect() const {
                    auto tmp = hdr("Location");
                    return String(tmp.data(), tmp.size(), false);
                }

                strview hdr(const char *name) const {
                    String tmp(name);
                    return hdr(tmp);
                }

                strview hdr(String& name) const {
                    auto it = headers.find(name);
                    if (it != headers.end()){
                        return it->second;
                    }
                    return strview();
                }

                inline const OBuffer& operator()() const {
                    return body;
                }

                operator bool() {
                    return status() == Status::OK;
                }

                const strview contenttype() const;

                String getbody() {
                    return String{Ego.body};
                }

            private:

                friend struct Session;

                int handle_body_part(const char *at, size_t length) override;

                int msg_complete() override ;

                int handle_headers_complete() override;

                void receive(SocketAdaptor& sock, int64_t timeout);

                bool body_read{false};
                ResponseWriter reader{nullptr};
            };

            struct Request : LOGGER(HTTP_CLIENT) {

                Request(const Request&) = delete;
                Request& operator=(const Request&) = delete;
                inline void hdr(String&& name, String&& val) {
                    headers.insert(headers.end(),
                                   std::make_pair(std::move(name), std::move(val)));
                }

                template <typename V>
                inline void hdrs(const char *name, V val) {
                    String key(String(name).dup());
                    String value(utils::tostr(val));
                    hdr(std::move(key), std::move(value));
                }

                template <typename V, typename... E>
                inline void hdrs(const char *name, V val, E... e) {
                    hdrs(name, val);
                    hdrs(e...);
                }

                template <typename V>
                inline void args(const char *name, V val) {
                    String key(String(name).dup());
                    String tmp(utils::tostr(val));
                    arguments.emplace(std::move(key),
                                      std::move(utils::urlencode(tmp)));
                }

                template <typename V, typename... E>
                inline void args(const char *name, V val, E... e) {
                    args(name, val);
                    args(e...);
                }

                Request& operator<<(Form&& f);
                Request& operator<<(File&& f);

                template <typename J>
                inline Request& operator<<(J jobj) {
                    body << iod::json_encode(jobj);
                    hdrs("Content-Type", "application/json");
                    return *this;
                }

                OBuffer& buffer(const char* content_type = "text/plain");

                inline void keepalive(bool on) {
                    if (on)
                        hdrs("Connection", "Keep-Alive");
                    else
                        hdrs("Connection", "Close");
                }

                Request(Request&& o) noexcept;

                Request& operator=(Request&& o) noexcept;

                ~Request() override {
                    if (sock_ptr != nullptr) {
                        delete sock_ptr;
                        sock_ptr = nullptr;
                    }

                    cleanup();
                }

            private:
                friend struct Session;

                void reset(Method m, const char* res, bool clear = true) {
                    if (clear || m != method || resource != res) {
                        cleanup();
                        resource = String(res).dup();
                        method = m;
                    }
                }

                void cleanup() {
                    arguments.clear();
                    form.clear();
                    body.clear();
                    headers.clear();
                    bodyFd.close();
                }

                void encodeargs(OBuffer& dst) const;

                void encodehdrs(OBuffer& dst) const;

                size_t buildbody();

                void submit(int64_t timeout = -1);

                explicit Request(SocketAdaptor* adaptor)
                    : sock_ptr(adaptor),
                      sock(*sock_ptr)
                {}

                SocketAdaptor           *sock_ptr;
                SocketAdaptor&           sock;
                CaseMap<String>      headers{};
                Map<String>          arguments{};
                Method               method{Method::Unknown};
                String               resource{};
                Form                   form{};
                OBuffer               body{1024};
                File                  bodyFd{nullptr};
            };

            using request_builder_t = std::function<bool(Request&)>;

            struct Session : LOGGER(HTTP_CLIENT) {
                struct handle_t {
                    handle_t(Session& sess, SocketAdaptor* sock)
                        : sess(sess),
                          req(sock)
                    {}

                    handle_t(handle_t&& o) noexcept
                        : sess(o.sess),
                          req(std::move(o.req))
                    {}

                    handle_t& operator=(handle_t&& o) noexcept {
                        sess = o.sess;
                        req  = std::move(o.req);
                    }

                    handle_t(const handle_t&) = delete;
                    handle_t&operator=(const handle_t&) = delete;

                    Session& sess;
                private:
                    friend struct Session;
                    Request  req;
                };

                inline void header(String&& name, String&& value) {
                    headers.emplace(std::move(name), std::move(value));
                }

                inline void header(const char* name,const char* value) {
                    header(String(name).dup(), String(value).dup());
                }

                inline void language(const char *lang) {
                    header("Accept-Language", lang);
                }

                inline void useragent(const char *agent) {
                    header("User-Agent", agent);
                }

                inline void keepalive(bool on = true) {
                    if (on)
                        header("Connection", "Keep-Alive");
                    else
                        header("Connection", "Close");
                }

                inline Session::handle_t handle() {
                    SocketAdaptor *sock = nullptr;
                    if (ishttps()) {
                        sock = new SslSock;
                    }
                    else {
                        sock = new TcpSock;
                    }

                    return Session::handle_t{*this, sock};
                }

                handle_t connect(CaseMap<String> hdrs = {}) {
                    handle_t h = handle();
                    connect(h, hdrs);
                    return std::move(h);
                }

                void connect(handle_t& h, CaseMap<String> hdrs = {});

                Response head(handle_t& h, const char* resource, CaseMap<String> hdr = {});

                Session()
                    : Session(String("http").dup(), String("127.0.0.1").dup())
                {}

            private:
                template <typename... __O>
                friend Session  load(const char *, int port, const char *, __O...);
                friend Response perform(Method, handle_t& h, const char *, request_builder_t,ResponseWriter);

                Session(String&& proto, String&& host, int port = 80)
                    : port(port),
                      host(std::move(host)),
                      protocol(std::move(proto))
                {}

                template <typename... __O>
                void configure(const char* path, __O&... opts) {
                    /* configure Session */
                    // FIXME: String sess(utils::fs::readall(path, true));
                    addr = ipremote(host.data(), port, 0, utils::after(timeout));
                    if (errno != 0) {
                        throw Exception::create("getting address '", host(),
                                                 ":", port, "' failed:", errno_s);
                    }

                    header("Host", host.dup());
                    useragent(SUIL_HTTP_USER_AGENT);
                    language("en-US");
                }

                inline bool ishttps() const {
                    return protocol == "https";
                }

                Response perform(handle_t& h, Method m, const char *url, request_builder_t& builder, ResponseWriter& rd);
                inline Response perform(handle_t& h, Method m, const char *url = "") {
                    request_builder_t rb{nullptr};
                    ResponseWriter rw{nullptr};
                    return std::move(perform(h, m, url, rb, rw));
                }

                int       port{80};
                String  host;
                CaseMap<String> headers{};
                int64_t   timeout{20000};
                ipaddr    addr{};
                String  protocol{"http"};
            };

            inline client::Response perform(Method m, Session::handle_t& h, const char *u, request_builder_t b,
                                            ResponseWriter rd = nullptr) {
                auto resp = h.sess.perform(h, m, u, b, rd);
                if (resp.status() == Status::TEMPORARY_REDIRECT) {
                }

                return std::move(resp);
            }

#undef CRLF

            struct FileOffload: File {
                explicit FileOffload(const char *path, int64_t timeout = -1)
                        : File(path, O_WRONLY|O_CREAT, 0644),
                          timeout(timeout)
                {
                    handler = [&](const char *data, size_t len) {
                        if (len == 0) {
                            /* done receving */
                            flush(timeout);
                        }
                        else if (data != nullptr){
                            size_t nwr = write(data, len, timeout);
                            if (nwr != len) {
                                throw Exception::create("writing to file failed: ",
                                                         errno_s);
                            }
                            offset += nwr;
                            return nwr;
                        }
                    };
                }

                ~FileOffload() override = default;

                ResponseWriter& operator()() {
                    return handler;
                }

            private:
                off_t       offset{0};
                int64_t     timeout{-1};
                ResponseWriter  handler{nullptr};
            };

            struct MemoryOffload {
                MemoryOffload(size_t mapped_min = 65000)
                    : mapped_min(mapped_min)
                {
                    handler = [&](const char *at, size_t len) {
                        if (at == NULL && len) {
                            if (len > mapped_min) {
                                /* used mapped memory */
                                return map_region(len);
                            }
                            else {
                                /* allocate memory from heap */
                                data = (char *) malloc(len);
                                return data != nullptr;
                            }
                        }
                        else {
                            /* received data portion */
                            memcpy(&data[offset], at, len);
                            offset += len;
                            return true;
                        }
                    };
                }

                ~MemoryOffload();

                operator String() {
                    return String(data, offset, false);
                }

                ResponseWriter& operator()() {
                    return handler;
                }

            private:
                bool    map_region(size_t len);
                char    *data{nullptr};
                size_t  offset{0};
                size_t  mapped_min{65350};
                bool    is_mapped{false};
                ResponseWriter handler{nullptr};
            };

            /**
             * @brief loads an http client Session from the given path
             * @param host the host that the Session connects to
             * @param port the port to connect to on the host
             * @param path the local path where the Session is saved
             * @param opts options that will be passed to the Session
             *
             * @returns the loaded Session
             * */
            template <typename... __O>
            Session load(const char *url, int port, const char *path, __O... opts) {
                const char *tmp(strstr(url, "://"));
                String proto{}, host{};
                if (tmp != nullptr) {
                    /* protocol is part of the URL */
                    proto = String(url, tmp-url, 0).dup();
                    host  = String((tmp+3)).dup();
                }
                else {
                    /* use default protocol */
                    proto = String("http").dup();
                    host  = String(url).dup();
                }

                Session sess(std::move(proto), std::move(host), port);
                sess.configure(path, opts...);
                return sess;
            }

            inline Session load(const char *url, int port = 80) {
                return load(url, port, nullptr);
            }

            inline Response get(Session::handle_t& h, const char *resource, request_builder_t builder = nullptr ) {
                return client::perform(Method::Get, h, resource, builder);
            }

            inline Response get(Session& sess, const char *resource, request_builder_t builder = nullptr ) {
                auto h = sess.handle();
                return client::perform(Method::Get, h, resource, builder);
            }

            template <typename __T>
            inline Response get(__T& off, Session::handle_t& h, const char *resource, request_builder_t builder = nullptr ) {
                return client::perform(Method::Get, h, resource, builder, off());
            }

            template <typename __T>
            inline Response get(__T& off,Session& sess, const char *resource, request_builder_t builder = nullptr ) {
                auto h = sess.handle();
                return client::perform(Method::Get, h, resource, builder, off());
            }

            inline Response post(Session::handle_t& h, const char *resource, request_builder_t builder = nullptr) {
                return client::perform(Method::Post, h, resource, builder);
            }

            inline Response post(Session& sess, const char *resource, request_builder_t builder = nullptr) {
                auto h = sess.handle();
                return client::perform(Method::Post, h, resource, builder);
            }

            inline Response put(Session::handle_t& h, const char *resource, request_builder_t builder = nullptr) {
                return client::perform(Method::Put, h, resource, builder);
            }

            inline Response put(Session& sess, const char *resource, request_builder_t builder = nullptr) {
                auto h = sess.handle();
                return client::perform(Method::Put, h, resource, builder);
            }

            inline Response del(Session::handle_t& h, const char *resource, request_builder_t builder = nullptr) {
                return client::perform(Method::Delete, h, resource, builder);
            }
            inline Response del(Session& sess, const char *resource, request_builder_t builder = nullptr) {
                auto h = sess.handle();
                return client::perform(Method::Delete, h, resource, builder);
            }

            inline Response options(Session::handle_t& h, const char *resource, request_builder_t builder = nullptr) {
                return client::perform(Method::Options, h, resource, builder);
            }

            inline Response options(Session& sess, const char *resource, request_builder_t builder = nullptr) {
                auto h = sess.handle();
                return client::perform(Method::Options, h, resource, builder);
            }

            inline Response head(Session::handle_t& h, const char *resource, request_builder_t builder = nullptr) {
                return client::perform(Method::Head, h, resource, builder);
            }

            inline Response head(Session& sess, const char *resource, request_builder_t builder = nullptr) {
                auto h = sess.handle();
                return client::perform(Method::Head, h, resource, builder);
            }
        }
    }
}

#endif //SUIL_CLIENT_HPP
