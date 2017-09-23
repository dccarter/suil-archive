//
// Created by dc on 8/31/17.
//

#ifndef SUIL_UNIXSOCK_HPP
#define SUIL_UNIXSOCK_HPP

#include "app.hpp"

namespace suil {

    define_log_tag(UNIX_SOCK);
    template <typename __Proto>
    struct unix_sock_server : public apptask, LOGGER(dtag(UNIX_SOCK)) {
        unix_sock_server(const char *path)
            : apptask("unix_sock_server"),
              path(zcstring(path).dup())
        {}

        virtual int  start() {
            master = unixlisten(path.cstr, 32);
            if (master == nullptr) {
                /* open listener socket failed */
                error("opening unix socket '%s' failed: %s",
                        path.cstr, errno_s);

                return errno;
            }

            info("accepting connections on unix://'%s'", path.cstr);

            int rc = 0;
            unixsock sock{nullptr};
            while (!stopped) {
                /* accept, waiting until stopped */
                sock = unixaccept(master, -1);
                if (sock == nullptr) {
                    /* accepting connection failed */
                    if (!stopping) {
                        /* error since stop was not requested */
                        error("error accepting unix connection: %s", errno_s);
                    }
                    else {
                        /* stop was requested */
                        trace("stop was requested: %s", errno_s);
                    }
                    rc = errno;
                    break;
                }
                /* go handle the accepted connection */
                go(handle(*this, sock));
                sock = nullptr;
            }

            master = nullptr;
            stopped = true;
            info("accepting unix connections stopped {rc:%d}", rc);
            return rc;
        }

        virtual void stop() {
            if (stopping || stopped) {
                debug("stop request on ipc master '%s' already in progress",
                            path.cstr);
                return;
            }
            stopping = true;
            if (master) {
                /* shutdown the master unix socket */
                unixshutdown(master);
                master = nullptr;
            }
        }

    private:
        static coroutine void handle(unix_sock_server<__Proto>& m, unixsock sock) {
            ltrace(&m, "starting handling connection: %p", sock);
            __Proto(m)(sock);
            ltrace(&m, "done handling connection: %p", sock);
            /* closing this socket won't harm */
            unixclose(sock);
        }
        zcstring   path;
        unixsock   master;
        bool       stopped{false};
        bool       stopping{false};
    };

    template <typename __Proto>
    struct unix_sock_client: public apptask, LOGGER(dtag(UNIX_SOCK)) {
        unix_sock_client(const char *path)
            : apptask("unix_sock_client"),
              master(zcstring(path).dup())
        {}

        virtual int start() {
            /* connect to server */
            sock = unixconnect(master.cstr);
            if (sock == nullptr) {
                /* connecting to server failed */
                error("connecting to unix server '%s' failed: %s",
                      master.cstr, errno_s);

                return errno;
            }

            stopping = false;
            /* create and start protocol */
            info("client %p connected to '%s'", sock, master.cstr);
            int rc = proto(sock);
            info("client %p disconnecting from '%s'", sock, master.cstr);

            sock = nullptr;
            return rc;
        }

        virtual void stop() {
            if (stopping) {
                /* in progress */
                trace("stop already requested");
                return;
            }

            stopping = true;
            if (sock) {
                unixclose(sock);
                sock = nullptr;
            }
        }

    public:
        unixsock    sock;
        zcstring    master;
        __Proto     proto;
        bool        stopping{false};
    };
}
#endif //SUIL_UNIXSOCK_HPP
