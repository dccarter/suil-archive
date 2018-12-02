//
// Created by dc on 29/11/18.
//

#ifndef SUIL_MIDDLEWARES_H
#define SUIL_MIDDLEWARES_H

#include <suil/redis.h>
#include <suil/channel.h>
#include <suil/http/auth.h>

namespace suil::http::mw {

    define_log_tag(REDIS_MW);

    template <typename Proto>
    struct _Redis : LOGGER(REDIS_MW) {
        using Protocol = Proto;
        using Db = redis::RedisDb<Proto>;
        using Connection = redis::Client<Proto>;

        struct Context {

            Connection& conn(int db = 0) {
                if (self == nullptr) {
                    /* shouldn't happen if used correctly */
                    throw Exception::create("Redis middleware context should be accessed by routes or any middlewares after Redis");
                }
                auto it = mConns.find(db);
                if (it != mConns.end()) {
                    /* open connection */
                    return it->second;
                }
                else {
                    /* get and cache new connection */
                    auto tmp = mConns.emplace(db, std::move(self->conn()));
                    return tmp.first->second;
                }
            }

        private:
            template <typename P>
            friend struct _Redis;
            std::map<int, Connection> mConns{};
            _Redis<Proto>             *self{nullptr};
        };

        void before(http::Request&, http::Response&, Context& ctx) {
            /* we just initialize context here, we don't create any connections */
            ctx.self = this;
        }

        void after(http::Request& req, http::Response&, Context& ctx) {
            /* clear all cached connections for given request */
            trace("clearing {%lu} redis connections for request %s:%d",
                    ctx.mConns.size(), req.ip(), req.port());
            ctx.mConns.clear();
            ctx.self = nullptr;
        }

        template <typename Opts>
        void configure(const char *host, int port, Opts& opts) {
            database.configure(host, port, opts);
        }

        template <typename... Opts>
        void setup(const char *host, int port, Opts... opts) {
            auto options = iod::D(std::forward<Opts>(opts)...);
            configure(host, port, options);
        }

    private:
        inline Connection conn(int db = 0) {
            trace("connecting to database %d", db);
            return database.connect(db);
        }

        Db database{};
    };

    using RedisSsl = _Redis<SslSock>;
    using Redis    = _Redis<TcpSock>;

    template <typename Proto=TcpSock>
    struct _JwtSession {

        using RedisContext =  typename http::mw::_Redis<Proto>::Context;

        struct Context {
            bool authorize(http::Jwt&& jwt) {
                /* pass token to JWT authorization middleware */
                jwtContext->authorize(std::move(jwt));
                /* store generated token in database */
                auto& conn = redisContext->conn(0);
                if (!conn.hset(self->hashName.peek(), jwtContext->jwtRef().aud().peek(), jwtContext->token().peek())) {
                    /* saving token fail */
                    jwtContext->authenticate("Creating session failed");
                    return false;
                }

                return true;
            }

            bool authorize(const String& user) {
                /* fetch token and try authorizing the token */
                auto& conn = redisContext->conn(0);
                if (!conn.hexists(self->hashName.peek(), user.peek())) {
                    /* session does not exist */
                    return false;
                }

                auto token = conn.template hget<String>(self->hashName.peek(), user.peek());
                if (!jwtContext->authorize(token)) {
                    /* token invalid, revoke the token */
                    Ego.revoke(user);
                    return false;
                }

                return true;
            }

            inline void revoke(const http::Jwt& jwt) {
                Ego.revoke(jwt.aud());
            }

            void revoke(const String& user) {
                /* revoke token for given user */
                auto& conn = redisContext->conn(0);
                conn.hdel(self->hashName.peek(), user.peek());
                jwtContext->logout();
            }

        private:
            friend struct _JwtSession;
            http::JwtAuthorization::Context*  jwtContext{nullptr};
            RedisContext* redisContext{nullptr};
            _JwtSession<Proto>*               self{nullptr};
        };

        template <typename Contexts>
        void before(http::Request& req, http::Response& resp, Context& ctx, Contexts& ctxs) {
            /* Only applicable on authorized routes with tokens */
            ctx.self = this;
            http::JwtAuthorization::Context& jwtContext = ctxs.template get<http::JwtAuthorization>();
            RedisContext& redisContext = ctxs.template get<http::mw::_Redis<Proto>>();
            ctx.jwtContext   = &jwtContext;
            ctx.redisContext = &redisContext;

            if (!jwtContext.token().empty()) {
                /* request has authorization token, ensure that is still valid */
                auto aud = jwtContext.jwtRef().aud().peek();
                auto& conn = redisContext.conn(0);
                if (!conn.hexists(hashName.peek(), aud.peek())) {
                    /* token does not exist */
                    jwtContext.authenticate("Attempt to access protected resource with invalid token");
                    resp.end();
                    return;
                }

                auto cachedToken = conn.template hget<String>(hashName.peek(), aud.peek());
                if (cachedToken != jwtContext.token()) {
                    /* Token bad or token has been revoked */
                    jwtContext.authenticate("Attempt to access protected resource with invalid token");
                    resp.end();
                    return;
                }
                ctx.jwtContext   = &jwtContext;
                ctx.redisContext = &redisContext;
            }
        }

        void after(http::Request& req, http::Response&, Context& ctx) {
            /* also only applicable when the */
        }

    private:
        String hashName{"session_tokens"};
    };

    using JwtSession    = _JwtSession<TcpSock>;
    using JwtSessionSsl = _JwtSession<SslSock>;
}

#endif //SUIL_MIDDLEWARES_H
