//
// Created by dc on 11/16/17.
//

#ifndef SUIL_SMTP_HPP
#define SUIL_SMTP_HPP

#include <suil/sock.hpp>

namespace suil {

    define_log_tag(SMTP_CLIENT);

    template <typename Proto>
    struct stmp;

    namespace __internal {
        struct client;
    }

    struct Email {
        struct Address {
            Address(const char *email, const char *name = nullptr)
                : name(zcstring(name).dup()),
                  email(zcstring(email).dup())
            {}
            zcstring name;
            zcstring email;

            inline void encode(buffer_t& b) const {
                if (!name.empty()) {
                    b << "'" << name << "'";
                }
                b << "<" << email << ">";
            }
        };

        struct Attachment {
            zcstring fname;
            zcstring  mimetype;
            char    *data{nullptr};
            off_t    size{0};
            size_t   len{0};
            bool     mapped{false};

            Attachment(const zcstring& fname)
                : fname(fname.dup()),
                  mimetype(zcstring(utils::mimetype(fname)).dup())
            {}

            Attachment(const Attachment&) = delete;
            Attachment&operator=(const Attachment&) = delete;

            Attachment(Attachment&& other)
                : fname(std::move(other.fname)),
                  mimetype(std::move(other.mimetype)),
                  data(other.data),
                  size(other.size),
                  len(other.len),
                  mapped(other.mapped)
            {
                other.data = nullptr;
                other.mapped = false;
                size = len = 0;
            }



            Attachment&operator=(Attachment&& other)
            {
                fname = std::move(other.fname);
                mimetype = std::move(other.mimetype);
                data = other.data;
                size = other.size;
                len  = other.len;
                mapped = other.mapped;

                other.data = nullptr;
                other.size = other.len = 0;
                other.mapped = false;
            }

            inline bool load();

            void send(sock_adaptor& sock, const zcstring& boundary, int64_t timeout);

            inline const char *filename() const {
                const char *tmp = strrchr(fname.cstr, '/');
                if (tmp) return tmp+1;
                return fname.cstr;
            }

            ~Attachment();
        };

        Email(Address addr, const zcstring subject)
            : subject(subject.dup())
        {
            to(std::move(addr));
            zcstring  tmp = utils::catstr("suil", utils::randbytes(4));
            boundry = base64::encode(tmp);
        }

        template <typename... __T>
        inline void to(Address addr, __T... other) {
            pto(std::move(addr));
            to(other...);
        }

        template <typename... __T>
        inline void cc(Address addr, __T... other) {
            pcc(std::move(addr));
            cc(other...);
        }

        template <typename... __T>
        inline void bcc(Address addr, __T... other) {
            pbcc(std::move(addr));
            bcc(other...);
        }

        template <typename... __T>
        inline void attach(zcstring fname, __T... other) {
            pattach(fname);
            attach(other...);
        }

        inline void content(const char *type) {
            body_type = zcstring(type).dup();
        }

        inline void body(const char *msg, const char *type = nullptr) {
            zcstring tmp(msg);
            body(tmp, type);
        }

        inline void body(zcstring& msg, const char *type = nullptr) {
            if (type != nullptr)
                content(type);
            bodybuf << msg;
        }

        inline buffer_t& body() {
            return bodybuf;
        }

    private:
        zcstring gethead(const Address &from);

        template <typename Proto>
        friend struct suil::stmp;
        friend struct suil::__internal::client;

        inline void to(){}
        inline void pto(Address&& addr) {
            if (!exists(addr)) {
                receipts.emplace_back(addr);
            }
        }

        inline void cc(){}
        inline void pcc(Address&& addr) {
            if (!exists(addr)) {
                ccs.emplace_back(addr);
            }
        }

        inline void bcc(){}
        inline void pbcc(Address&& addr) {
            if (!exists(addr)) {
                bccs.emplace_back(addr);
            }
        }

        inline void attach(){}
        inline void pattach(const zcstring& fname) {
            if (attachments.end() ==  attachments.find(fname)) {
                attachments.emplace(fname.dup(), Attachment(fname));
            }
        }

        inline bool exists(const Address& addr) {
            auto predicate = [&](Address& in) {
                return in.email == addr.email;
            };
            return (receipts.end() != std::find_if(receipts.begin(), receipts.end(), predicate)) &&
                   (ccs.end() != std::find_if(ccs.begin(), ccs.end(), predicate)) &&
                   (bccs.end() != std::find_if(bccs.begin(), bccs.end(), predicate));
        }

        zcstring                body_type{"text/plain"};
        zcstring                subject;
        zcstring                boundry;
        std::vector<Address>    receipts;
        std::vector<Address>    ccs;
        std::vector<Address>    bccs;
        zcstr_map_t<Attachment> attachments;
        buffer_t                bodybuf;
    };

    namespace __internal {

        struct client : LOGGER(dtag(SMTP_CLIENT)) {
            client(sock_adaptor& sock)
                :sock(sock)
            {}

        private:
            template <typename Proto>
            friend struct suil::stmp;

            bool sendaddresses(const std::vector<Email::Address>& addrs, int64_t timeout);
            bool sendhead(Email& msg, int64_t timeout);
            void send(const Email::Address& from, Email& msg, int64_t timeout);

            bool login(const zcstring& domain, const zcstring& username, const zcstring& passwd);

            int getresponse(int64_t timeout);

            void quit();

            const char* showerror(int code);

            inline bool sendflush(int64_t timeout) {
                if (sock.send("\r\n", 2, timeout) == 2)
                    return sock.flush(timeout);
                return false;
            }

            inline bool sendline(int64_t timeout) { return  true; }

            inline bool send_data(int64_t timeout, const char *cmd) {
                size_t len = strlen(cmd);
                return sock.send(cmd, len, timeout) == len;
            }

            inline bool send_data(int64_t timeout, const zcstring& str) {
                return sock.send(str.cstr, str.len, timeout) == str.len;
            }

            template <typename... __P>
            inline ssize_t sendline(int64_t timeout, const zcstring data, __P... p) {
                if (send_data(timeout, data) && sendline(timeout, p...)) {
                    return sendflush(timeout);
                }
                return false;
            }

            ~client() {
                // close current connection gracefully
                quit();
            }

            sock_adaptor&    sock;
        };
    }

    template <typename Proto = ssl_sock>
    struct stmp : LOGGER(dtag(SMTP_CLIENT)) {
        /**
         * @brief Creates a new smtp mail server
         * @param server the server address
         * @param port the stmp port on the server
         */
        stmp(const char *server, int port)
            : server(zcstring(server).dup()),
              port(port),
              sender(proto)
        {}

        template <typename ...Params>
        inline void send(Email& msg, Email::Address from, Params... params) {
            if (!proto.isopen()) {
                // cannot send without logging
                throw suil_error::create("Send email failed: not connected to server");
            }
            auto opts = iod::D(params...);
            do_send(msg, from, opts);
        }

        template <typename... Params>
        inline void login(const zcstring username, const zcstring passwd, Params... params) {
            if (proto.isopen()) {
                throw suil_error::create("Already logged into mailserver");
            }
            auto opts = iod::D(params...);
            do_login(username, passwd, opts);
        }

    private:

        template <typename __P>
        void do_send(Email& msg, Email::Address& from, __P& params) {
            int64_t timeout = params.get(sym(timeout), 1500);
            sender.send(from, msg, timeout);
        }
        template <typename __P>
        bool do_login(const zcstring& user, const zcstring& passwd, __P& params) {
            int64_t timeout = params.get(sym(timeout), 1500);
            zcstring domain = params.get(sym(domain),  "localhost");

            ipaddr addr = ipremote(server.cstr, port, 0, utils::after(timeout));
            if (errno != 0) {
                error("server address '%s:%d' could not be resolved: %",
                            server.cstr, port, errno_s);
                return false;
            }

            // open connection to server using underlying protocol (either raw TCP or SSL)
            if (!proto.connect(addr, timeout)) {
                error("connecting to server '%s:%d' failed: %s",
                          server.cstr, port, errno_s);
                return false;
            }
            trace("connection to email server '%s:%d' open", server.cstr, port);

            // login to server
            return sender.login(domain, user, passwd);
        }

        Proto proto{};
        __internal::client sender;
        zcstring  server;
        int       port;
    };

}
#endif //SUIL_SMTP_HPP
