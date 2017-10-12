//
// Created by dc on 9/7/17.
//

#ifndef SUIL_CLIENT_HPP
#define SUIL_CLIENT_HPP

#include <deque>
#include <vector>
#include <fcntl.h>

#include <suil/http.hpp>
#include <suil/http/parser.hpp>

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
                struct File {
                    File(const char *name, const char *path, const char *ctype = nullptr)
                        : name(std::move(zcstring(name).dup())),
                          path(std::move(zcstring(path).dup())),
                          ctype(std::move(zcstring(ctype).dup()))
                    {}

                    File(const File&) = delete;
                    File& operator=(const File&) = delete;
                    File(File&& o)
                        : name(std::move(o.name)),
                          path(std::move(o.path)),
                          ctype(std::move(o.ctype)),
                          size(o.size)
                    { o.size = 0; }

                    File& operator=(File&& o) {
                        if (this != &o) {
                            name = std::move(o.name);
                            path = std::move(o.path);
                            ctype = std::move(o.ctype);
                            size = o.size;
                        }
                        return *this;
                    }

                    zcstring name;
                    zcstring path;
                    zcstring ctype;
                    size_t   size{0};
                };

                Form()
                {}

                template <typename... __F>
                Form(int type, __F... ff)
                    : encoding(type),
                      boundary(std::move(utils::catstr("-----", utils::randbytes(8))))
                {
                    append_args(ff...);
                }

                Form(Form&& o)
                    : files(std::move(o.files)),
                      encoding(o.encoding),
                      boundary(std::move(o.boundary)),
                      uploads(std::move(o.uploads)),
                      data(std::move(o.data))
                {}

                Form&operator=(Form&& o) {
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

                template <typename... __F>
                Form(__F... ff)
                    : Form(URL_ENCODED, ff...)
                {}

                operator bool() {
                    return  !data.empty() || !files.empty();
                }

                size_t encode(buffer_t& b) const;

                int encoding{URL_ENCODED};
                std::vector<File> files;

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
                    Upload(const File *f, zcstring&& head)
                        : file(f),
                          head(std::move(head))
                    {}

                    Upload(const Upload&) = delete;
                    Upload&operator=(const Upload&) = delete;

                    Upload(Upload&& u)
                        : fd(u.fd),
                          file(u.file),
                          head(std::move(u.head))
                    {}

                    Upload&operator=(Upload&& u) {
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

                    int        fd{-1};
                    const File *file;
                    zcstring head;
                };

                friend struct request;

                template <typename __V>
                void append_args(const char* name, __V& val) {
                    zcstring key = zcstring(name).dup();
                    zcstring value = utils::tozcstr(val);
                    if (encoding == URL_ENCODED) {
                        key = utils::urlencode(key);
                        value = utils::urlencode(value);
                    }
                    data.emplace(std::move(key), std::move(value));
                }

                void append_args() {
                }

                void append_args(File&& file) {
                    file.size = utils::fs::size(file.path.cstr);
                    files.push_back(std::move(file));
                }

                template <typename __V, typename... __A>
                void append_args(const char* key, __V& val, __A&... aa) {
                    append_args(key, val);
                    append_args(aa...);
                };

                template <typename... __A>
                void append_args(File& file, __A&... aa) {
                    append_args(std::move(file));
                    append_args(aa...);
                };

                zcstring boundary{};
                mutable std::vector<Upload>   uploads;
                mutable zcstr_map_t<zcstring> data;
            };
            using File = Form::File;
            using response_writer_t = std::function<size_t(const char*, size_t)>;

            struct response : protected http::parser {
                response()
                    : http::parser(http_parser_type::HTTP_RESPONSE)
                {}

                template <typename __R>
                bool read(__R& res) {
                    if (body.empty()) {
                        return false;
                    }

                    body >> res;
                }

                inline status_t status() const {
                    return (status_t) status_code;
                }

                zcstring redirect() const {
                    auto tmp = hdr("Location");
                    return zcstring(tmp.data(), tmp.size(), false);
                }

                strview_t hdr(const char *name) const {
                    zcstring tmp(name);
                    return hdr(tmp);
                }

                strview_t hdr(zcstring& name) const {
                    auto it = headers.find(name);
                    if (it != headers.end()){
                        return it->second;
                    }
                    return strview_t();
                }

                inline const buffer_t& operator()() const {
                    return body;
                }

                operator bool() {
                    return status() == status_t::OK;
                }

                const strview_t contenttype() const;

            private:

                friend struct session;

                virtual int handle_body_part(const char *at, size_t length) override;
                virtual int msg_complete() override ;
                void receive(sock_adaptor& sock, int64_t timeout);

                bool body_read{false};
                response_writer_t reader{nullptr};
            };

            struct request {
                request(const request&) = delete;
                request& operator=(const request&) = delete;
                inline void hdr(zcstring&& name, zcstring&& val) {
                    headers.insert(headers.end(),
                                   std::make_pair(std::move(name), std::move(val)));
                }
                template <typename __V>
                inline void hdrs(const char *name, __V val) {
                    zcstring key(zcstring(name).dup());
                    zcstring value(utils::tozcstr(val));
                    hdr(std::move(key), std::move(value));
                }

                template <typename __V, typename... __E>
                inline void hdrs(const char *name, __V val, __E... e) {
                    hdrs(name, val);
                    hdrs(e...);
                }

                template <typename __V>
                inline void args(const char *name, __V val) {
                    zcstring key(zcstring(name).dup());
                    zcstring tmp(utils::tozcstr(val));
                    arguments.emplace(key, utils::urlencode(tmp));
                }

                template <typename __V, typename... __E>
                inline void args(const char *name, __V val, __E... e) {
                    hdrs(name, val);
                    hdrs(e...);
                }

                request& operator<<(Form&& f);


                template <typename __Json>
                inline request& operator<<(__Json jobj) {
                    body << iod::json_encode(jobj);
                    hdrs("Content-Type", "application/json");
                    return *this;
                }

                buffer_t& buffer(const char* content_type = "text/plain") {
                    hdrs("Content-Type", content_type);
                    return body;
                }

                inline void keepalive(bool on) {
                    if (on)
                        hdrs("Connection", "Keep-Alive");
                    else
                        hdrs("Connection", "Close");
                }

                request(request&& o) noexcept;

                request& operator=(request&& o) noexcept;

                ~request() {
                    if (sock_ptr != nullptr) {
                        delete sock_ptr;
                        sock_ptr = nullptr;
                    }

                    cleanup();
                }

            private:
                friend struct session;

                void reset(method_t m, const char* res, bool clear = true) {
                    if (clear || m != method || resource != res) {
                        cleanup();
                        resource = zcstring(res).dup();
                        method = m;
                    }
                }

                void cleanup() {
                    arguments.clear();
                    form.clear();
                    body.clear();
                    headers.clear();
                }

                void encodeargs(buffer_t& dst) const;

                void encodehdrs(buffer_t& dst) const;

                size_t buildbody();

                void submit(int timeout = -1);

                request(sock_adaptor* adaptor)
                    : sock_ptr(adaptor),
                      sock(*sock_ptr)
                {}

                sock_adaptor           *sock_ptr;
                sock_adaptor&           sock;
                zcstr_map_t<zcstring>  headers{};
                zcstr_map_t<zcstring>  arguments{};
                method_t               method{method_t::Unknown};
                zcstring               resource{};
                Form                   form{};
                buffer_t               body{1024};
            };

            using request_builder_t = std::function<bool(request&)>;

            struct session : LOGGER(dtag(HTTP_CLIENT)) {
                struct handle_t {
                    handle_t(session& sess, sock_adaptor* sock)
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

                    session& sess;
                private:
                    friend struct session;
                    request  req;
                };

                inline void header(zcstring&& name, zcstring&& value) {
                    headers.emplace(std::move(name), std::move(value));
                }

                inline void header(const char* name,const char* value) {
                    header(zcstring(name).dup(), zcstring(value).dup());
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

                inline session::handle_t handle() {
                    sock_adaptor *sock = nullptr;
                    if (ishttps()) {
                        sock = new ssl_sock;
                    }
                    else {
                        sock = new tcp_sock;
                    }

                    return session::handle_t{*this, sock};
                }

                handle_t connect(zcstr_map_t<zcstring> hdrs = {}) {
                    handle_t h = handle();
                    connect(h, hdrs);
                    return std::move(h);
                }

                void connect(handle_t& h, zcstr_map_t<zcstring> hdrs = {});

                response head(handle_t& h, const char* resource, zcstr_map_t<zcstring> hdr = {});

            private:
                template <typename... __O>
                friend session  load(const char *, int port, const char *, __O...);
                friend response perform(method_t, handle_t& h, const char *, request_builder_t,response_writer_t);

                session(zcstring&& proto, zcstring&& host, int port = 80)
                    : port(port),
                      host(std::move(host)),
                      protocol(std::move(proto))
                {}

                template <typename... __O>
                void configure(const char* path, __O&... opts) {
                    /* configure session */
                    // FIXME: zcstring sess(utils::fs::readall(path, true));
                    addr = ipremote(host.cstr, port, 0, utils::after(timeout));
                    if (errno != 0) {
                        throw suil_error::create("getting address '", host.cstr,
                                                 ":", port, "' failed:", errno_s);
                    }

                    header("Host", host.dup());
                    useragent(SUIL_HTTP_USER_AGENT);
                    language("en-US");
                }

                inline bool ishttps() const {
                    return protocol == "https";
                }

                response perform(handle_t& h, method_t m, const char *url, request_builder_t& builder, response_writer_t& rd);
                inline response perform(handle_t& h, method_t m, const char *url = "") {
                    request_builder_t rb{nullptr};
                    response_writer_t rw{nullptr};
                    return std::move(perform(h, m, url, rb, rw));
                }

                int       port{80};
                zcstring  host;
                zcstr_map_t<zcstring> headers{};
                int64_t   timeout{20000};
                ipaddr    addr{};
                zcstring  protocol{"http"};
            };

            inline client::response perform(method_t m, session::handle_t& h, const char *u, request_builder_t b,
                                            response_writer_t rd = nullptr) {
                auto resp = h.sess.perform(h, m, u, b, rd);
                if (resp.status() == status_t::TEMPORARY_REDIRECT) {
                }

                return std::move(resp);
            }

#undef CRLF

            struct response_offload: file_t {
                explicit response_offload(const char *path, int64_t timeout = -1)
                        : file_t(path, O_WRONLY|O_CREAT, 0644),
                          timeout(timeout)
                {
                    writer = [&](const char *data, size_t len) {
                        if (data == nullptr) {
                            /* done receving */
                            flush(timeout);
                        }

                        size_t nwr = write(data, len, timeout);
                        if (nwr != len) {
                            throw suil_error::create("writing to file failed: ",
                                                     errno_s);
                        }
                        offset += nwr;
                        return nwr;
                    };
                }

                ~response_offload() override = default;

                operator response_writer_t&() {
                    return writer;
                }

                response_writer_t  writer{nullptr};

            private:
                off_t       offset{0};
                int64_t     timeout{-1};
            };

            /**
             * @brief loads an http client session from the given path
             * @param host the host that the session connects to
             * @param port the port to connect to on the host
             * @param path the local path where the session is saved
             * @param opts options that will be passed to the session
             *
             * @returns the loaded session
             * */
            template <typename... __O>
            session load(const char *url, int port, const char *path, __O... opts) {
                const char *tmp(strstr(url, "://"));
                zcstring proto{}, host{};
                if (tmp != nullptr) {
                    /* protocol is part of the URL */
                    proto = zcstring(url, tmp-url, 0).dup();
                    host  = zcstring((tmp+3)).dup();
                }
                else {
                    /* use default protocol */
                    proto = zcstring("http").dup();
                    host  = zcstring(url).dup();
                }

                session sess(std::move(proto), std::move(host), port);
                sess.configure(path, opts...);
                return sess;
            }

            inline response get(session::handle_t& h, const char *resource, request_builder_t builder = nullptr ) {
                return client::perform(method_t::Get, h, resource, builder);
            }

            inline response get(session& sess, const char *resource, request_builder_t builder = nullptr ) {
                auto h = sess.handle();
                return client::perform(method_t::Get, h, resource, builder);
            }

            inline response get(response_offload& wr, session::handle_t& h, const char *resource, request_builder_t builder = nullptr ) {
                return client::perform(method_t::Get, h, resource, builder, wr.writer);
            }

            inline response get(response_offload& wr,session& sess, const char *resource, request_builder_t builder = nullptr ) {
                auto h = sess.handle();
                return client::perform(method_t::Get, h, resource, builder, wr.writer);
            }

            inline response post(session::handle_t& h, const char *resource, request_builder_t builder = nullptr) {
                return client::perform(method_t::Post, h, resource, builder);
            }
            inline response post(session& sess, const char *resource, request_builder_t builder = nullptr) {
                auto h = sess.handle();
                return client::perform(method_t::Post, h, resource, builder);
            }

            inline response put(session::handle_t& h, const char *resource, request_builder_t builder = nullptr) {
                return client::perform(method_t::Put, h, resource, builder);
            }
            inline response put(session& sess, const char *resource, request_builder_t builder = nullptr) {
                auto h = sess.handle();
                return client::perform(method_t::Put, h, resource, builder);
            }

            inline response del(session::handle_t& h, const char *resource, request_builder_t builder = nullptr) {
                return client::perform(method_t::Delete, h, resource, builder);
            }
            inline response del(session& sess, const char *resource, request_builder_t builder = nullptr) {
                auto h = sess.handle();
                return client::perform(method_t::Delete, h, resource, builder);
            }

            inline response options(session::handle_t& h, const char *resource, request_builder_t builder = nullptr) {
                return client::perform(method_t::Options, h, resource, builder);
            }
            inline response options(session& sess, const char *resource, request_builder_t builder = nullptr) {
                auto h = sess.handle();
                return client::perform(method_t::Options, h, resource, builder);
            }

            inline response head(session::handle_t& h, const char *resource, request_builder_t builder = nullptr) {
                return client::perform(method_t::Head, h, resource, builder);
            }
            inline response head(session& sess, const char *resource, request_builder_t builder = nullptr) {
                auto h = sess.handle();
                return client::perform(method_t::Head, h, resource, builder);
            }
        }
    }
}

#endif //SUIL_CLIENT_HPP
