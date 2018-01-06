//
// Created by dc on 8/28/17.
//
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <suil/http/auth.hpp>

namespace suil {
    namespace http {

        bool Jwt::decode(Jwt& jwt,zcstring &&jwtstr, zcstring& secret) {
            /* header.payload.signature */
            char *tok_sig = strrchr(jwtstr.data(), '.');
            if (tok_sig == NULL) {
                /* invalid json token */
                throw error::unauthorized(
                        "invalid token");
            }
            *tok_sig++ = '\0';
            zcstring data(jwtstr.data());

            /* get signature hash  */
            zcstring signature =
                    utils::shaHMAC256(secret, data, true);
            strace("token signature orig:%s, generated %s",
                   tok_sig, signature.data());

            if (strcmp(tok_sig, signature.data())) {
                /* token invalid */
                return false;
            }

            auto parts = std::move(utils::strsplit(data, "."));
            if (parts.size() != 2) {
                /* invalid token */
                /* invalid json token */
                throw error::unauthorized(
                        "invalid token");
            }

            zcstring header(base64::decode(parts[0]));
            strace("header: %s", header.data());
            iod::json_decode(jwt.header, header);
            zcstring payload(base64::decode(parts[1]));
            strace("header: %s", payload.data());
            iod::json_decode(jwt.payload, payload);
            return true;
        }

        bool Jwt::verify(zcstring&& jwtstr, zcstring &secret) {
            /* header.payload.signature */
            char *tok_sig = strrchr(jwtstr.data(), '.');
            if (tok_sig == NULL) {
                /* invalid json token */
                throw error::unauthorized(
                        "invalid token");
            }
            *tok_sig++ = '\0';
            zcstring data(jwtstr.data());

            /* get signature hash  */
            zcstring signature =
                    utils::shaHMAC256(secret, data);
            strace("token signature orig:%s, generated %s",
                   tok_sig, signature.data());

            return strcmp(tok_sig, signature.data()) == 0;
        }

        zcstring Jwt::encode(zcstring& secret) {
            /* encode jwt */
            /* 1. base64(hdr) & base64(payload)*/
            std::string raw_hdr(iod::json_encode(header));
            zcstring hdr(std::move(base64::encode(raw_hdr)));
            std::string raw_data(iod::json_encode(payload));
            zcstring data(std::move(base64::encode(raw_data)));

            /* 2. base64(hdr).base64(payload) */
            zbuffer tmp(hdr.size() + data.size() + 4);
            tmp << hdr << "." << data;

            /* 3. HMAC_Sha256 (base64(hdr).base64(payload))*/
            zcstring signature =
                    utils::shaHMAC256(secret,
                                      (const uint8_t *) tmp.data(),
                                      tmp.size(), true);

            /* 4. header.payload.signature */
            tmp << "." << signature;
            strace("encoded jwt: %s", tmp.data());

            return std::move(zcstring(tmp));
        }

        zcstring rand_8byte_salt::operator()(const zcstring &) {
            /* simple generate random bytes */
            uint8_t key[8];
            RAND_bytes(key, 8);
            return std::move(utils::hexstr(key, 8));
        }

        zcstring pbkdf2_sha1_hash::operator()(zcstring &passwd, zcstring& salt) {
            /* simply hash using SHA_256 appsalt.passwd.salt*/
            zcstring tmp = utils::catstr(appsalt, ".", salt);
            uint8_t out[SHA_DIGEST_LENGTH];
            int rc = PKCS5_PBKDF2_HMAC_SHA1(passwd.data(), passwd.size(),
                              (const uint8_t*)tmp.data(), tmp.size(),
                                   1000, SHA_DIGEST_LENGTH, out);

            if (rc) {
                /* convert buffer to byte string */
                return std::move(utils::hexstr(out, SHA_DIGEST_LENGTH));
            }

            return zcstring();
        }

        void JwtAuthorization::before(Request& req, Response& resp, Context& ctx) {
            if (req.route().AUTHORIZE) {
                zcstring tkhdr;
                if (use.use == JwtUse::HEADER) {
                    auto auth = req.header(use.key);
                    if (auth.empty() ||
                        strncasecmp(auth.data(), "Bearer ", 7)) {
                        /* user is not authorized */
                        authrequest(resp);
                    }
                    /* temporary dup of the token */
                    tkhdr = zcstring(auth.data(), auth.size(), false);
                    ctx.actual_token = zcstring(&auth.data()[7], auth.size() - 7, false);
                }
                else {
                    // token
                    CookieIterator cookies = req();
                    auto auth = cookies[use.key];
                    if (!auth) {
                        /* user is not authorized */
                        authrequest(resp);
                    }
                    tkhdr = auth;
                    ctx.actual_token = auth;
                }

                if (ctx.actual_token.size() == 0) {
                    /* no need to proceed, token is invalid */
                    authrequest(resp);
                }

                zcstring token(ctx.actual_token.dup());

                bool valid{false};
                try {
                    valid = Jwt::decode(ctx.jwt, std::move(token), key);
                }
                catch(...) {
                    /* error decoding token */
                    authrequest(resp, "Invalid authorization token.");
                }

                if (!valid || (ctx.jwt.exp() < time(NULL))) {
                    /* token unauthorized */
                    authrequest(resp);
                }

                if (!req.route().AUTHORIZE.check(ctx.jwt.roles())) {
                    /* token does not have permission to access resouce */
                    authrequest(resp, "Access to resource denied.");
                }

                ctx.token_hdr = tkhdr;
                ctx.send_tok = 1;
            }
        }

        void JwtAuthorization::after(Request&, http::Response& resp, Context& ctx) {
            /* if authorized token should have been set */
            if (ctx.send_tok) {
                zcstring tok{nullptr}, tok2{nullptr};
                /* send token in header */
                if (ctx.encode) {
                    /* encode the current json web-token*/
                    ctx.jwt.exp(time(NULL) + expiry);
                    zcstring encoded(ctx.jwt.encode(key));
                    tok = utils::catstr("Bearer ", encoded);
                    tok2 = std::move(encoded);
                }
                else {
                    /* move header from Request */
                    tok = std::move(ctx.token_hdr);
                }

                if (use.use == JwtUse::HEADER) {
                    resp.header(use.key.peek(), std::move(tok));
                }
                else {
                    Cookie cookie(use.key.data());
                    cookie.domain(domain.peek());
                    cookie.path(path.peek());
                    if (tok2) {
                        cookie.value(std::move(tok2));
                    } else {
                        cookie.value(std::move(tok));
                    }
                    cookie.expires(time(NULL) + expiry);
                    resp.cookie(cookie);
                }
            } else if(!ctx.logout_url.empty()) {
                resp.redirect(Status::FOUND, ctx.logout_url.data());
                if (use.use == JwtUse::COOKIE) {
                    // delete cookie
                    delete_cookie(resp);
                }
            } else if (ctx.request_auth) {
                /* send authentication Request */
                resp.header("WWW-Authenticate", authenticate);
                if (ctx.token_hdr) {
                    throw error::unauthorized(ctx.token_hdr.data());
                }
                else {
                    throw error::unauthorized();
                }
            }
        }
    }
}