//
// Created by dc on 27/06/17.
//

#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>

#include "request.hpp"

namespace suil {
    namespace http {

        bool request::parse_cookies() {
            zcstring key("Cookie");
            cookied = true;
            auto it = headers.find(key);
            if (it == headers.end()) {
                // no cookies in request
                return false;
            }

            const std::vector<char *> parts = utils::strsplit(it->second, ";");
            for (char *ch: parts) {
                char *tmp = ch;

                while(isspace(*ch) && *ch != '\0') ch++;
                if (*ch == '\0')  {
                    trace("invalid cookie in header %s", tmp);
                }

                char *name = ch;
                char *value = nullptr;

                char *k = strchr(ch, '=');
                if (k != nullptr) {
                    *k = '\0';
                    value = ++k; // mind the ++
                }
                else {
                    value = nullptr;
                }
                zcstring ck(name, (k-name-1), false);
                if (value) {
                    zcstring cv(value, strlen(value), false);
                    cookies.emplace(std::move(ck), std::move(cv));
                } else {
                    cookies.emplace(std::move(ck), zcstring());
                }
            }

            return true;
        }

        bool request::parse_form() {
            if (formed) {
                trace("form parse already attempted");
                return true;
            }

            // we attempted to parse form
            formed = true;

            if (!any_method(method_t::Post, method_t::Put)) {
                trace("parsing form in unexpected method %u", method);
                return false;
            }

            auto ctype = header("Content-Type");
            if (ctype.size() == 0) {
                trace("only posts with content type are supported");
                return false;
            }

            if (ctype == "application/x-www-form-urlencoded") {
                trace("parsing url encoded form");
                return parse_url_encoded_form();
            }

            size_t len = std::min(sizeof("multipart/form-data")-1, ctype.size());
            if (strncasecmp(ctype.data(), "multipart/form-data", len) == 0) {
                trace("parsing url encoded form");
                const char *boundary = strchr(ctype.data(), '=');
                if (boundary == nullptr) {
                    debug("multipart/form-data without boundary: %", ctype.data());
                    return false;
                }
                boundary++;
                zcstring tmp(boundary);

                return parse_multipart_form(boundary);
            }

            return false;
        }

#define eat_space(p) while (*(p) == ' ' && *(p) != '\r') (p)++

       static inline bool _get_header(char *&p, char *e, char *&f, char *&v) {
           f = p;
           if ((p = strchr(p, ':')) == nullptr)
               return false;
           *p++ = '\0';
           eat_space(p);
           v = p;
           while (*(++p) != '\r' && p != e);

           return (*p == '\r' && *++p == '\n');
        };

#define goto_chr(p, e, c) ({bool __v = false;                       \
        while ((p) != (e) && *(p) != (c) && *(p) != '\r') (p)++;    \
        if (*(p) == (c)) __v = true; __v; })

        static inline bool _get_disposition(char *&p, char *e, char *&name, char *&filename) {
            name = filename = nullptr;
            while (*p != '\0' && p != e) {
                if (strncasecmp(p, "name=\"", 4) == 0) {
                    /* eat name=" */
                    p += 6;
                    name = p;
                    if (goto_chr(p, e, '"')) {
                        *p++ = '\0';
                    } else
                        return false;
                }
                else if (strncasecmp(p, "filename=\"", 10) == 0) {
                    /* eat filename= */
                    p += 10;
                    filename = p;
                    if (goto_chr(p, e, '"')) {
                        *p++ = '\0';
                    } else
                        return false;
                }
                else {
                    /* unknown field */
                    return false;
                }

                if (*p == ';') p++;
                eat_space(p);
            }

            return true;
        }

        bool request::parse_multipart_form(const zcstring& boundary) {
            buffer_t rb((uint32_t) content_length+2);
            if (read_body(rb.data(), content_length) <= 0) {
                debug("reading body failed");
                return false;
            }
            debug("%s", rb.data());

            enum {
                state_begin, state_is_boundary,
                state_boundary, state_save_data,
                state_save_file, state_header,
                state_content, state_data,
                state_error, state_end
            }  state = state_begin, next_state = state_begin;
            rb.bseek(content_length);
            char *p = rb.data(), *end = p + content_length;
            char *name = nullptr, *filename = nullptr, *data = nullptr, *dend = nullptr;
            bool cap{false};
            debug("start parse multipart form %d", mnow());
            while (cap || (p != end)) {
                switch (next_state) {
                    case state_begin:
                        trace("multipart/form-data state_begin");
                        /* fall through */
                        state = state_error;
                        next_state = state_is_boundary;
                    case state_is_boundary:
                        trace("multipart/form-data state_is_boundary");
                        if (*p == '-' && *++p == '-') {
                            state = state_is_boundary;
                            next_state = state_boundary;
                            ++p;
                        } else {
                            next_state = state;
                        }
                        break;

                    case state_boundary:
                        state = state_is_boundary;
                        trace("multipart/form-data state_boundary");
                        if (strncmp(p, boundary.cstr, boundary.len) != 0) {
                            /* invalid boundary */
                            debug("multipart/form-data invalid boundary");
                            next_state = state_error;
                        }
                        else {
                            p += boundary.len;
                            bool el = (p[0] == '\r' && p[1] == '\n');
                            bool eb = (p[0] == '-' && p[1] == '-');
                            if (el || eb)
                            {
                                state = el? state_content : state_end;
                                p += 2;
                                if (filename != nullptr) {
                                    next_state = state_save_file;
                                    cap = true;
                                } else if (name != nullptr) {
                                    next_state = state_save_data;
                                    cap = true;
                                } else if (el) {
                                    next_state = state_content;
                                } else
                                    next_state = state_end;
                            }
                            else {
                                debug("multipart/form-data invalid state %d %d", el, eb);
                                next_state = state_error;
                            }
                        }
                        break;

                    case state_save_file: {
                        trace("multipart/form-data state_save_file");
                        *dend-- = '\0';
                        upfile_t f;
                        zcstring tmp(filename);
                        zcstring tmp_name(name);
                        f.name_ = std::move(tmp);
                        f.len_  = dend - data;
                        f.data_ = data;
                        files.emplace(std::move(tmp_name),
                                      std::move(f));
                        filename = data = dend = nullptr;
                        next_state = state;
                        cap = false;

                        break;
                    }

                    case state_save_data: {
                        trace("multipart/form-data state_save_data");
                        *--dend = '\0';
                        zcstring tmp(name);
                        zcstring tmp_data(data, dend - data, false);
                        form.emplace(std::move(tmp), std::move(tmp_data));
                        name = data = dend = nullptr;

                        next_state = state;
                        cap = false;
                        break;
                    }

                    case state_content:
                        trace("multipart/form-data state_content");
                        char *disp, *val;
                        state = state_content;
                        next_state = state_error;
                        if (_get_header(p, end, disp, val) &&
                            strcasecmp(disp, "Content-Disposition") == 0)
                        {
                            *(++p-2) = '\0';
                            trace("multipart/form-data content disposition: %s", val);
                            if (strncasecmp(val, "form-data", 9)) {
                                /* unsupported disposition */
                                debug("multipart/form-data not content disposition: %s",
                                      val);
                                continue;
                            }
                            /* eat form-data */
                            val += 10;
                            eat_space(val);
                            if (_get_disposition(val, end, name, filename)) {
                                /* successfully go disposition */
                                trace("multipart/form-data name: %s, filename %s",
                                name, filename);
                                next_state = state_header;
                            } else {
                                debug("multipart/form-data invalid disposition: %s", val);
                            }
                        } else {
                            debug("multipart/form-data missing disposition");
                        }
                        break;

                    case state_header:
                        state = state_header;
                        trace("multipart/form-data state_header\n%s", p);
                        char *field, *value;
                        if (*p == '\r' && *++p == '\n') {
                            data = ++p;
                            next_state = state_data;
                        }
                        else if (_get_header(p, end, field, value)) {
                            *(++p-2) = '\0';
                            trace("multipart/form-data header field: %s, value: %s",
                                  field, value);
                        }
                        else {
                            debug("multipart/form-data parsing header failed");
                            next_state = state_error;
                        }
                        break;

                    case state_data:
                        state = state_data;
                        trace("multipart/form-data state_data");
                        dend = p;
                        ++p;
                        next_state = state_is_boundary;
                        break;

                    case state_end:
                        state = state_end;
                        debug("multipart/form-data state machine done %d fields, %d files %d",
                                form.size(), files.size(), mnow());
                        if (form.size() || files.size()) {
                            // cache the buffer for later references
                            zcstring tmp(rb);
                            form_str = std::move(tmp);
                        }
                        return true;

                    case state_error:
                    default:
                        debug("multipart/form-data error in state machine");
                        return false;
                }
            }

            return false;
        }

#undef eat_space
#undef goto_chr

        bool request::parse_url_encoded_form() {
            buffer_t rb((uint32_t) content_length+2);
            if (read_body(rb.data(), content_length) > 0) {
                rb.bseek(content_length);
                zcstring tmp(rb);
                auto parts = utils::strsplit(tmp, "&");
                for(auto& part : parts) {
                    /* save all parameters in part */
                    char *name = part;
                    char *value = strchr(part, '=');
                    if (value == nullptr) {
                        value = (char *) "";
                    }
                    else {
                        (*value) = '\0';
                        value++;
                    }
                    zcstring tname(name);
                    zcstring tvalue(value);
                    form.emplace(std::move(tname), std::move(tvalue));
                }

                if (!parts.empty()) {
                    /* took buffer, if has valid data keep for future reference */
                    form_str = std::move(tmp);
                }
                return true;
            }

            return false;
        }
        
        request::body_offload::body_offload(buffer_t &path)
            : file_t(mfmktemp((char *)path))
        {
            if (fd) {
                this->path = zcstring(std::move(path));
            }
        }

        status_t request::receive_headers(server_stats_t& stats) {
            status_t  status = status_t::OK;
            stage.reserve(1023);

            char *ptr = (char *) &stage[0];
            do {
                size_t len = stage.capacity();
                // receive a chunk of headers
                if (!sock.read(ptr, len, config.connection_timeout)) {
                    trace("%s - receiving headers failed: %s",
                          sock.id(), errno_s);
                    status = (errno == ETIMEDOUT)?
                             status_t::REQUEST_TIMEOUT : status_t::INTERNAL_ERROR;
                    break;
                }
                stats.rx_bytes += len;
                // parse the chunk of received headers
                if (!feed(ptr, len)) {
                    trace("%s - parsing headers failed: %s",
                            sock.id(), http_errno_name((enum http_errno) http_errno));
                    status = status_t::BAD_REQUEST;
                    break;
                }

            } while(!headers_complete);

            if (status == status_t::OK) {
                // process the completed headers
                status = process_headers();
            }

            stage.clear();
            return status;
        }

        status_t request::process_headers() {
            if (headers.count("Content-Length"))
            {
                if (content_length > config.max_body_len) {
                    trace("%s - body request too large: %d", sock.id(), content_length);
                    return status_t::REQUEST_ENTITY_TOO_LARGE;
                }
                has_body = 1;
            }

            if (has_body && config.disk_offload &&
                content_length > config.disk_offload_min)
            {
                buffer_t tmp(64);
                tmp.appendf(tmp,"%s/http_body.XXXXXX", config.offload_path.c_str());
                if (offload == nullptr) {
                    offload = new body_offload(tmp);
                    offload->length = content_length;
                }

                if (!offload->valid()) {
                    trace("%s - error opening file(%s) to save body: %s",
                          sock.id(), offload->path.cstr, errno_s);
                    return status_t::INTERNAL_ERROR;
                }
            }

            return status_t::OK;
        }

        status_t request::receive_body(server_stats_t& stats) {
            status_t status = status_t::OK;
            if (!has_body || body_complete) {
                return status;
            }
            size_t len  = 0, left = content_length;
            // read body in chunks
            stage.reserve(MIN(content_length, 2048));

            do {
                stage.reset(left, true);
                len = MIN(stage.capacity(), left);
                if (!sock.receive(&stage[0], len, config.connection_timeout)) {
                    trace("%s - receive failed: %s", sock.id(), errno_s);
                    status = (errno == ETIMEDOUT)?
                             status_t::REQUEST_TIMEOUT:
                             status_t::INTERNAL_ERROR;
                    break;
                }
                stats.rx_bytes += len;

                stage.seek(len);
                // parse header line
                if (!feed(stage, len)) {
                    trace("%s - parsing failed: %s",
                          sock.id(), http_errno_name((enum http_errno )http_errno));
                    status = status_t::BAD_REQUEST;
                    break;
                }
                left -= len;
                // no need to reset buffer
            } while (!body_complete && left > 0);

            if (status == status_t::OK && !body_complete) {
                trace("%s - parsing failed, body not complete",
                         sock.id(), port());
                status = status_t::BAD_REQUEST;
            }

            if (status == status_t::OK && offload == nullptr) {
                body_read = 1;
            }

            // clear the buffer after reading headers
            stage.clear();
            return status;
        }

        int request::handle_body_part(const char *at, size_t length) {
            if (config.disk_offload && offload) {
                size_t nwr = offload->write(at, length, config.connection_timeout);
                if (nwr != length) {
                    trace("%s error offloading body: %s", sock.id(), errno_s);
                    return -1;
                }
            }
            else {
                return  parser::handle_body_part(at, length);
            }

            return 0;
        }

        int request::msg_complete() {
            has_body = 1;
            body_read = 1;
            return parser::msg_complete();
        }

        strview_t request::get_body() {
            if (!has_body || body_read || body_error) {
                return body;
            }

            if (offload) {
                // read all body
                body.reserve(content_length+2);
                if (read_body(&body[0], content_length) < 0) {
                    debug("%s - reading body failed: %s", sock.id(), errno_s);
                    body.clear();
                    body_error = 1;
                }
                body_read = 1;
            }

            return body;
        }

        ssize_t request::read_body(void *buf, size_t len) {
            ssize_t toread = len;
            if (buf == nullptr || toread <= 0)
                return -1;

            if (!body_read || (!offload_error && offload)) {
                char *ptr = (char *) buf;
                toread = MIN((offload->length - body_offset), len);
                size_t nrd = 0;

                do {
                    size_t chunk = MIN(4096, (size_t) (toread-nrd));
                    if (offload->read((ptr+nrd), chunk, config.connection_timeout)) {
                        trace("%s - reading body failed: %s", sock.id(), errno_s);
                        offload_error = 1;
                        return -1;
                    }

                    nrd += chunk;
                }   while (nrd != (size_t) toread);
            }
            else if ((body.size()-body_offset) >= (size_t) toread) {
                toread = MIN((body.size()-body_offset), len);
                // just dup data from the already read body
                memcpy(buf, &body[body_offset], (size_t) toread);
            }

            body_offset += toread;

            return toread;
        }

        bool request::body_seek(off_t off) {
            if (!body_read) {
                if (!offload_error && offload) {
                    offload->seek(off);
                    return true;
                }
            }
            else if (!body_error) {
                body_offset = (uint16_t) off;
                return true;
            }
            return false;
        }

        void request::clear() {
            parser::clear();
            files.clear();
            cookies.clear();
            if (offload) {
                delete offload;
                offload = NULL;
            }
            stage.reset(0);
            has_body = 0;
            body_read = 0;
            body_error = 0;
            offload_error = 0;

            cookied = false;
            if (formed) {
                form.clear();
                files.clear();
                formed = false;
            }
        }

        request_form_t::request_form_t(const request &req)
            : req(req)
        {}

        void request_form_t::operator|(form_data_it_t f) {
            for(const auto& dt : req.form) {
                if (f(dt.first, dt.second)) {
                    break;
                }
            }
        }

        void request_form_t::operator|(form_file_it_t f) {
            for(const auto& dt : req.files) {
                if (f(dt.second)) {
                    break;
                }
            }
        }

        const zcstring& request_form_t::operator[](const char *key) {
            zcstring tmp(key);
            const auto it = req.form.find(tmp);
            if (it != req.form.end()) {
                return it->second;
            }
            throw error::internal("form data not found in post form");
        }

        const upfile_t& request_form_t::operator()(const char *f) {
            zcstring tmp(f);
            const auto it = req.files.find(tmp);
            if (it != req.files.end()) {
                return it->second;
            }
            throw error::internal("file not found in form");
        }

        bool request_form_t::find(zcstring& out, const char *name) {
            zcstring tmp(name);
            auto it = req.form.find(tmp);
            if (it != req.form.end()) {
                out = std::move(zcstring(it->second.cstr, it->second.len, false));
                return true;
            }

            return false;
        }

        void upfile_t::save(const char *dir, int64_t timeout) const {
            zcstring real_dir = utils::fs::realpath(dir);
            if (!real_dir || !utils::fs::isdir(real_dir.cstr)) {
                /* directory is not valid */
                throw suil_error::create("directory: '", dir, "' invalid.");
            }

            zcstring path = utils::catstr(real_dir.cstr, "/", basename(name_.str));
            if (!path || utils::fs::isdir(path.cstr)) {
                /* directory is not valid */
                throw suil_error::create("file with name: '", name_.cstr, "' not supported.");
            }

            /* async write data to disk */
            file_t writer(path.cstr, O_WRONLY | O_CREAT, 0644);
            writer.write(data_, len_, timeout);
        }
    }
}