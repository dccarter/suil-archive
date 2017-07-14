//
// Created by dc on 28/06/17.
//

#include "response.hpp"

namespace suil {
    namespace http {

        response::response(response && other)
            : headers(std::move(other.headers)),
              cookies(std::move(other.cookies)),
              body(std::move(other.body)),
              status(other.status),
              completed(other.completed)
        {
        }

        response& response::operator=(response &&other) {
            status = other.status;
            body = std::move(other.body);
            headers = std::move(other.headers);
            cookies = std::move(other.cookies);
            completed = other.completed;
            return *this;
        }

        void response::clear() {
            completed = false;
            headers.clear();
            body.clear();
            cookies.clear();
            status = status_t::OK;
        }

        void response::end(status_t status) {
            this->status = status;
            completed = true;
        }

        void response::end(status_t status, buffer_t& body) {
            if (this->body)
                this->body << body;
            else
                this->body = std::move(body);

            this->status = status;
            completed = true;
        }

        void response::end(proto_handler_t p) {
            proto = p;
            status = status_t::SWITCHING_PROTOCOLS;
        }

        void response::flush_cookies() {
            // avoid allocating unnecessary memory
            buffer_t b(0);
            for(auto& it : cookies) {
                cookie_t& ck = it.second;
                if (!ck || !ck.value()) {
                    trace("ignoring invalid cookie in response");
                    continue;
                }

                b.reserve(127);

                b << ck.name();
                b.append("=", 1);
                b << ck.value();

                // append domain
                if (ck.domain()) {
                    const size_t sz = sizeofcstr("; Domain=");
                    b.append("; Domain=", sz);
                    b << ck.domain();
                }

                // append path
                if (ck.path()) {
                    const size_t sz = sizeofcstr("; Path=");
                    b.append("; Path=", sz);
                    b << ck.path();
                }

                // append secure
                if (ck.secure()) {
                    const size_t sz = sizeofcstr("; Secure");
                    b.append("; Secure", sz);
                }

                if (ck.maxage() > 0) {
                    b.appendf("; Max-Age=%lu", ck.maxage());
                }

                if (ck.expires() > 0) {
                    const size_t sz = sizeofcstr("; Expires=");
                    b.append("; Expires=", sz);
                    // by default the format for time is is http date
                    b.append(ck.expires(), nullptr);
                }

                zcstring ckv(b);
                zcstring ckh = zcstring("Set-Cookie").copy();
                header(std::move(ckh), std::move(ckv));
            }
        }

    }
}