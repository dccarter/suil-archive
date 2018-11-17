//
// Created by dc on 01/06/17.
//

#ifndef SUIL_SOCK_HPP
#define SUIL_SOCK_HPP

#include <suil/utils.h>
#include <suil/logging.h>

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
        virtual size_t send(const String &str, int64_t timeout = -1) {
            return send(str.data(), str.size(), timeout);
        }

        virtual size_t sendf(int64_t timeout, const char* fmt, ...) {
            va_list args;
            va_start(args, fmt);
            size_t ret = sendv(timeout, fmt, args);
            va_end(args);
            return ret;
        }

        virtual size_t sendv(int64_t timeout, const char* fmt, va_list args) {
            char buf[1024] = {0};
            ssize_t sz = vsnprintf(buf, sizeof(buf)-1, fmt, args);
            if (sz < 0) {
                // buffer larger than 1024
                errno = ENOMEM;
                return 0;
            }
            return send(buf, sz, timeout);
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
            if (m_id == nullptr) {
                m_id = (char *) malloc(IPADDR_MAXSTRLEN+8);
                snprintf(m_id, IPADDR_MAXSTRLEN+8, "%s::%d",
                         ipstr(addr()), port());
            }

            return m_id;
        }

        virtual void buffering(bool on, int64_t dd = -1) {}

        virtual ~SocketAdaptor() {
            if (m_id) {
                free(m_id);
                m_id = nullptr;
            }
        }

    private:
        char    *m_id{nullptr};
    };

    define_log_tag(SSL_SOCK);

    struct SslSock : public virtual SocketAdaptor, LOGGER(SSL_SOCK) {
        SslSock()
            : raw(nullptr)
        {}

        SslSock(sslsock s)
            : raw(s)
        {}

        SslSock(SslSock &&other);

        SslSock &operator=(SslSock &&other);

        virtual int port() const;

        virtual const ipaddr addr() const;

        virtual bool connect(ipaddr addr, int64_t timeout = -1);

        virtual size_t send(const void *buf, size_t len, int64_t timeout = -1);

        virtual size_t sendfile(int fd, off_t offset, size_t len, int64_t timeout = -1);

        virtual bool flush(int64_t timeout = -1);

        virtual bool receive(void *buf, size_t &len, int64_t timeout = -1);

        virtual bool receiveuntil(void *buf, size_t &len, const char *delims,
                                  size_t ndelims, int64_t timeout = -1);

        virtual bool read(void *buf, size_t &len, int64_t timeout = -1);

        virtual bool isopen() const;

        virtual void close() ;

        virtual ~SslSock();

    protected:
        sslsock raw;
    };

    define_log_tag(TCP_SOCKET);
    struct TcpSock : public virtual SocketAdaptor, LOGGER(TCP_SOCKET) {
        TcpSock()
            : raw(nullptr)
        {}

        TcpSock(tcpsock s)
            : raw(s)
        {}

        TcpSock(TcpSock &&other);

        TcpSock &operator=(TcpSock &&other);

        virtual int port() const;

        virtual const ipaddr addr() const;

        virtual bool connect(ipaddr addr, int64_t timeout = -1);

        virtual size_t send(const void *buf, size_t len, int64_t timeout = -1);

        virtual size_t sendfile(int fd, off_t offset, size_t len, int64_t timeout = -1);

        virtual bool flush(int64_t timeout = -1);

        virtual bool receive(void *buf, size_t &len, int64_t timeout = -1);

        virtual bool receiveuntil(void *buf, size_t &len, const char *delims,
                                  size_t ndelims, int64_t timeout = -1);

        virtual bool read(void *buf, size_t &len, int64_t timeout = -1);

        virtual bool isopen() const;

        virtual void close();

        virtual void buffering(bool on, int64_t dd) {
            tcpbuffering(raw, (on?1:0), dd);
        }

        virtual ~TcpSock();


    protected:
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

    struct SslSs : public ServerSock<SslSock>, LOGGER(SSL_SOCK) {
        typedef SslSock sock_t;
        typedef SslSsConfig config_t;

        SslSs(SslSsConfig& cfg)
            :config(cfg)
        {}

        virtual bool listen(ipaddr addr, int backlog);

        virtual bool accept(sock_t& s, int64_t timeout = -1);

        virtual void close();

        virtual void shutdown()
        {}

    private:
        sslsock         raw{nullptr};
        SslSsConfig& config;
    };

    struct TcpSsConfig {
    };

    struct TcpSs : public ServerSock<TcpSock>, public LOGGER(TCP_SOCKET) {
        typedef TcpSock sock_t;
        typedef TcpSsConfig config_t;

        TcpSs(TcpSsConfig /* Unused */)
            : raw(nullptr)
        {}

        virtual bool listen(ipaddr addr, int backlog);

        virtual bool accept(sock_t& s, int64_t timeout = -1);

        virtual void close();

        virtual void shutdown();

    private:
        tcpsock      raw;
    };
}
#endif //SUIL_SOCK_HPP
