//
// Created by dc on 28/06/17.
//

#include <suil/http/response.h>

namespace suil {
    namespace http {

        Response::Response(Response && other)
            : headers(std::move(other.headers)),
              cookies(std::move(other.cookies)),
              body(std::move(other.body)),
              status(other.status),
              completed(other.completed)
        {
        }

        Response& Response::operator=(Response &&other) {
            status = other.status;
            body = std::move(other.body);
            headers = std::move(other.headers);
            cookies = std::move(other.cookies);
            completed = other.completed;
            return *this;
        }

        void Response::clear() {
            completed = false;
            headers.clear();
            body.clear();
            cookies.clear();
            chunks.clear();
            status = Status::OK;
        }

        void Response::end(Status status) {
            Ego(status);
            completed = true;
        }

        void Response::end(Status status, OBuffer& body) {
            if (this->body)
                this->body << body;
            else
                this->body = std::move(body);

            Ego.end(status);
        }

        void Response::end(ProtocolHandler p) {
            proto = p;
            status = Status::SWITCHING_PROTOCOLS;
        }

        void Response::flush_cookies() {
            // avoid allocating unnecessary memory
            OBuffer b(0);
            for(auto& it : cookies) {
                Cookie& ck = it.second;
                if (!ck || !ck.value()) {
                    trace("ignoring invalid cookie in Response");
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

                String ckv(b);
                String ckh = String("Set-Cookie").dup();
                header(std::move(ckh), std::move(ckv));
            }
        }

    }
}