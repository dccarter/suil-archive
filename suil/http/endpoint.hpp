//
// Created by dc on 28/06/17.
//

#ifndef SUIL_ENDPOINT_HPP
#define SUIL_ENDPOINT_HPP

#include "symbols.h"
#include "connection.hpp"
#include "routing.hpp"
#include "net.hpp"

namespace suil {
    namespace http {

        define_log_tag(HTTP_SERVER);

        template<typename __H, typename __B = tcp_ss, typename... __Mws>
        struct base_server : LOGGER(dtag(HTTP_SERVER)) {
            typedef base_server<__H, __B, __Mws...> server_t;

            struct socket_handler {
                void operator()(sock_adaptor &sock, server_t *s) {
                    connection<__H, __Mws...> conn(
                            sock, s->config, s->handler, &s->mws, s->stats);

                    conn.start();
                }
            };

            typedef server<socket_handler, __B, server_t> raw_server_t;
            typedef std::tuple<__Mws...> middlewares_t;

        protected:
            template<typename... __Opts>
            base_server(__H &h, __Opts &... opts)
                : backend(config, config, this),
                  handler(h)
            {
                utils::apply_config(config, opts...);
                initialize();
            }

            int run() {
                return backend.run([&](pid_t pid){
                    stats.pid = (uint32_t) pid;
                    // add web socket ipc apis
                    websock_api::setup_ipc();

                    setup_ipc();

                    return EXIT_SUCCESS;
                });
            }

            void initialize() {
                backend.init();

                stats.rx_bytes = 0;
                stats.tx_bytes = 0;
                stats.total_requests = 0;
                stats.open_requests  = 0;
            }

            void setup_ipc() {

                worker::ipcreg(GET_STATS,
                [&](uint8_t src, const void *data, size_t len) {
                    trace("GET_STATS src %hhu, data %p, len %lu", src, data, len);

                    auto jstats = iod::json_encode(stats);
                    worker::send_get_response(data, src, jstats.data(), jstats.size());
                    return false;
                });

                worker::ipcreg(GET_MEMORY_INFO,
                [&](uint8_t src, const void *data, size_t len) {
                    trace("GET_MEMORY_INFO src %hhu, data %p, len %lu", src, data, len);

                    auto jinfo = iod::json_encode(memory::get_usage());
                    worker::send_get_response(data, src, jinfo.data(), jinfo.size());
                    return false;
                });
            }

            raw_server_t        backend;
            http_config_t       config;
            __H&                handler;
            middlewares_t       mws;
            server_stats_t      stats;
        };

        #define eproute(app, url) \
            app.route<magic::get_parameter_tag(magic::const_str(url))>(url)

        template <typename __B = tcp_ss, typename... __Mws>
        struct endpoint_t : public base_server<router_t, __B, __Mws...> {
            typedef base_server<router_t, __B, __Mws...> basesrv_t;
            /**
             * creates a new http endpoint which handles http requests
             * @param conf the configuration
             */
            template <typename... __Opts>
            endpoint_t(__Opts... opts)
                : basesrv_t(router, opts...)
            {}

            int start() {
                eproute((*this), "/admin/stats/<uint>")
                ("GET"_method)
                ([this](const request& /*req*/, response& resp, uint32_t w) {
                    if ((w > this->config.nworkers) || (w == 0)) {
                        /* invalid worker */
                        throw  error::not_found();
                    }

                    if (w != spid) {
                        auto nb = std::move(worker::get(GET_STATS, (uint8_t) w, this->config.connection_timeout));
                        if (nb) {
                            /* network buffer valid */
                            resp.append(nb.get(), nb.size());
                            resp.set_content_type("application/json");
                        } else {
                            throw error::bad_request();
                        }
                    } else {
                        resp.append(iod::json_encode(this->stats));
                        resp.set_content_type("application/json");
                    }
                });

                eproute((*this), "/admin/stats")
                ("GET"_method)
                ([this](const request& /*req*/, response& resp) {
                    if (this->config.nworkers) {
                        resp.append("[");
                    }

                    resp.append(iod::json_encode(this->stats));

                    if (this->config.nworkers) {
                        std::vector<network_buffer> netbufs =
                                std::move(worker::gather(GET_STATS, this->config.connection_timeout));
                        for (auto &nb : netbufs) {
                            /* append buffer */
                            resp.append(",", 1);
                            resp.append(nb.get(), nb.size());
                        }

                        resp.append("]");
                    }

                    resp.set_content_type("application/json");
                });

                eproute((*this), "/admin/memory/info")
                ("GET"_method)
                ([this](const request& /*req*/, response& resp){
                    if (this->config.nworkers) {
                        resp.append("[");
                    }

                    resp.append(iod::json_encode(memory::get_usage()));

                    if (this->config.nworkers) {
                        std::vector<network_buffer> netbufs =
                                std::move(worker::gather(GET_MEMORY_INFO, this->config.connection_timeout));
                        for (auto &nb : netbufs) {
                            /* append buffer */
                            resp.append(",", 1);
                            resp.append(nb.get(), nb.size());
                        }

                        resp.append("]");
                    }

                    resp.set_content_type("application/json");
                });

                eproute((*this), "/api/v1")
                ("GET"_method)
                ([this](){
                    return SUIL_SOFTWARE_NAME " " SUIL_VERSION_STRING;
                });

                eproute((*this), "/api/v2")
                ("GET"_method)
                ([this](){
                    return ver_json;
                });

                router.validate();

                notice("starting server...");

                return basesrv_t::run();
            }

            dynamic_rule& dynamic(std::string&& rule) {
                return router.new_rule_dynamic(std::move(rule));
            }

            template<uint64_t Tag>
            auto route(std::string&& rule)
            -> typename std::result_of<decltype(&router_t::new_rule_tagged<Tag>)(router_t, std::string&&)>::type
            {
                return router.new_rule_tagged<Tag>(std::move(rule));
            }

            dynamic_rule&  operator()(std::string&& rule)
            {
                return dynamic(std::move(rule));
            }

            using context_t = detail::context<__Mws...>;
            template <typename T>
            typename T::Context& context(const request& req)
            {
                static_assert(magic::contains<T, __Mws...>::value, "not middleware in endpoint");
                auto& ctx = *reinterpret_cast<context_t*>(req.middleware_context);
                return ctx.template get<T>();
            }

            template <typename T>
            T& middleware()
            {
                return get_element_by_type<T, __Mws...>(basesrv_t::mws);
            }

        private:
            router_t   router;
        };

        template <typename... __Mws>
        using endpoint = endpoint_t<tcp_ss, __Mws...>;
        template <typename... __Mws>
        using ssl_endpoint = endpoint_t<ssl_ss, __Mws...>;
    }
}

#endif //SUIL_ENDPOINT_HPP
