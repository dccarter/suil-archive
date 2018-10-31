//
// Created by dc on 30/10/18.
//

#include <suil/sock.hpp>

namespace suil {

    SslSock::SslSock(SslSock &&other)
            : raw(other.raw) {
        other.raw = nullptr;
    }

    SslSock &SslSock::operator=(SslSock &&other) {
        raw = other.raw;
        other.raw = nullptr;
        return *this;
    }

    int SslSock::port() const {
        if (raw) return sslport(raw);
        return -1;
    }

    const ipaddr SslSock::addr() const {
        if (raw) return ssladdr(raw);
        return ipaddr{};
    }

    bool SslSock::connect(ipaddr addr, int64_t timeout) {
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

    size_t SslSock::send(const void *buf, size_t len, int64_t timeout) {
        if (!isopen()) {
            iwarn("writing to a closed socket not supported");
            errno = ENOTSUP;
            return 0;
        }

        size_t ns = sslsend(raw, buf, (int) len, utils::after(timeout));
        if (errno != 0) {
            trace("send error: %s", errno_s);
            if (errno == ECONNRESET) {
                close();
            }
            return 0;
        }

        return ns;
    }

    size_t SslSock::sendfile(int fd, off_t offset, size_t len, int64_t timeout) {
        assert(!"Unsupported operation");
        return 0;
    }

    bool SslSock::flush(int64_t timeout) {
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
                Ego.close();
            }
            return false;
        }

        return true;
    }

    bool SslSock::receive(void *buf, size_t &len, int64_t timeout) {
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
                Ego.close();
            }
            return false;
        }

        return true;
    }

    bool SslSock::receiveuntil(void *buf, size_t &len, const char *delims,
                                       size_t ndelims, int64_t timeout) {
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
                Ego.close();
            }
            return false;
        }

        return true;
    }

    bool SslSock::read(void *buf, size_t &len, int64_t timeout) {
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
                Ego.close();
            }
            return false;
        }

        return true;
    }

    bool SslSock::isopen() const {
        return raw != nullptr;
    }

    void SslSock::close() {
        if (isopen()) {
            flush(500);
            sslclose(raw);
            raw = nullptr;
        }
    }

    SslSock::~SslSock() {
        if (raw)
            close();
    }


    TcpSock::TcpSock(TcpSock &&other)
            : raw(other.raw) {
        other.raw = nullptr;
    }

    TcpSock &TcpSock::operator=(TcpSock &&other) {
        raw = other.raw;
        other.raw = nullptr;
        return *this;
    }

    int TcpSock::port() const {
        if (raw) return tcpport(raw);
        return -1;
    }

    const ipaddr TcpSock::addr() const {
        if (raw) return tcpaddr(raw);
        return ipaddr{};
    }

    bool TcpSock::connect(ipaddr addr, int64_t timeout) {
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

    size_t TcpSock::send(const void *buf, size_t len, int64_t timeout) {
        if (!isopen()) {
            trace("writing to a closed socket not supported");
            errno = ENOTSUP;
            return 0;
        } else {
            size_t ns = tcpsend(raw, buf, len, utils::after(timeout));
            if (errno != 0) {
                trace("sending failed: %s", errno_s);
                if (errno == ECONNRESET) {
                    Ego.close();
                }
                return 0;
            }

            return ns;
        }
    }

    size_t TcpSock::sendfile(int fd, off_t offset, size_t len, int64_t timeout) {
        if (!isopen()) {
            trace("writing to a closed socket not supported");
            errno = ENOTSUP;
            return 0;
        } else {
            size_t ns = tcpsendfile(raw, fd, offset, len, utils::after(timeout));
            if (errno != 0) {
                trace("sending failed: %s", errno_s);
                if (errno == ECONNRESET)
                    Ego.close();

                return 0;
            }

            return ns;
        }
    }

    bool TcpSock::flush(int64_t timeout) {
        if (!isopen())
            return false;

        tcpflush(raw, utils::after(timeout));
        if (errno != 0) {
            trace("flushing socket failed: %s", errno_s);
            if (errno == ECONNRESET)
                Ego.close();
            return false;
        }

        return true;

    }

    bool TcpSock::receive(void *buf, size_t &len, int64_t timeout) {
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
                Ego.close();

            return false;
        }

        return true;
    }

    bool TcpSock::receiveuntil(void *buf, size_t &len, const char *delims,
                                       size_t ndelims, int64_t timeout) {
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
                Ego.close();

            return false;
        }

        return true;
    }

    bool TcpSock::read(void *buf, size_t &len, int64_t timeout) {
        if (!isopen()) {
            iwarn("reading from a closed socket not supported");
            errno = ENOTSUP;
            len = 0;
            return false;
        }

        len = tcpread(raw, buf, len, utils::after(timeout));
        if (errno != 0) {
            trace("reading failed: %s", errno_s);
            if (errno == ECONNRESET)
                Ego.close();

            return false;
        }

        return true;
    }

    bool TcpSock::isopen() const {
        return raw != nullptr;
    }

    void TcpSock::close() {
        if (raw != nullptr) {
            tcpclose(raw);
            raw = nullptr;
        }
    }

    TcpSock::~TcpSock() {
        if (raw)
            close();
    }

    bool SslSs::listen(ipaddr addr, int backlog) {
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

    bool SslSs::accept(sock_t& s, int64_t timeout) {
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

    void SslSs::close() {
        if (raw) {
            sslclose(raw);
            raw = nullptr;
        }
    }

    bool TcpSs::listen(ipaddr addr, int backlog) {
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

    bool TcpSs::accept(sock_t& s, int64_t timeout) {
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

    void TcpSs::close() {
        if (raw) {
            tcpclose(raw);
            raw = nullptr;
        }
    }

    void TcpSs::shutdown() {
        if (raw) {
            tcpshutdown(raw, 2);
        }
    }
}
