//
// Created by dc on 28/06/17.
//

#ifndef SUIL_ENDPOINT_HPP
#define SUIL_ENDPOINT_HPP

#include <suil/symbols.h>
#include <suil/net.hpp>
#include <suil/http/connection.hpp>
#include <suil/http/routing.hpp>

namespace suil {
    namespace http {

        define_log_tag(HTTP_SERVER);

        template<typename __H, typename __B = TcpSs, typename... __Mws>
        struct BaseServer : LOGGER(dtag(HTTP_SERVER)) {
            typedef BaseServer<__H, __B, __Mws...> server_t;

            struct socket_handler {
                void operator()(SocketAdaptor &sock, server_t *s) {
                    Connection<__H, __Mws...> conn(
                            sock, s->config, s->handler, &s->mws, s->stats);

                    conn.start();
                }
            };

            inline void stop() {
                /* stop the backend */
                backend.stop();
            }

            typedef Server<socket_handler, __B, server_t> raw_server_t;
            typedef std::tuple<__Mws...> middlewares_t;

        protected:
            template<typename... __Opts>
            BaseServer(__H &h, __Opts&... opts)
                : backend(config, config, this),
                  handler(h)
            {
                utils::apply_config(config, opts...);
                initialize();
            }

            BaseServer(__H &h)
                    : backend(config, config, this),
                      handler(h)
            {
                initialize();
            }

            inline int run() {
                /* start the backend server */
                return backend.run();
            }

            void initialize() {
                backend.init();

                stats.rx_bytes = 0;
                stats.tx_bytes = 0;
                stats.total_requests = 0;
                stats.open_requests  = 0;
            }

            raw_server_t        backend;
            HttpConfig       config;
            __H&                handler;
            middlewares_t       mws;
            ServerStats      stats;
        };

        #define eproute(app, url) \
            app.route<magic::get_parameter_tag(magic::const_str(url))>(url)

        template <typename __B = TcpSs, typename... __Mws>
        struct Endpoint : public BaseServer<Router, __B, __Mws...> {
            typedef BaseServer<Router, __B, __Mws...> basesrv_t;
            /**
             * creates a new http endpoint which handles http requests
             * @param conf the configuration
             */
            Endpoint(std::string api)
                    : router(api),
                      basesrv_t(router)
            {}

            template <typename... __Opts>
            Endpoint(std::string api, __Opts... opts)
                : router(api),
                  basesrv_t(router, opts...)
            {}

            int start() {

                eproute((*this), "/sys/stats")
                ("GET"_method)
                .attrs(opt(AUTHORIZE, Roles{"System"}))
                ([this] () {
                    return this->stats;
                });

                eproute((*this), "/sys/memory")
                ("GET"_method)
                .attrs(opt(AUTHORIZE, Roles{"System"}))
                ([this] () {
                    return memory::get_usage();
                });

                eproute((*this), "/sys/about")
                ("GET"_method)
                .attrs(opt(AUTHORIZE, Roles{"System"}))
                ([this] () {
                    return SUIL_SOFTWARE_NAME " " SUIL_VERSION_STRING;
                });

                eproute((*this), "/sys/version")
                ("GET"_method)
                .attrs(opt(AUTHORIZE, Roles{"System"}))
                ([this] () {
                    return ver_json;
                });

                router.validate();

                trace("starting server...");

                return basesrv_t::run();
            }

            template <typename __Opts>
            inline void configure(__Opts& opts) {
                /* apply endpoint configuration */
                utils::apply_options(this->config, opts);
            }

            DynamicRule& dynamic(std::string&& rule) {
                return router.new_rule_dynamic(std::move(rule));
            }

            template<uint64_t Tag>
            auto route(std::string&& rule)
            -> typename std::result_of<decltype(&Router::new_rule_tagged<Tag>)(Router, std::string&&)>::type
            {
                return router.new_rule_tagged<Tag>(std::move(rule));
            }

            DynamicRule&  operator()(std::string&& rule)
            {
                return dynamic(std::move(rule));
            }

            using context_t = detail::context<__Mws...>;
            template <typename T>
            typename T::Context& context(const Request& req)
            {
                static_assert(magic::contains<T, __Mws...>::value, "not Middleware in endpoint");
                auto& ctx = *reinterpret_cast<context_t*>(req.middleware_context);
                return ctx.template get<T>();
            }

            template <typename T>
            T& middleware()
            {
                return get_element_by_type<T, __Mws...>(basesrv_t::mws);
            }

        private:
            Router   router;
        };

        namespace opts {
            template<typename... T>
            auto http_endpoint(T &&... s) {
                return sym(http_endpoint) = std::make_tuple(s...);
            }
        }

        template <typename... __Mws>
        using TcpEndpoint = Endpoint<TcpSs, __Mws...>;
        template <typename... __Mws>
        using SslEndpoint = Endpoint<SslSs, __Mws...>;
    }
}

#endif //SUIL_ENDPOINT_HPP
