//
// Created by dc on 20/06/17.
//

#ifndef SUIL_NET_HPP
#define SUIL_NET_HPP

#include <suil/sock.hpp>
#include <suil/worker.hpp>

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

        int run() {
            notice("starting http server at %s:%d", config.name.c_str(), config.port);
            ipaddr addr = iplocal(config.name.c_str(), config.port, 0);

            if (!adaptor.listen(addr, config.accept_backlog)) {
                error("listening on adaptor failed: %s", errno_s);
                return errno;
            }

            int  status = EXIT_SUCCESS;

            while (!exiting) {
                /* accept connections until adaptor is closed */
                __sock_t s;
                int ret = adaptor.accept(s, config.accept_timeout);
                if (!ret) {
                    // timeout's are allowed
                    if (errno != ETIMEDOUT) {
                        if (!exiting) {
                            error("accepting next connection failed: %s", errno_s);
                            status = errno;
                        }

                        // cleanup socket
                        adaptor.close();
                        break;
                    }
                } else {
                    go(handle(this, s));
                }
            }

            notice("server exiting...");

            return status;
        }

        void init() {
            // TODO verify some configurations
        }

        inline void stop() {
            /* set the exiting flag and close the adaptor */
            debug("stopping server...");
            exiting = true;
            adaptor.shutdown();
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
