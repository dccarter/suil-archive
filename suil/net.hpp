//
// Created by dc on 20/06/17.
//

#ifndef SUIL_NET_HPP
#define SUIL_NET_HPP

#include "sock.hpp"
#include "worker.hpp"

namespace suil {

    struct server_handler {
        void operator()(sock_adaptor& sock, void *) {
            //sinfo("received connection from: %s", sock.id());
            sock.send(version::SWNAME);
            sock.send(" - ");
            sock.send(version::STRING);
            sock.send("\n");
            sock.flush();
        }
    };

    define_log_tag(SERVER);
    struct server_config : public  ssl_ss_config , public tcp_ss_config {
        std::string     name{"127.0.0.1"};
        int             port{1080};
        int             accept_backlog{127};
        int64_t         accept_timeout{-1};
        uint64_t        request_limit{40};
        uint8_t         nworkers{2};
    };

    template <class __H = server_handler, class __A = tcp_ss, class __C = void>
    struct server : LOGGER(dtag(SERVER)) {
        typedef typename __A::sock_t __sock_t;
    public:
        server(server_config& scfg, typename __A::config_t& acfg, __C *ctx)
            : adaptor(acfg),
              context(ctx),
              config(scfg)
        {}

        int run(post_spawn_t hdlr) {
            notice("starting server at %s:%d", config.name.c_str(), config.port);
            ipaddr addr = iplocal(config.name.c_str(), config.port, 0);

            if (!adaptor.listen(addr, config.accept_backlog)) {
                error("listening on adaptor failed: %s", errno_s);
                return errno;
            }

            int status = worker::spawn(config.nworkers,
            [&]() {
                bool ret = false;
                __sock_t s;
                ret = adaptor.accept(s, config.accept_timeout);
                if (!ret) {
                    int status = EXIT_FAILURE;
                    // timeout's are allowed
                    if (errno != ETIMEDOUT) {
                        if (!exiting) {
                            error("accepting next connection failed: %s", errno_s);
                            status = errno;
                        }

                        // cleanup socket
                        adaptor.close();
                        return status;
                    }
                }
                else {
                    go(handle(this, s));
                }

                return 0;
            },
            [&](uint8_t w) {
                worker::register_cleaner([&]() {
                    /* close adaptor to allow the worker to exit */
                    exiting = true;
                    adaptor.shutdown();
                });

                return hdlr(w);
            });

            notice("server exiting, status %d", status);

            return status;
        }

        void init() {
            // verify some configurations
            int ncpus = sysconf(_SC_NPROCESSORS_ONLN);
            if (config.nworkers == 1) {
                // 1 signify using all available CPU's
                config.nworkers = ncpus;
            } else if (config.nworkers == 0) {
                // no workers, just use parent as work
                notice("workers disabled");
            }
        }

    private:

        static coroutine void handle(server<__H, __A, __C> *srv, __sock_t& s) {
            __sock_t ss = std::move(s);
            __H()(ss, srv->context);

            if (ss.isopen()) {
                ss.close();
            }
        }

        __A adaptor;
        __C *context;
        server_config& config;
        bool     running{true};
        bool     has_lock{false};
        bool     exiting{false};
    };

    template <class __H = server_handler>
    using ssl_server = server<__H, ssl_ss>;

    template <class __H = server_handler>
    using tcp_server = server<__H, tcp_ss>;
}
#endif //SUIL_NET_HPP
