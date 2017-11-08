//
// Created by dc on 9/7/17.
//
#include <fcntl.h>
#include <sys/param.h>

#include <suil/http/client.hpp>
#include <sys/mman.h>

namespace suil {
    namespace http {
        namespace client {

#undef  CRLF
#define CRLF "\r\n"

            size_t Form::encode(buffer_t &out) const {
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
                        buffer_t tmp(127);
                        tmp << "--" << boundary << CRLF;
                        tmp << "Content-Disposition: form-data; name=\""
                            << ff.name << "\"; filename=\"" << basename(ff.path.str)
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
                    content_length += sizeofcstr("----") + boundary.len;
                    return content_length;
                }
            }

            int Form::Upload::open() {
                if (fd > 0) {
                    return fd;
                }
                fd = ::open(file->path.cstr, O_RDWR);
                if (fd < 0) {
                    /* opening file failed */
                    throw suil_error::create("open '", file->path.cstr,
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

            void response::receive(sock_adaptor &sock, int64_t timeout) {
                buffer_t tmp(1023);
                do {
                    size_t nrd = tmp.capacity();
                    if (!sock.read(&tmp[0], nrd, timeout)) {
                        /* failed to receive headers*/
                        throw suil_error::create("receiving request failed: ", errno_s);
                    }

                    if (!feed(tmp.data(), nrd)) {
                        throw suil_error::create("parsing headers failed: ",
                              http_errno_name((enum http_errno) http_errno));
                    }
                } while (!headers_complete);

                if (body_read || content_length == 0) {
                    strace("%s - response has no body to read: %lu", sock.id(), content_length);
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
                        throw suil_error::create("receive failed: ", errno_s);
                    }

                    tmp.seek(len);
                    // parse header line
                    if (!feed(tmp, len)) {
                        throw suil_error::create("parsing  body failed: ",
                                     http_errno_name((enum http_errno )http_errno));
                    }
                    left -= len;
                    // no need to reset buffer
                } while (!body_complete && left > 0);
            }

            int response::handle_body_part(const char *at, size_t length) {
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

            int response::handle_headers_complete() {
                /* if reader is configured, */
                if (reader != NULL) {
                    reader(NULL, content_length);
                }
                else {
                    body.reserve(content_length + 2);
                }
                return 0;
            }

            bool memory_offload::map_region(size_t len) {
                size_t total = len;
                int page_sz = getpagesize();
                total += page_sz-(len % page_sz);
                data = (char *) mmap(NULL, total, PROT_READ, MAP_SHARED , -1, 0);
                if (data == nullptr) {
                    swarn("client::memory_offload mmap failed: %s", errno_s);
                    return false;
                }
                is_mapped = true;
                return true;
            }

            memory_offload::~memory_offload() {
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

            int response::msg_complete() {
                if (reader) {
                    reader(nullptr, 0);
                }
                body_read = true;
            }

            const strview_t response::contenttype() const {
                auto it = headers.find("Content-Type");
                if (it != headers.end()) {
                    return it->second;
                }

                return strview_t();
            }

            request& request::operator=(request &&o) noexcept {
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
                    o.method = method_t::Unknown;
                    o.cleanup();
                }
                return *this;
            }

            request::request(request &&o) noexcept
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
                o.method = method_t::Unknown;
            }

            request& request::operator<<(Form &&f) {
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

            void request::encodeargs(buffer_t &dst) const {
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

            void request::encodehdrs(buffer_t &dst) const {
                if (!headers.empty()) {
                    for (auto& hdr: headers) {
                        dst << hdr.first << ": " << hdr.second << CRLF;
                    }
                }
            }

            size_t request::buildbody() {
                size_t content_length{0};
                if (utils::matchany(method, method_t::Put, method_t::Post)) {
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

            void request::submit(int timeout) {
                buffer_t head(1023);
                size_t  content_length{buildbody()};
                hdrs("Content-Length", content_length);

                head << method_name(method) << " " << resource;
                encodeargs(head);
                head << " HTTP/1.1" << CRLF;
                hdrs("Date", datetime()(datetime::HTTP_FMT));
                encodehdrs(head);
                head << CRLF;

                size_t nwr = sock.send(head.data(), head.size(), timeout);
                if (nwr != head.size()) {
                    /* sending headers failed, no need to send body */
                    throw suil_error::create("send request failed: (", nwr, ",",
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
                        throw suil_error::create("send request failed: ", errno_s);
                    }
                }

                if (!form.uploads.empty()) {
                    /* send upload files one after the other */
                    for (auto &up: form.uploads) {
                        int fd = up.open();
                        nwr = sock.send(up.head.cstr, up.head.len, timeout);
                        if (nwr != up.head.len) {
                            /* sending upload head failed, no need to send body */
                            throw suil_error::create("send request failed: ", errno_s);
                        }

                        /* send file */
                        nwr = sock.sendfile(fd, 0, up.file->size, timeout);
                        if (nwr != up.file->size) {
                            throw suil_error::create("uploading file: '", up.file->path.cstr,
                                                     "' failed: ", errno_s);
                        }

                        nwr = sock.send(CRLF, sizeofcstr(CRLF), timeout);
                        if (nwr != sizeofcstr(CRLF)) {
                            throw suil_error::create("send CRLF failed: ", errno_s);
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
                        throw suil_error::create("send terminal boundary failed: ", errno_s);
                    }
                }
                sock.flush();
            }

            response session::perform(handle_t& h, method_t m, const char *resource, request_builder_t& builder, response_writer_t& rd) {
                request& req = h.req;
                response resp;
                req.reset(m, resource, false);

                for(auto& hdr: headers) {
                    zcstring key(hdr.first.cstr, hdr.first.len, false);
                    zcstring val(hdr.second.cstr, hdr.second.len, false);
                    req.hdr(std::move(key), std::move(val));
                }

                if (builder != nullptr && !builder(req)) {
                    throw suil_error::create("building request '", resource, "' failed");
                }

                if (!req.sock.isopen()) {
                    /* open a new socket for the request */
                    if (!req.sock.connect(addr, timeout)) {
                        throw suil_error::create("Connecting to '", host.cstr, ":",
                                                 port, "' failed: ", errno_s);
                    }
                }

                req.submit(timeout);
                resp.reader = rd;
                resp.receive(req.sock, timeout);
                return std::move(resp);
            }


            void session::connect(handle_t &h, zcstr_map_t<zcstring> hdrs) {
                response resp = std::move(perform(h, method_t::Connect, "/"));
                if (resp.status() != status_t::OK) {
                    /* connecting to server failed */
                    throw suil_error::create("sending CONNECT to '", host.cstr, "' failed: ",
                            http::status_text(resp.status()));
                }

                /* cleanup request and return it fresh */
                h.req.cleanup();
            }

            response session::head(handle_t &h, const char *resource, zcstr_map_t<zcstring> hdr) {

                response resp = std::move(perform(h, method_t::Connect, resource));
            }

#undef CRLF
        }
    }
}