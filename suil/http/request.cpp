//
// Created by dc on 27/06/17.
//

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
                len = MIN(stage.capacity(), left);
                if (!sock.receive(&stage[0], len, config.connection_timeout)) {
                    trace("%s - receive failed: %s", sock.id(), errno_s);
                    status = (errno == ETIMEDOUT)?
                             status_t::REQUEST_TIMEOUT:
                             status_t::INTERNAL_ERROR;
                    break;
                }
                stats.rx_bytes += len;

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
            ssize_t toread = 0;
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
            else if ((body.size()-body_offset) > (size_t) toread) {
                toread = MIN((body.size()-body_offset), len);
                // just copy data from the already read body
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
            cookied = false;
        }
    }
}