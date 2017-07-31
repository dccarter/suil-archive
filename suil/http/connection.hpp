//
// Created by dc on 28/06/17.
//

#ifndef SUIL_CONNECTION_HPP
#define SUIL_CONNECTION_HPP

#include "parser.hpp"
#include "wsock.hpp"

namespace suil {
    namespace http {

        namespace detail {

            template <typename ... Middlewares>
            struct partial_context
                    : public magic::pop_back<Middlewares...>::template rebind<partial_context>
                            , public magic::last_element_type<Middlewares...>::type::Context
            {
                using parent_context = typename magic::pop_back<Middlewares...>::template rebind<http::detail::partial_context>;
                    template <int N>
                using partial = typename std::conditional<N == sizeof...(Middlewares)-1, partial_context, typename parent_context::template partial<N>>::type;

                template <typename T>
                typename T::Context& get()
                {
                    return static_cast<typename T::context&>(*this);
                }
            };

            template <>
            struct partial_context<>
            {
                template <int>
                using partial = partial_context;
            };

            template <int N, typename Context, typename Container, typename CurrentMW, typename ... Middlewares>
            bool middleware_call_helper(Container& middlewares, http::request& req, http::response& res, Context& ctx);

            template <typename ... Middlewares>
            struct context : private partial_context<Middlewares...>
                //struct context : private Middlewares::context... // simple but less type-safe
            {
                template <int N, typename Context, typename Container>
                friend typename std::enable_if<(N==0)>::type after_handlers_call_helper(Container& middlewares, Context& ctx, http::request& req, http::response& res);
                template <int N, typename Context, typename Container>
                friend typename std::enable_if<(N>0)>::type after_handlers_call_helper(Container& middlewares, Context& ctx, http::request& req, http::response& res);

                template <int N, typename Context, typename Container, typename CurrentMW, typename ... Middlewares2>
                friend bool middleware_call_helper(Container& middlewares, http::request& req, http::response& res, Context& ctx);

                template <typename T>
                typename T::Context& get()
                {
                    return static_cast<typename T::Context&>(*this);
                }

                template <int N>
                using partial = typename partial_context<Middlewares...>::template partial<N>;
            };

            template<typename MW>
            struct check_before_handle_arity_3_const {
                template<typename T,
                        void (T::*)(http::request &, http::response &, typename MW::Context &) const = &T::before
                >
                struct get {
                };
            };

            template<typename MW>
            struct check_before_handle_arity_3 {
                template<typename T,
                        void (T::*)(http::request &, http::response &, typename MW::Context &) = &T::before
                >
                struct get {
                };
            };

            template<typename MW>
            struct check_after_handle_arity_3_const {
                template<typename T,
                        void (T::*)(http::request &, http::response &, typename MW::Context &) const = &T::after
                >
                struct get {
                };
            };

            template<typename MW>
            struct check_after_handle_arity_3 {
                template<typename T,
                        void (T::*)(http::request &, http::response &, typename MW::Context &) = &T::after
                >
                struct get {
                };
            };

            template<typename T>
            struct is_before_handle_arity_3_impl {
                template<typename C>
                static std::true_type f(typename check_before_handle_arity_3_const<T>::template get<C> *);

                template<typename C>
                static std::true_type f(typename check_before_handle_arity_3<T>::template get<C> *);

                template<typename C>
                static std::false_type f(...);

            public:
                static const bool value = decltype(f<T>(nullptr))::value;
            };

            template<typename T>
            struct is_after_handle_arity_3_impl {
                template<typename C>
                static std::true_type f(typename check_after_handle_arity_3_const<T>::template get<C> *);

                template<typename C>
                static std::true_type f(typename check_after_handle_arity_3<T>::template get<C> *);

                template<typename C>
                static std::false_type f(...);

            public:
                static const bool value = decltype(f<T>(nullptr))
                ::value;
            };

            template<typename MW, typename Context, typename ParentContext>
            typename std::enable_if<!is_before_handle_arity_3_impl<MW>::value>::type
            before_handler_call(MW &mw, http::request &req, http::response &res, Context &ctx, ParentContext & /*parent_ctx*/) {
                mw.before(req, res, ctx.template get<MW>(), ctx);
            }

            template<typename MW, typename Context, typename ParentContext>
            typename std::enable_if<is_before_handle_arity_3_impl<MW>::value>::type
            before_handler_call(MW &mw, http::request &req, http::response &res, Context &ctx, ParentContext & /*parent_ctx*/) {
                mw.before(req, res, ctx.template get<MW>());
            }

            template<typename MW, typename Context, typename ParentContext>
            typename std::enable_if<!is_after_handle_arity_3_impl<MW>::value>::type
            after_handler_call(MW &mw, http::request &req, http::response &res, Context &ctx, ParentContext & /*parent_ctx*/) {
                mw.after(req, res, ctx.template get<MW>(), ctx);
            }

            template<typename MW, typename Context, typename ParentContext>
            typename std::enable_if<is_after_handle_arity_3_impl<MW>::value>::type
            after_handler_call(MW &mw, http::request &req, http::response &res, Context &ctx, ParentContext & /*parent_ctx*/) {
                mw.after(req, res, ctx.template get<MW>());
            }

            template<int N, typename Context, typename Container, typename CurrentMW, typename ... Middlewares>
            bool middleware_call_helper(Container &middlewares, http::request &req, http::response &res, Context &ctx) {
                using parent_context_t = typename Context::template partial<N - 1>;
                before_handler_call<CurrentMW, Context, parent_context_t>(std::get<N>(middlewares), req, res, ctx,
                                                                          static_cast<parent_context_t &>(ctx));

                if (res.iscompleted()) {
                    after_handler_call<CurrentMW, Context, parent_context_t>(std::get<N>(middlewares), req, res,
                                                                             ctx,
                                                                             static_cast<parent_context_t &>(ctx));
                    return true;
                }

                if (middleware_call_helper<N + 1, Context, Container, Middlewares...>(middlewares, req, res, ctx)) {
                    after_handler_call<CurrentMW, Context, parent_context_t>(std::get<N>(middlewares), req, res,
                                                                             ctx,
                                                                             static_cast<parent_context_t &>(ctx));
                    return true;
                }

                return false;
            }

            template<int N, typename Context, typename Container>
            bool middleware_call_helper(Container & /*middlewares*/, http::request & /*req*/, http::response & /*res*/,
                                        Context & /*ctx*/) {
                return false;
            }

            template<int N, typename Context, typename Container>
            typename std::enable_if<(N < 0)>::type
            after_handlers_call_helper(Container & /*middlewares*/, Context & /*context*/, http::request & /*req*/,
                                       http::response& /*res*/) {
            }

            template<int N, typename Context, typename Container>
            typename std::enable_if<(N == 0)>::type
            after_handlers_call_helper(Container &middlewares, Context &ctx, http::request &req, http::response &res) {
                using parent_context_t = typename Context::template partial<N - 1>;
                using CurrentMW = typename std::tuple_element<N, typename std::remove_reference<Container>::type>::type;
                after_handler_call<CurrentMW, Context, parent_context_t>(std::get<N>(middlewares), req, res, ctx,
                                                                         static_cast<parent_context_t &>(ctx));
            }

            template<int N, typename Context, typename Container>
            typename std::enable_if<(N > 0)>::type
            after_handlers_call_helper(Container &middlewares, Context &ctx, http::request &req, http::response &res) {
                using parent_context_t = typename Context::template partial<N - 1>;
                using CurrentMW = typename std::tuple_element<N, typename std::remove_reference<Container>::type>::type;
                after_handler_call<CurrentMW, Context, parent_context_t>(std::get<N>(middlewares), req, res, ctx,
                                                                         static_cast<parent_context_t &>(ctx));
                after_handlers_call_helper<N - 1, Context, Container>(middlewares, ctx, req, res);
            }
        }

#ifndef HTTP_RX_BUFFER_SZ
#define HTTP_RX_BUFFER_SZ   2048
#endif
        define_log_tag(HTTP_CONN);
        template <typename __H, typename... __Mws>
        struct connection : LOGGER(dtag(HTTP_CONN)) {
            typedef std::tuple<__Mws...> middlewares_t;

            connection(sock_adaptor& sock,
                       http_config_t& config,
                       __H& handler,
                       middlewares_t* mws,
                       server_stats_t& stats)
                : mws(mws),
                  config(config),
                  sock(sock),
                  handler(handler),
                  stats(stats)
            {
                stats.total_requests++;
                stats.open_requests++;
                trace("(%p) %s - creating connection", this, sock.id());
            }

            void start() {

                status_t  status = status_t::OK;
                request req(sock, config);

                trace("%s - starting connection handler", sock.id());
                do {
                    // receive request headers
                    status = req.receive_headers(stats);
                    if (status != status_t::OK) {
                        if (status != status_t::REQUEST_TIMEOUT) {
                            // receiving headers failed, send back failure
                            response res(status);
                            send_response(req, res, true);
                        }
                        else {
                            // receiving headers timed out, abort
                            trace("%s - receiving headers timeout", sock.id());
                        }
                        break;
                    }

                    // receive request body
                    status = req.receive_body(stats);
                    if (status != status_t::OK) {
                        // receiving body failed, send back error
                        response res(status);
                        send_response(req, res, true);
                        break;
                    }

                    // handle received request
                    bool err{false};
                    response res(status_t::OK);
                    detail::context<__Mws...> ctx =
                            detail::context<__Mws...>();

                    try {
                        req.middleware_context = (void *) &ctx;
                        // call before handle middleware contexts
                        detail::middleware_call_helper<
                                0,
                                decltype(ctx),
                                decltype(*mws),
                                __Mws...>
                                (*mws, req, res, ctx);

                        if (!res.iscompleted()) {
                            // call the request handler
                            handler.handle(req, res);
                        }

                        // call after handle middleware contexts
                        detail::after_handlers_call_helper<(
                                (int)sizeof...(__Mws)-1),
                                decltype(ctx), decltype(*mws)>
                                (*mws, ctx, req, res);
                    }
                    catch (http::exception& ex) {
                        res.status = ex.status();
                        res.body.reset(0, true);
                        res.body.append(ex.what());
                        err = true;
                    }
                    catch (...) {
                        res.status = status_t::INTERNAL_ERROR;
                        res.body.reset(0, true);
                        err = true;
                    }

                    send_response(req, res, err);
                    if (res.status == status_t::SWITCHING_PROTOCOLS && res()) {
                        // easily switch protocols
                        res()(req, res);
                        close_ = true;
                    }

                    req.clear();
                    res.clear();
                } while (!close_);

                trace("%p - done handling connection, %d", this, close_);
            }

            ~connection() {
                stats.open_requests--;
            }

        private:

            using sendbuf_t = std::vector<response::chunk_t>;

            void send_response(request& req, response& res, bool err = false) {
                if (!sock.isopen()) {
                    close_ = true;
                    trace("%p - send response error: socket closed", this);
                    return;
                }

                sendbuf_t obuf;
                obuf.reserve(4);

                buffer_t hbuf(2047);

                const char *status = status_text(res.status);
                hbuf.append(status);
                hbuf.append("\r\n", 2);
                if (!err) {
                    const strview_t conn = req.header("Connection");
                    if (!conn.empty() && !strcasecmp(conn.data(), "Close")) {
                        // client requested connection be closed
                        close_ = true;
                    }

                    if (config.keep_alive_time && !close_) {
                        // set keep alive time
                        hbuf.append("Connection: Keep-Alive\r\n",
                                    (sizeof("Connection: Keep-Alive\r\n")-1));
                        hbuf.appendnf(30, "Keep-Alive: %lu\r\n",
                                      config.keep_alive_time);
                    }

                    if (config.hsts_enable) {
                        hbuf.appendnf(48,
                                      "Strict-Transport-Security: max-age %lu; includeSubdomains\r\n",
                                      config.hsts_enable);
                    }
                }
                else {
                    // force connection close on error
                    close_ = true;
                }

                if (res.status > status_t::BAD_REQUEST && !res.body) {
                    res.body.append((status+9));
                }
                // flush cookies.
                res.flush_cookies();

                for (auto h : res.headers) {
                    hbuf.appendnf(255, "%s : %s\r\n", h.first.str, h.second.str);
                }

                if (!res.headers.count("Server")) {
                    hbuf.appendnf(96, "Server: %s\r\n", config.server_name.c_str());
                }

                if (!res.headers.count("Date")) {
                    hbuf.appendnf(64, "Date: %s\r\n", get_cached_date());
                }

                if (!res.headers.count("Content-Length")) {
                    hbuf.appendnf(32, "Content-Length: %lu\r\n", res.length());
                }

                hbuf.append("\r\n", 2);
                obuf.emplace_back(hbuf.data(), hbuf.size());

                if (res.body) {
                    obuf.emplace_back(res.body.data(), res.body.size());
                } else if (!res.chunks.empty()){
                    for (auto ch : res.chunks) {
                        obuf.push_back(ch);
                    }
                }

                if (!write_response(obuf)) {
                    warn("(%p:%s) - sending data to socket failed: %s",
                         this, sock.isopen(), errno_s);
                    close_ = true;
                    res.clear();
                }

                obuf.clear();
            }

            bool write_response(sendbuf_t& buf) {
                bool rc{false};
                for (auto b : buf) {
                    if (b.use_fd) {
                        // send file descriptor
                        size_t nsent = 0;
                        size_t chunk;
                        do {
                            chunk = std::min(config.send_chunk, b.len - nsent);
                            rc = sock.sendfile(b.fd, (b.offset + nsent), chunk,
                                               config.connection_timeout);
                            if (rc < 0 || rc != chunk) {
                                trace("(%p) -  sending response failed: %s", errno_s);
                                return false;
                            }

                            nsent += chunk;
                        } while (nsent < b.len);
                    }
                    else {
                        // for buffers we need to chunk the data
                        size_t nsent = 0;
                        size_t chunk;
                        do {
                            chunk = std::min(config.send_chunk, b.len-nsent);
                            rc = sock.send(((char *)b.data + b.offset + nsent),
                                            chunk,
                                            config.connection_timeout);
                            if (!rc) {
                                trace("(%p) sending response failed: %s", errno_s);
                                return false;
                            }

                            nsent += chunk;
                        } while (nsent < b.len);
                    }

                    // update server statistics
                    stats.tx_bytes += b.len;
                }

                sock.flush(config.connection_timeout);
                return true;
            }

            static const char* get_cached_date() {
                static int64_t rec = 0;
                int64_t point = mnow();
                static char buf[64];
                if ((point-rec) > 1000) {
                    rec = point;
                    datetime()(buf, 64, datetime::HTTP_FMT);
                }
                return buf;
            }

            middlewares_t       *mws;
            http_config_t&      config;
            sock_adaptor&       sock;
            __H&                handler;
            server_stats_t&     stats;
            bool                close_{false};
        };
    }
}
#endif //SUIL_CONNECTION_HPP
