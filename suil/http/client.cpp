//
// Created by dc on 9/7/17.
//
#include <fcntl.h>
#include <sys/param.h>

#include <suil/http/clientapi.hpp>
#include <sys/mman.h>

namespace suil {
    namespace http {
        namespace client {

#undef  CRLF
#define CRLF "\r\n"

            size_t Form::encode(zbuffer &out) const {
                if (data.empty() && files.empty()) {
                    strace("trying to encode an empty form");
                    return 0;
                }

                if (encoding == URL_ENCODED) {
                    bool first{true};
                    for(auto& pair : data) {
                        if (!first) {
                            out << "&";
                        }
                        first = false;
                        out << pair.first << "=" << pair.second;
                    }

                    return out.size();
                } else {
                    /* multi-part encoded form */
                    for (auto& fd: data) {
                        out << "--" << boundary << CRLF;
                        out << "Content-Disposition: form-data; name=\""
                            << fd.first << "\"" << CRLF;
                        out << CRLF << fd.second << CRLF;
                    }

                    if (files.empty()) {
                        out << "--" << boundary << "--";
                        return out.size();
                    }

                    size_t  content_length{out.size()};
                    /* encode file into upload buffers */
                    for (auto& ff: files) {
                        zbuffer tmp(127);
                        tmp << "--" << boundary << CRLF;
                        tmp << "Content-Disposition: form-data; name=\""
                            << ff.name << "\"; filename=\"" << basename((char *)ff.path.data())
                            << "\"" << CRLF;
                        if (ff.ctype) {
                            /* append content type header */
                            tmp << "Content-Type: " << ff.ctype << CRLF;
                        }
                        tmp << CRLF;
                        content_length += tmp.size() + ff.size;

                        Upload upload(&ff, std::move(zcstring(tmp)));
                        uploads.push_back(std::move(upload));
                        /* \r\n will added at the end of each upload */
                        content_length += sizeofcstr(CRLF);
                    }

                    /* --boundary--  will be appended */
                    content_length += sizeofcstr("----") + boundary.size();
                    return content_length;
                }
            }

            int Form::Upload::open() {
                if (fd > 0) {
                    return fd;
                }
                fd = ::open(file->path(), O_RDWR);
                if (fd < 0) {
                    /* opening file failed */
                    throw SuilError::create("open '", file->path(),
                                             "' failed: ", errno_s);
                }

                return fd;
            }

            void Form::Upload::close() {
                if (fd > 0) {
                    ::close(fd);
                    fd = -1;
                }
            }

            void Response::receive(SocketAdaptor &sock, int64_t timeout) {
                zbuffer tmp(1023);
                do {
                    size_t nrd = tmp.capacity();
                    if (!sock.read(&tmp[0], nrd, timeout)) {
                        /* failed to receive headers*/
                        throw SuilError::create("receiving Request failed: ", errno_s);
                    }

                    if (!feed(tmp.data(), nrd)) {
                        throw SuilError::create("parsing headers failed: ",
                              http_errno_name((enum http_errno) http_errno));
                    }
                } while (!headers_complete);

                if (body_read || content_length == 0) {
                    strace("%s - Response has no body to read: %lu", sock.id(), content_length);
                    return;
                }

                /* received and parse body */
                size_t len  = 0, left = content_length;
                // read body in chunks
                tmp.reserve(MIN(content_length, 8900));

                do {
                    len = MIN(tmp.capacity(), left);
                    tmp.reset(len, true);
                    if (!sock.receive(&tmp[0], len, timeout)) {
                        throw SuilError::create("receive failed: ", errno_s);
                    }

                    tmp.seek(len);
                    // parse header line
                    if (!feed(tmp, len)) {
                        throw SuilError::create("parsing  body failed: ",
                                     http_errno_name((enum http_errno )http_errno));
                    }
                    left -= len;
                    // no need to reset buffer
                } while (!body_complete && left > 0);
            }

            int Response::handle_body_part(const char *at, size_t length) {
                if (reader == nullptr) {
                    return parser::handle_body_part(at, length);
                }
                else {
                    if (reader(at, length) == length) {
                        return 0;
                    }
                    return -1;
                }
            }

            int Response::handle_headers_complete() {
                /* if reader is configured, */
                if (reader != NULL) {
                    reader(NULL, content_length);
                }
                else {
                    body.reserve(content_length + 2);
                }
                return 0;
            }

            bool MemoryOffload::map_region(size_t len) {
                size_t total = len;
                int page_sz = getpagesize();
                total += page_sz-(len % page_sz);
                data = (char *) mmap(NULL, total, PROT_READ, MAP_SHARED , -1, 0);
                if (data == nullptr) {
                    swarn("client::MemoryOffload mmap failed: %s", errno_s);
                    return false;
                }
                is_mapped = true;
                return true;
            }

            MemoryOffload::~MemoryOffload() {
                if (data) {
                    if (is_mapped) {
                        /* unmap mapped memory */
                        munmap(data, offset);
                    }
                    else {
                        /* free allocated memory */
                        free(data);
                    }
                    data = nullptr;
                }
            }

            int Response::msg_complete() {
                if (reader) {
                    reader(nullptr, 0);
                }
                body_read = true;
            }

            const strview Response::contenttype() const {
                auto it = headers.find("Content-Type");
                if (it != headers.end()) {
                    return it->second;
                }

                return strview();
            }

            Request& Request::operator=(Request &&o) noexcept {
                if (this != &o) {
                    sock_ptr = o.sock_ptr;
                    sock = *sock_ptr;
                    headers = std::move(o.headers);
                    arguments = std::move(o.arguments);
                    method = o.method;
                    resource = std::move(o.resource);
                    form = std::move(o.form);
                    body = std::move(o.body);

                    o.sock_ptr = nullptr;
                    o.method = Method::Unknown;
                    o.cleanup();
                }
                return *this;
            }

            Request::Request(Request &&o) noexcept
                : sock_ptr(o.sock_ptr),
                  sock(*sock_ptr),
                  headers(std::move(o.headers)),
                  arguments(std::move(o.arguments)),
                  method(o.method),
                  resource(std::move(o.resource)),
                  form(std::move(o.form)),
                  body(std::move(o.body))
            {
                o.sock_ptr = nullptr;
                o.method = Method::Unknown;
            }

            Request& Request::operator<<(Form &&f) {
                form = std::move(f);
                zcstring key("Content-Type");
                zcstring val;
                if (form.encoding == Form::URL_ENCODED) {
                    val = zcstring("application/x-www-form-urlencoded").dup();
                }
                else {
                    val = utils::catstr("multipart/form-data; boundary=", form.boundary);
                }
                headers.emplace(key.dup(), std::move(val));

                return *this;
            }

            void Request::encodeargs(zbuffer &dst) const {
                if (!arguments.empty()) {
                    dst << "?";
                    bool first{true};
                    for(auto& arg: arguments) {
                        if (!first) {
                            dst << "&";
                        }
                        first = false;
                        dst << arg.first << "=" << arg.second;
                    }
                }
            }

            void Request::encodehdrs(zbuffer &dst) const {
                if (!headers.empty()) {
                    for (auto& hdr: headers) {
                        dst << hdr.first << ": " << hdr.second << CRLF;
                    }
                }
            }

            size_t Request::buildbody() {
                size_t content_length{0};
                if (utils::matchany(method, Method::Put, Method::Post)) {
                    if (form) {
                        /*encode form if available */
                        content_length = form.encode(body);
                    }
                    else {
                        content_length = body.size();
                    }
                }

                return content_length;
            }

            void Request::submit(int timeout) {
                zbuffer head(1023);
                size_t  content_length{buildbody()};
                hdrs("Content-Length", content_length);

                head << method_name(method) << " " << resource;
                encodeargs(head);
                head << " HTTP/1.1" << CRLF;
                hdrs("Date", Datetime()(Datetime::HTTP_FMT));
                encodehdrs(head);
                head << CRLF;

                size_t nwr = sock.send(head.data(), head.size(), timeout);
                if (nwr != head.size()) {
                    /* sending headers failed, no need to send body */
                    throw SuilError::create("send Request failed: (", nwr, ",",
                             head.size(), ")", errno_s);
                }
                if (content_length == 0) {
                    sock.flush();
                    return;
                }

                if (!body.empty()) {
                    /* send body */
                    nwr = sock.send(body.data(), body.size(), timeout);
                    if (nwr != body.size()) {
                        /* sending headers failed, no need to send body */
                        throw SuilError::create("send Request failed: ", errno_s);
                    }
                }

                if (!form.uploads.empty()) {
                    /* send upload files one after the other */
                    for (auto &up: form.uploads) {
                        int fd = up.open();
                        nwr = sock.send(up.head(), up.head.size(), timeout);
                        if (nwr != up.head.size()) {
                            /* sending upload head failed, no need to send body */
                            throw SuilError::create("send Request failed: ", errno_s);
                        }

                        /* send file */
                        nwr = sock.sendfile(fd, 0, up.file->size, timeout);
                        if (nwr != up.file->size) {
                            throw SuilError::create("uploading file: '", up.file->path(),
                                                     "' failed: ", errno_s);
                        }

                        nwr = sock.send(CRLF, sizeofcstr(CRLF), timeout);
                        if (nwr != sizeofcstr(CRLF)) {
                            throw SuilError::create("send CRLF failed: ", errno_s);
                        }

                        /* close boundary*/
                        up.close();
                    }

                    /* send end of boundary tag */
                    head.reset(1023, true);
                    head << "--" << form.boundary << "--"
                         << CRLF;

                    nwr = sock.send(head.data(), head.size(), timeout);
                    if (nwr != head.size()) {
                        throw SuilError::create("send terminal boundary failed: ", errno_s);
                    }
                }
                sock.flush();
            }

            Response Session::perform(handle_t& h, Method m, const char *resource, request_builder_t& builder, ResponseWriter& rd) {
                Request& req = h.req;
                Response resp;
                req.reset(m, resource, false);

                for(auto& hdr: headers) {
                    zcstring key(hdr.first.data(), hdr.first.size(), false);
                    zcstring val(hdr.second(), hdr.second.size(), false);
                    req.hdr(std::move(key), std::move(val));
                }

                if (builder != nullptr && !builder(req)) {
                    throw SuilError::create("building Request '", resource, "' failed");
                }

                if (!req.sock.isopen()) {
                    /* open a new socket for the Request */
                    if (!req.sock.connect(addr, timeout)) {
                        throw SuilError::create("Connecting to '", host(), ":",
                                                 port, "' failed: ", errno_s);
                    }
                }

                req.submit(timeout);
                resp.reader = rd;
                resp.receive(req.sock, timeout);
                return std::move(resp);
            }


            void Session::connect(handle_t &h, zmap<zcstring> hdrs) {
                Response resp = std::move(perform(h, Method::Connect, "/"));
                if (resp.status() != Status::OK) {
                    /* connecting to server failed */
                    throw SuilError::create("sending CONNECT to '", host(), "' failed: ",
                            http::Statusext(resp.status()));
                }

                /* cleanup Request and return it fresh */
                h.req.cleanup();
            }

            Response Session::head(handle_t &h, const char *resource, zmap<zcstring> hdr) {

                Response resp = std::move(perform(h, Method::Connect, resource));
            }

#undef CRLF
        }
    }
}