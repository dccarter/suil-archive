//
// Created by dc on 20/06/17.
//

#ifndef SUIL_NET_HPP
#define SUIL_NET_HPP

#include <suil/sock.h>

namespace suil {

    struct server_handler {
        void operator()(SocketAdaptor& sock, void *);
    };

    define_log_tag(SERVER);
    struct ServerConfig : public  SslSsConfig , public TcpSsConfig {
        std::string     name{"127.0.0.1"};
        int             port{1080};
        int             accept_backlog{127};
        int64_t         accept_timeout{-1};
        uint64_t        request_limit{40};
    };

    template <class __H = server_handler, class __A = TcpSs, class __C = void>
    struct Server : LOGGER(SERVER) {
        typedef typename __A::sock_t __sock_t;
    public:
        Server(ServerConfig& scfg, typename __A::config_t& acfg, __C *ctx)
            : adaptor(acfg),
              context(ctx),
              config(scfg)
        {}

        int listen() {
            if (listening) {
                // server already listening
                return 0;
            }

            ipaddr addr = iplocal(config.name.c_str(), config.port, 0);

            if (!adaptor.listen(addr, config.accept_backlog)) {
                ierror("listening on adaptor failed: %s", errno_s);
                return errno;
            }

            listening = true;
            inotice("socket server listening at %s:%d", config.name.c_str(), config.port);
            return 0;
        }

        int run() {
            int  status = EXIT_SUCCESS;
            if (!listening && (status = listen()))
                return status;

            while (!exiting) {
                /* accept connections until adaptor is closed */
                __sock_t s;
                int ret = adaptor.accept(s, config.accept_timeout);
                if (!ret) {
                    // timeout's are allowed
                    if (errno != ETIMEDOUT) {
                        if (!exiting) {
                            ierror("accepting next Connection failed: %s", errno_s);
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

            idebug("socket server exiting...");

            return status;
        }

        void init() {
            // TODO verify some configurations
        }

        inline void stop() {
            /* set the exiting flag and close the adaptor */
            idebug("stopping server...");
            exiting = true;
            adaptor.shutdown();
            listening = false;
        }

    private:

        static coroutine void handle(Server<__H, __A, __C> *srv, __sock_t& s) {
            __sock_t ss = std::move(s);
            __H()(ss, srv->context);

            if (ss.isopen()) {
                ss.close();
            }
        }

        __A adaptor;
        __C *context;
        ServerConfig& config;
        bool     running{true};
        bool     has_lock{false};
        bool     exiting{false};
        bool     listening{false};
    };

    template <class __H = server_handler>
    using SslServer = Server<__H, SslSs>;

    template <class __H = server_handler>
    using TcpServer = Server<__H, TcpSs>;
}
#endif //SUIL_NET_HPP
