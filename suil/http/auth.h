//
// Created by dc on 8/10/17.
//

#ifndef SUIL_AUTH_HPP
#define SUIL_AUTH_HPP

#include <suil/http.h>
#include <suil/logging.h>
#include <suil/sql/sqlite.h>

namespace suil {
    namespace http {

        define_log_tag(AUTHENTICATION);
        struct Jwt {
            typedef decltype(iod::D(
                prop(typ, String),
                prop(alg, String)
            )) JwtHeader;

            typedef decltype(iod::D(
                s::_iss(var(optional),   var(ignore))    = String(),
                s::_aud(var(optional),   var(ignore))    = String(),
                s::_sub(var(optional),   var(ignore))    = String(),
                s::_exp(var(optional),   var(ignore))    = int64_t(),
                s::_iat(var(optional),   var(ignore))    = int64_t(),
                s::_nbf(var(optional),   var(ignore))    = int64_t(),
                s::_jti(var(optional),   var(ignore))    = String(),
                s::_claims(var(optional),var(ignore))    = json::Object()
            )) JwtPayload;

            Jwt(const char* typ)
            {
                header.typ = String(typ).dup();
                header.alg = String("HS256").dup();
                iod::zero(payload);
                payload.claims = json::Object(json::Obj);
            }

            Jwt()
                : Jwt("JWT")
            {}

            Jwt(Jwt&& o) noexcept
                : header(std::move(o.header)),
                  payload(std::move(o.payload))
            {}

            Jwt& operator=(Jwt&& o) noexcept {
                if (this != &o) {
                    Ego.header  = std::move(o.header);
                    Ego.payload = std::move(o.payload);
                }
                return Ego;
            }

            Jwt(const Jwt&) = delete;
            Jwt& operator=(const Jwt&) = delete;

            template <typename T, typename... Claims>
            inline void claims(const char* key, T value, Claims... c) {
                Ego.payload.claims.set(key, std::forward<T>(value), std::forward<Claims>(c)...);
            }

            inline void roles(std::vector<String>& roles) {
                auto rs = Ego.payload.claims["roles"];
                if (rs.empty())
                    Ego.payload.claims.set("roles", json::Object{roles});
                else for(auto& r: roles)
                    rs.push(r);
            }

            template <typename R, typename... Roles>
            inline void roles(R r, Roles... roles) {
                auto rs = Ego.payload.claims["roles"];
                if (rs.empty())
                    Ego.payload.claims.set("roles", json::Object(json::Arr,
                            std::forward<R>(r), std::forward<Roles>(roles)...));
                else
                    rs.push(std::forward<R>(r), std::forward<Roles>(roles)...);
            }

            inline json::Object roles() {
                return Ego.payload.claims["roles"];
            }

            inline json::Object& claims() {
                // get the claims
                return payload.claims;
            }

            inline const String& typ() const {
                return header.typ;
            }

            inline const String& alg() const {
                return header.alg;
            }

            inline const String& iss() const {
                return payload.iss;
            }

            inline void iss(String& iss){
                payload.iss = iss.dup();
            }

            inline void iss(const char* iss){
                payload.iss = String(iss).dup();
            }

            inline const String& aud() const {
                return payload.aud;
            }

            inline void aud(String& aud) {
                payload.aud = aud.dup();
            }

            inline void aud(const char *aud) {
                payload.aud = String(aud).dup();
            }

            inline const String sub() const {
                return payload.sub;
            }

            inline void sub(String& sub) {
                payload.sub = sub.dup();
            }

            inline void sub(const char* sub) {
                payload.sub = String(sub).dup();
            }

            inline const int64_t exp() const {
                return payload.exp;
            }

            inline void exp(int64_t exp) {
                payload.exp = exp;
            }

            inline const int64_t iat() const {
                return payload.iat;
            }

            inline void iat(int64_t iat) {
                payload.iat = iat;
            }

            inline const int64_t nfb() const {
                return payload.nbf;
            }

            inline void nbf(int64_t nbf) {
                payload.exp = nbf;
            }

            inline const String& jti() const {
                return payload.jti;
            }

            inline void jti(String&& jti) {
                payload.jti = std::move(jti);
            }

            static bool decode(Jwt& jwt, String&& zcstr, String& secret);
            static bool verify(String&& zcstr, String& secret);
            String encode(String& secret);

        private:
            JwtHeader  header;
            JwtPayload payload;
        };

        typedef decltype(iod::D(
            s::_id(var(AUTO_INCREMENT), var(UNIQUE), var(optional)) = int(),
            s::_username(var(PRIMARY_KEY)) = String(),
            s::_email(var(UNIQUE))         = String(),
            prop(passwd,     String),
            s::_salt(var(optional))        = String(),
            prop(fullname,   String),
            s::_roles(var(optional))       = std::vector<String>(),
            s::_verified(var(optional))    = bool(),
            s::_locked(var(optional))      = bool()
        )) User;

        struct rand_8byte_salt {
            String operator()(const String & /*username*/);
            String operator()(const char */* username*/) {
                String tmp;
                return (*this)(tmp);
            }
        };

        struct pbkdf2_sha1_hash {
            pbkdf2_sha1_hash(const String& appsalt)
                : appsalt(appsalt)
            {}

            String operator()(String& passwd, String& salt);

            String operator()(const char *passwd, const char* salt) {
                String tpass(passwd);
                String tsalt(salt);

                return (*this)(tpass, tsalt);
            }

        private:
            const String& appsalt;
        };

        struct JwtUse {
            typedef enum { HEADER, COOKIE} From;
            JwtUse()
                : use(From::HEADER)
            {}
            JwtUse(From from, const char *key)
                : use(from),
                  key(String(key).dup())
            {}

            From use;
            String key{"Authorization"};
        };

        struct JwtAuthorization : LOGGER(AUTHENTICATION) {
            struct Context{
                Context()
                    : sendTok(0),
                      encode(0),
                      requestAuth(0)
                {}

                inline void authorize(Jwt&& jwt) {
                    this->jwt = std::move(jwt);
                    Ego.jwt.iat(time(nullptr));
                    Ego.jwt.exp(time(nullptr) + jwtAuth->expiry);
                    sendTok = 1;
                    encode = 1;
                    requestAuth = 0;
                    Ego.actualToken = Ego.jwt.encode(jwtAuth->key);
                }

                inline bool authorize(const String& token) {
                    /* decode and use given token in authorization */
                    Jwt tok;
                    if (Jwt::decode(tok, token.dup(), jwtAuth->key)) {
                        /* decoded, authorize only if not expired */
                        if (tok.exp() > time(NULL)) {
                            /* token valid, authorize with token */
                            this->jwt = std::move(tok);
                            sendTok = 1;
                            encode  = 1;
                            requestAuth = 0;
                            Ego.actualToken = token.dup();
                            return true;
                        }
                    }
                    /*  token not valid */
                    return false;
                }

                inline Status authenticate(Status status, const char *msg = NULL) {
                    sendTok = 0;
                    encode   = 0;
                    requestAuth = 1;
                    if (msg) {
                        tokenHdr = String(msg).dup();
                    }

                    return status;
                }

                inline Status authenticate(const char *msg = NULL) {
                    return authenticate(Status::UNAUTHORIZED, msg);
                }

                inline void logout(const char* redirect = NULL) {
                    if (redirect) {
                        redirectUrl = String(redirect).dup();
                    }
                    // do not send token, but send delete cookie if using cookie method
                    sendTok = 0;
                }

                const String& token() const  {
                    return actualToken;
                }

                const Jwt& jwtRef() const {
                    return jwt;
                }

            private:
                Jwt    jwt;
                friend struct JwtAuthorization;
                union {
                    struct {
                        uint8_t sendTok : 1;
                        uint8_t encode : 1;
                        uint8_t requestAuth: 1;
                        uint8_t __u8r5: 5;
                    };
                    uint8_t     _u8;
                } __attribute__((packed));

                String tokenHdr{};
                String actualToken{};
                String redirectUrl{};
                JwtAuthorization *jwtAuth{nullptr};
            };

            void before(Request& req, Response& resp, Context& ctx);

            void after(Request&, http::Response& resp, Context& ctx);

            template <typename __Opts>
            void configure(__Opts& opts) {
                /* configure expiry time */
                expiry = opts.get(sym(expires), 3600);
                String tmp(opts.get(sym(key), String()));
                /* configure key */
                if (tmp) {
                    key = std::move(tmp.dup());
                    trace("jwt key changed to %s", key());
                }

                /* configure authenticate header string */
                tmp = opts.get(sym(realm), String());
                if (tmp) {
                    authenticate = utils::catstr("Bear realm=\"",tmp, "\"");
                }

                /* configure domain */
                tmp = opts.get(sym(domain), String());
                if (tmp) {
                    domain = tmp.dup();
                }

                /* configure domain */
                tmp = opts.get(sym(path), String());
                if (tmp) {
                    path = tmp.dup();
                }

                if (opts.has(sym(jwt_token_use))) {
                    use = opts.get(sym(jwt_token_use), use);
                }
            }

            template <typename... __A>
            inline void setup(__A... args) {
                auto opts = iod::D(args...);
                configure(opts);
            }

            JwtAuthorization()
                : key(rand_8byte_salt()("")),
                  authenticate(String("Bearer").dup())
            {
                idebug("generated secret is %s", key());
            }

        private:
            inline void deleteCookie(Response &resp) {
                Cookie cookie(use.key());
                cookie.value("");
                cookie.domain(domain.peek());
                cookie.path(path.peek());
                cookie.expires(time(NULL)-2);
                resp.cookie(cookie);
            }

            inline void authrequest(Response& resp, const char *msg = "")
            {
                deleteCookie(resp);
                resp.header("WWW-Authenticate", authenticate);
                throw Error::unauthorized(msg);
            }

            uint64_t  expiry{900};
            String    key;
            String    authenticate;
            JwtUse    use;
            // cookie attributes
            String    domain{""};
            String    path{"/"};
        };

        namespace auth {
            template <typename __C>
            static bool getuser(__C& conn, User& user) {
                OBuffer qb(32);
                qb << "SELECT * FROM suildb.users WHERE username=";
                __C::params(qb, 1);

                return (conn(qb)(user.username) >> user);
            }

            template <typename __C>
            static bool hasuser(__C& conn, User& user) {
                size_t users = 0;
                OBuffer qb(32);
                qb << "SELECT COUNT(username) FROM suildb.users WHERE username=";
                __C::params(qb, 1);

                conn(qb)(user.username) >> users;
                return users != 0;
            }

            template <typename __C>
            static bool hasusers(__C& conn) {
                size_t users = 0;
                conn("SELECT COUNT(username) FROM suildb.users")() >> users;
                return users != 0;
            }

            template <typename __C>
            static bool updateuser(__C& conn, User& user) {
                sql::Orm<__C,User> Orm("suildb.users", conn);
                return Orm.update(user);
            }

            template <typename __C>
            static void removeuser(__C& conn, User& user) {
                sql::Orm<__C,User> Orm("suildb.users", conn);
                Orm.remove(user);
            }

            template <typename __C>
            static bool adduser(__C& conn, const String& key, User& user) {
                /* generate random salt for user */
                user.salt   = rand_8byte_salt()(nullptr);
                user.passwd = pbkdf2_sha1_hash(key)(user.passwd, user.salt);
                sql::Orm<__C,User> Orm("suildb.users", conn);
                return Orm.insert(user);
            }

            template <typename __C>
            static bool updateuser(__C& conn, const String& key, User& user) {
                /* generate random salt for user */
                user.salt   = rand_8byte_salt()(nullptr);
                user.passwd = pbkdf2_sha1_hash(key)(user.passwd, user.salt);
                sql::Orm<__C,User> Orm("suildb.users", conn);
                return Orm.update(user);
            }

            template <typename __C>
            static bool seedusers(__C& conn, const String& key, std::vector<User> users) {
                sql::Orm<__C,User> Orm("suildb.users", conn);
                if (hasusers(conn)) {
                    /* table does not exist */
                    swarn("seeding to non-empty table prohibited");
                    return false;
                }

                for (auto& user : users) {
                    user.salt   = rand_8byte_salt()(nullptr);
                    user.passwd = pbkdf2_sha1_hash(key)(user.passwd, user.salt);
                    if ((Orm.insert(user)) != 1) {
                        swarn("seed database table with user '%' failed", user.username());
                        return false;
                    }
                }

                return true;
            }

            template <typename __C>
            static bool seedusers(__C& conn, const String& key, const char *json) {
                decltype(iod::D(prop(data, std::vector<User>))) users;
                String jstr = utils::fs::readall(json);
                if (!jstr) {
                    swarn("reading file %s failed", json);
                    return false;
                }
                iod::json_decode(users, jstr);
                return  seedusers(conn, key, users.data);
            }

            template <typename __C>
            static bool verifyuser(__C& conn, const String& key, User& user) {
                User u;
                u.username = std::move(user.username);
                if (getuser(conn, u)) {
                    /* user exists */
                    String passwd = pbkdf2_sha1_hash(key)(user.passwd, u.salt);
                    if (passwd == u.passwd) {
                        /* user authorized */
                        user = std::move(u);
                        return user.verified && !user.locked;
                    }
                }

                return false;
            }

            template <typename __C>
            String reguser(__C& conn, const String& key, User& user, bool verified = true) {
                if (hasuser(conn, user)) {
                    return utils::catstr("User '", user.username, "' already exists");
                }

                user.verified = verified;
                user.locked = false;
                // add user to database
                if (!adduser(conn, key, user)) {
                    // adding user failed.
                    return "could not register user, contact system admin";
                }

                // user successfully added
                return nullptr;
            }

            template <typename __C, typename __P>
            bool _setuserprops(__C& conn, User& user, __P& props) {
                if (props.has(sym(verified)))
                    user.verified = props.get(sym(verified));
                if (props.has(sym(locked)))
                    user.locked = props.get(sym(locked));

                sql::Orm<__C,User> Orm("users", conn);
                return Orm.update(user);
            }

            template <typename __C, typename... Props>
            inline bool setuserprops(__C& conn, User& user, Props... props) {
                auto opts = iod::D(props...);
                return _setuserprops(conn, user, opts);
            }
        }
    }
}
#endif //SUIL_AUTH_HPP
