//
// Created by dc on 8/10/17.
//

#ifndef SUIL_AUTH_HPP
#define SUIL_AUTH_HPP

#include <suil/http.hpp>
#include <suil/sql/sqlite.hpp>

namespace suil {
    namespace http {

        define_log_tag(AUTHENTICATION);
        struct jwt_t {
            typedef decltype(iod::D(
                prop(typ, zcstring),
                prop(alg, zcstring)
            )) jwt_header_t;

            typedef decltype(iod::D(
                prop(iss, zcstring),
                prop(aud, zcstring),
                prop(sub, zcstring),
                prop(exp, int64_t),
                prop(claims, iod::json_string)
            )) jwt_payload_t;

            jwt_t(const char* typ)
            {
                header.typ = zcstring(typ).dup();
                header.alg = zcstring("HS256").dup();
            }

            jwt_t()
                : jwt_t("JWT")
            {}

            template <typename __T>
            inline void claims(__T& claims) {
                /* encode the claims to a json string*/
                payload.claims.str = std::move(iod::json_encode(claims));
            }

            template <typename... __Claims>
            inline void claims(__Claims... c) {
                auto tmp = iod::D(c...);
                /* encode the claims to a json string*/
                claims(tmp);
            }

            template <typename __T>
            inline __T claims() {
                __T c;
                iod::json_decode(c, payload.claims);
                return std::move(c);
            }

            inline const zcstring& typ() const {
                return header.typ;
            }

            inline const zcstring& alg() const {
                return header.alg;
            }

            inline const zcstring& iss() const {
                return payload.iss;
            }

            inline void iss(zcstring& iss){
                payload.iss = iss.dup();
            }

            inline void iss(const char* iss){
                payload.iss = zcstring(iss).dup();
            }

            inline const zcstring& aud() const {
                return payload.aud;
            }

            inline void aud(zcstring& aud) {
                payload.aud = aud.dup();
            }

            inline void aud(const char *aud) {
                payload.aud = zcstring(aud).dup();
            }

            inline const zcstring sub() const {
                return payload.sub;
            }

            inline void sub(zcstring& sub) {
                payload.sub = sub.dup();
            }

            inline void sub(const char* sub) {
                payload.sub = zcstring(sub).dup();
            }

            inline const int64_t exp() const {
                return payload.exp;
            }

            inline void exp(int64_t exp) {
                payload.exp = exp;
            }

            static bool decode(jwt_t& jwt, zcstring&& zcstr, zcstring& secret);
            static bool verify(zcstring&& zcstr, zcstring& secret);
            zcstring encode(zcstring& secret);

        private:
            jwt_header_t  header;
            jwt_payload_t payload;
        };

        typedef decltype(iod::D(
            s::_id(var(AUTO_INCREMENT), var(UNIQUE)) = int(),
            s::_username(var(PRIMARY_KEY)) = zcstring(),
            s::_email(var(UNIQUE))         = zcstring(),
            prop(passwd,     zcstring),
            prop(salt,       zcstring),
            prop(fullname,   zcstring)
        )) user_t;

        struct rand_8byte_salt {
            zcstring operator()(const zcstring & /*username*/);
            zcstring operator()(const char */* username*/) {
                zcstring tmp;
                return (*this)(tmp);
            }
        };

        struct pbkdf2_sha1_hash {
            pbkdf2_sha1_hash(const zcstring& appsalt)
                : appsalt(appsalt)
            {}

            zcstring operator()(zcstring& passwd, zcstring& salt);

            zcstring operator()(const char *passwd, const char* salt) {
                zcstring tpass(passwd);
                zcstring tsalt(salt);

                return (*this)(tpass, tsalt);
            }

        private:
            const zcstring& appsalt;
        };

        struct jwt_authorization : LOGGER(dtag(AUTHENTICATION)) {
            struct Context{
                Context()
                    : send_tok(0),
                      encode(0),
                      request_auth(0)
                {}

                inline void authorize(jwt_t&& jwt) {
                    this->jwt = std::move(jwt);
                    send_tok = 1;
                    encode = 1;
                    request_auth = 0;
                }

                inline status_t authenticate(status_t status, const char *msg = NULL) {
                    send_tok = 0;
                    encode   = 0;
                    request_auth = 1;
                    if (msg) {
                        token_hdr = zcstring(msg).dup();
                    }

                    return status;
                }

                inline status_t authenticate(const char *msg = NULL) {
                    return authenticate(status_t::UNAUTHORIZED, msg);
                }

                const zcstring& token() const  {
                    return actual_token;
                }

            private:
                jwt_t    jwt;
                friend struct jwt_authorization;
                union {
                    struct {
                        uint8_t send_tok : 1;
                        uint8_t encode : 1;
                        uint8_t request_auth: 1;
                        uint8_t __u8r5: 5;
                    };
                    uint8_t     _u8;
                } __attribute__((packed));
                zcstring token_hdr{};
                zcstring actual_token{};
            };

            void before(request& req, response& resp, Context& ctx) {
                if (req.route().AUTHORIZE) {
                    auto auth = req.header("Authorization");
                    if (auth.empty()||
                        strncasecmp(auth.data(), "Bearer ", 7)) {
                        /* user is not authorized */
                        resp.header("WWW-Authenticate", authenticate);
                        throw error::unauthorized();
                    }

                    /* temporary dup of the token */
                    ctx.actual_token = zcstring(&auth.data()[7], auth.size() - 7, false);
                    zcstring token(ctx.actual_token.dup());
                    if (!jwt_t::decode(ctx.jwt, std::move(token), key)) {
                        /* decoding the web-token failed */
                        resp.header("WWW-Authenticate", authenticate);
                        throw error::unauthorized();
                    }

                    /* token successfully decoded */
                    if (ctx.jwt.exp() < time(NULL)) {
                        /* token expired */
                        resp.header("WWW-Authenticate", authenticate);
                        throw error::unauthorized();
                    }
                    ctx.token_hdr = zcstring(auth.data(), auth.size(), false);
                    ctx.send_tok = 1;
                }
            }

            void after(request&, http::response& resp, Context& ctx) {
                /* if authorized token should have been set */
                if (ctx.send_tok) {
                    /* send token in header */
                    if (ctx.encode) {
                        /* encode the current json web-token*/
                        ctx.jwt.exp(time(NULL) + expiry);
                        zcstring encoded(ctx.jwt.encode(key));
                        zcstring token(utils::catstr("Bearer ", encoded));
                        resp.header("Authorization", token);
                    }
                    else {
                        /* move header from request */
                        resp.header("Authorization", ctx.token_hdr);
                    }
                } else if (ctx.request_auth) {
                    /* send authentication request */
                    resp.header("WWW-Authenticate", authenticate);
                    if (ctx.token_hdr) {
                        throw error::unauthorized(ctx.token_hdr.cstr);
                    }
                    else {
                        throw error::unauthorized();
                    }
                }
            }

            template <typename __Opts>
            void configure(__Opts& opts) {
                /* configure expiry time */
                expiry = opts.get(sym(expires), 3600);
                zcstring tmp(opts.get(sym(key), zcstring()));
                /* configure key */
                if (tmp) {
                    key = std::move(tmp);
                } else {
                    /* generate random key */
                    key  = rand_8byte_salt()("");
                }

                /* configure authenticate header string */
                tmp = opts.get(sym(realm), zcstring());
                if (tmp) {
                    authenticate = utils::catstr("Bear realm=\"",tmp, "\"");
                }
            }

            jwt_authorization()
                : key(rand_8byte_salt()("")),
                  authenticate(zcstring("Bearer").dup())
            {
                debug("generated secret is %s", key.cstr);
            }

        private:
            uint64_t  expiry{3600};
            zcstring  key;
            zcstring  authenticate;
        };

        namespace auth {
            template <typename __C>
            bool getuser(__C& conn, user_t& user) {
                buffer_t qb(32);
                qb << "SELECT * FROM users WHERE username='";
                __C::params(qb, 0); qb << "'";

                return (conn(qb)(user.username) >> user);
            }

            template <typename __C>
            bool hasuser(__C& conn, user_t& user) {
                int users = 0;
                buffer_t qb(32);
                qb << "SELECT COUNT(username) FROM users WHERE username='";
                __C::params(qb, 0);
                qb << "'";

                conn(qb)(user.username) >> users;
                return users != 0;
            }

            template <typename __C>
            bool updateuser(__C& conn, user_t& user) {
                sql::orm<__C,user_t> orm("users", conn);
                return orm.update(user) == 1;
            }

            template <typename __C>
            void removeuser(__C& conn, user_t& user) {
                sql::orm<__C,user_t> orm("users", conn);
                orm.remove(user);
            }

            template <typename __C>
            bool adduser(__C& conn, const zcstring& key, user_t& user) {
                /* generate random salt for user */
                user.salt   = rand_8byte_salt()(nullptr);
                user.passwd = pbkdf2_sha1_hash(key)(user.passwd, user.salt);
                sql::orm<__C,user_t> orm("users", conn);
                return orm.insert(user) == 1;
            }

            template <typename __C>
            bool updateuser(__C& conn, const zcstring& key, user_t& user) {
                /* generate random salt for user */
                user.salt   = rand_8byte_salt()(nullptr);
                user.passwd = pbkdf2_sha1_hash(key)(user.passwd, user.salt);
                sql::orm<__C,user_t> orm("users", conn);
                return orm.update(user) == 1;
            }

            template <typename __C>
            bool seedusers(__C& conn, const zcstring& key, std::vector<user_t> users) {
                sql::orm<__C,user_t> orm("users", conn);
                if (!orm.exists()) {
                    /* table does not exist */
                    throw suil_error::create("table 'users' does not exist");
                }

                for (auto& user : users) {
                    user.salt   = rand_8byte_salt()(nullptr);
                    user.passwd = pbkdf2_sha1_hash(key)(user.passwd, user.salt);
                    if ((orm.update(user)) != 1) {
                        swarn("seed database table with user '%' failed", user.username.cstr);
                        return false;
                    }
                }

                return true;
            }

            template <typename __C>
            bool verifyuser(__C& conn, const zcstring& key, user_t& user) {
                user_t u;
                u.username = user.username;
                if (getuser(conn, u)) {
                    /* user exists */
                    zcstring passwd = pbkdf2_sha1_hash(key)(user.passwd, u.salt);
                    if (passwd == u.passwd) {
                        /* user authorized */
                        return true;
                    }
                }

                return false;
            }
        }
    }
}
#endif //SUIL_AUTH_HPP
