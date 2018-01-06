//
// Created by dc on 01/06/17.
//

#ifndef SUIL_SOCK_HPP
#define SUIL_SOCK_HPP

#include <suil/sys.hpp>
#include <suil/log.hpp>

namespace suil {

    inline const char* ipstr(ipaddr addr, char *buf = nullptr) {
        char *ret = buf;
        if (buf == nullptr) {
            static char s_buf[IPADDR_MAXSTRLEN] = {0};
            ret = s_buf;
        }
        return ipaddrstr(addr, ret);
    }

    struct SocketAdaptor {
        virtual bool connect(ipaddr, int64_t timeout = -1) = 0;
        virtual int port() const  = 0;
        virtual const ipaddr addr() const = 0;
        virtual size_t send(const void*, size_t, int64_t timeout = -1) = 0;
        virtual size_t send(const zcstring &str, int64_t timeout = -1) {
            return send(str.data(), str.size(), timeout);
        }
        virtual size_t sendfile(int, off_t, size_t, int64_t timeout = -1) = 0;
        virtual bool flush(int64_t timeout = -1) = 0;
        virtual bool receive(void*,
                             size_t&,
                             int64_t timeout = -1) = 0;
        virtual bool read(void*,
                          size_t&,
                          int64_t timeout = -1) = 0;
        virtual bool receiveuntil(void*,
                                  size_t&,
                                  const char*,
                                  size_t,
                                  int64_t timeout = -1) = 0;
        virtual bool isopen() const  = 0;
        virtual void close() = 0;

        const char *id() {
            if (id_ == nullptr) {
                id_ = (char *) memory::alloc(IPADDR_MAXSTRLEN+8);
                snprintf(id_, IPADDR_MAXSTRLEN+8, "%s::%d",
                         ipstr(addr()), port());
            }

            return id_;
        }

        virtual ~SocketAdaptor() {
            if (id_) {
                memory::free(id_);
                id_ = nullptr;
            }
        }

    private:
        char    *id_{nullptr};
    };

    define_log_tag(SSL_SOCK);
    struct SslSock : public virtual SocketAdaptor, LOGGER(dtag(SSL_SOCK)) {
        SslSock()
            : raw(nullptr)
        {}

        SslSock(sslsock s)
            : raw(s)
        {}

        SslSock(SslSock &&other)
        : raw(other.raw)
        {
            other.raw = nullptr;
        }

        SslSock &operator=(SslSock &&other) {
            raw = other.raw;
            other.raw = nullptr;
            return *this;
        }

        virtual int port() const {
            if (raw) return sslport(raw);
            return -1;
        }

        virtual const ipaddr addr() const {
            if (raw) return ssladdr(raw);
            return ipaddr {};
        }

        virtual bool connect(ipaddr addr, int64_t timeout = -1) {
            if (isopen()) {
                iwarn("attempting connect on an open socket");
                return false;
            }

            raw = sslconnect(addr, utils::after(timeout));
            if (raw == nullptr) {
                trace("connetion to address %s failed: %s",
                    ipstr(addr), errno_s);

                return false;
            }

            return true;
        }

        virtual size_t send(const void *buf, size_t len, int64_t timeout = -1) {
            if (!isopen()) {
                iwarn("writing to a closed socket not supported");
                errno = ENOTSUP;
                return 0;
            }

            size_t ns = sslsend(raw, buf, (int)len, utils::after(timeout));
            if (errno != 0) {
                trace("send error: %s", errno_s);
                if (errno == ECONNRESET) {
                    close();
                }
                return 0;
            }

            return ns;
        }

        virtual size_t sendfile(int fd, off_t offset, size_t len, int64_t timeout = -1) {
            assert(!"Unsupported operation");
            return 0;
        }

        virtual bool flush(int64_t timeout = -1) {
            if (!isopen()) {
                iwarn("writing to a closed socket not supported");
                errno = ENOTSUP;
                return false;
            }

            sslflush(raw, utils::after(timeout));
            if (errno != 0) {
                iwarn("flushing socket failed: %s", errno_s);
                if (errno == ECONNRESET) {
                    // close the socket
                    close();
                }
                return false;
            }

            return true;
        }

        virtual bool receive(void *buf, size_t &len, int64_t timeout = -1) {
            if (!isopen()) {
                iwarn("writing to a closed socket not supported");
                errno = ENOTSUP;
                return false;
            }

            len = sslrecv(raw, buf, (int) len, utils::after(timeout));
            if (errno != 0) {
                trace("receiving failed: %s", errno_s);
                if (errno == ECONNRESET) {
                    // close the socket
                    close();
                }
                return false;
            }

            return true;
        }

        virtual bool receiveuntil(void *buf, size_t &len, const char *delims,
                                  size_t ndelims, int64_t timeout = -1)
        {
            if (!isopen()) {
                iwarn("writing to a closed socket not supported");
                errno = ENOTSUP;
                return false;
            }

            len = sslrecvuntil(raw, buf, len, delims, ndelims, utils::after(timeout));
            if (errno != 0) {
                trace("receiving failed: %s", errno_s);
                if (errno == ECONNRESET) {
                    // close the socket
                    close();
                }
                return false;
            }

            return true;
        }

        virtual bool read(void *buf, size_t &len, int64_t timeout = -1) {
            if (!isopen()) {
                iwarn("writing to a closed socket not supported");
                errno = ENOTSUP;
                return false;
            }

            len = sslrecv(raw, buf, len, utils::after(timeout));
            if (errno != 0) {
                trace("receiving failed: %s", errno_s);
                if (errno == ECONNRESET) {
                    // close the socket
                    close();
                }
                return false;
            }

            return true;
        }

        virtual bool isopen() const {
            return raw != nullptr;
        }

        virtual void close() {
            if (isopen()) {
                flush(500);
                sslclose(raw);
                raw = nullptr;
            }
        }

        virtual ~SslSock() {
            if (raw)
                close();
        }

        sslsock raw;
    };

    define_log_tag(TCP_SOCKET);
    struct TcpSock : public virtual SocketAdaptor, LOGGER(dtag(TCP_SOCKET)) {
        TcpSock()
            : raw(nullptr)
        {}

        TcpSock(tcpsock s)
            : raw(s)
        {}

        TcpSock(TcpSock &&other)
            : raw(other.raw)
        {
            other.raw = nullptr;
        }

        TcpSock &operator=(TcpSock &&other) {
            raw = other.raw;
            other.raw = nullptr;
            return *this;
        }

        virtual int port() const {
            if (raw) return tcpport(raw);
            return -1;
        }

        virtual const ipaddr addr() const {
            if (raw) return tcpaddr(raw);
            return ipaddr{};
        }

        virtual bool connect(ipaddr addr, int64_t timeout = -1) {
            if (isopen()) {
                iwarn("attempting connect on an open socket");
                return false;
            }

            raw = tcpconnect(addr, utils::after(timeout));
            if (raw == NULL) {
                trace("Connection to address %s failed: %s", ipstr(addr), errno_s);
                return false;
            }
            return true;
        }

        virtual size_t send(const void *buf, size_t len, int64_t timeout = -1){
            if (!isopen()) {
                trace("writing to a closed socket not supported");
                errno = ENOTSUP;
                return 0;
            } else {
                size_t  ns = tcpsend(raw, buf, len, utils::after(timeout));
                if (errno != 0) {
                    trace("sending failed: %s", errno_s);
                    if (errno == ECONNRESET) {
                        close();
                    }
                    return 0;
                }

                return ns;
            }
        }

        virtual size_t sendfile(int fd, off_t offset, size_t len, int64_t timeout = -1) {
            if (!isopen()) {
                trace("writing to a closed socket not supported");
                errno = ENOTSUP;
                return 0;
            } else {
                size_t ns = tcpsendfile(raw, fd, offset, len, utils::after(timeout));
                if (errno != 0) {
                    trace("sending failed: %s", errno_s);
                    if (errno == ECONNRESET)
                        close();

                    return 0;
                }

                return ns;
            }
        }

        virtual bool flush(int64_t timeout = -1) {
            if (!isopen())
                return false;

            tcpflush(raw, utils::after(timeout));
            if (errno != 0) {
                trace("flushing socket failed: %s", errno_s);
                if (errno == ECONNRESET)
                    close();
                return false;
            }

            return true;

        }

        virtual bool receive(void *buf, size_t &len, int64_t timeout = -1) {
            if (!isopen()) {
                iwarn("receiving from a closed socket not supported");
                errno = ENOTSUP;
                len = 0;
                return false;
            }

            len = tcprecv(raw, buf, len, utils::after(timeout));
            if (errno != 0) {
                trace("receiving failed: %s", errno_s);
                if (errno == ECONNRESET)
                    close();

                return false;
            }

            return true;
        }

        virtual bool receiveuntil(void *buf, size_t &len, const char *delims,
                                  size_t ndelims, int64_t timeout = -1)
        {
            if (!isopen()) {
                iwarn("receiving from a closed socket not supported");
                errno = ENOTSUP;
                len = 0;
                return false;
            }
            len = tcprecvuntil(raw, buf, len, delims, ndelims, utils::after(timeout));
            if (errno != 0) {
                trace("receiving failed: %s", errno_s);
                if (errno == ECONNRESET)
                    close();

                return false;
            }

            return true;
        }

        virtual bool read(void *buf, size_t &len, int64_t timeout = -1) {
            if (!isopen()) {
                iwarn("reading from a closed socket not supported");
                errno = ENOTSUP;
                len = 0;
                return false;
            }

            len = tcpread(raw, buf, len, utils::after(timeout));
            if (errno != 0) {
                trace("reading failed: %s",errno_s);
                if (errno == ECONNRESET)
                    close();

                return false;
            }

            return true;
        }

        virtual bool isopen() const {
            return raw != nullptr;
        }

        virtual void close() {
            if (raw != nullptr) {
                tcpclose(raw);
                raw = nullptr;
            }
        }

        virtual ~TcpSock() {
            if (raw)
                close();
        }

        tcpsock raw;
    };

    template <class __S>
    struct ServerSock {
        virtual bool listen(ipaddr, int) = 0;
        virtual bool accept(__S&, int64_t timeout = -1) = 0;
        virtual void close() = 0;
        virtual void shutdown() = 0;
    };

    struct SslSsConfig {
        std::string     key;
        std::string     cert;
    };

    struct SslSs : public ServerSock<SslSock>, LOGGER(dtag(SSL_SOCK)) {
        typedef SslSock sock_t;
        typedef SslSsConfig config_t;

        SslSs(SslSsConfig& cfg)
            :config(cfg)
        {}

        virtual bool listen(ipaddr addr, int backlog) {
            if (raw != nullptr) {
                iwarn("server socket already listening");
                errno = EINPROGRESS;
                return false;
            }

            raw = ssllisten(addr, config.key.c_str(),
                             config.cert.c_str(), backlog);

            if (raw == nullptr) {
                iwarn("listening failed: %s", errno_s);
                return false;
            }
            return true;
        }

        virtual bool accept(sock_t& s, int64_t timeout = -1) {
            sslsock tsock;
            tsock = sslaccept(raw, utils::after(timeout));
            if (tsock == nullptr) {
                trace("accept Connection failed: %s", errno_s);
                s = sock_t(nullptr);
                return false;
            } else {
                s = sock_t(tsock);
                return true;
            }
        }

        virtual void close() {
            if (raw) {
                sslclose(raw);
                raw = nullptr;
            }
        }

        virtual void shutdown()
        {}

    private:
        sslsock         raw{nullptr};
        SslSsConfig& config;
    };

    struct TcpSsConfig {
    };

    struct TcpSs : public ServerSock<TcpSock>, public LOGGER(dtag(TCP_SOCKET)) {
        typedef TcpSock sock_t;
        typedef TcpSsConfig config_t;

        TcpSs(TcpSsConfig /* Unused */)
            : raw(nullptr)
        {}

        virtual bool listen(ipaddr addr, int backlog) {
            if (raw != nullptr) {
                iwarn("server socket already listening");
                errno = EINPROGRESS;
                return false;
            }

            raw = tcplisten(addr, backlog);
            if (raw == nullptr) {
                iwarn("listening failed: %s", errno_s);
                return false;
            }
            return true;
        }

        virtual bool accept(sock_t& s, int64_t timeout = -1) {
            tcpsock tsock;
            tsock = tcpaccept(raw, utils::after(timeout));
            if (tsock == nullptr) {
                trace("accept Connection failed: %s", errno_s);
                s = sock_t(nullptr);
                return false;
            } else {
                s = sock_t(tsock);
                return true;
            }
        }

        virtual void close() {
            if (raw) {
                tcpclose(raw);
                raw = nullptr;
            }
        }

        virtual void shutdown() {
            if (raw) {
                tcpshutdown(raw, 2);
            }
        }

    private:
        tcpsock      raw;
    };
}
#endif //SUIL_SOCK_HPP
