//
// Created by dc on 11/16/17.
//

#ifndef SUIL_SMTP_HPP
#define SUIL_SMTP_HPP

#include <suil/sock.h>
#include <suil/base64.h>

namespace suil {

    define_log_tag(SMTP_CLIENT);

    template <typename Proto>
    struct Stmp;

    namespace __internal {
        struct client;
    }

    struct Email {
        struct Address {
            Address(const char *email, const char *name = nullptr)
                : name(String(name).dup()),
                  email(String(email).dup())
            {}
            String name;
            String email;

            inline void encode(OBuffer& b) const {
                if (!name.empty()) {
                    b << "'" << name << "'";
                }
                b << "<" << email << ">";
            }
        };

        struct Attachment {
            String fname;
            String  mimetype;
            char    *data{nullptr};
            off_t    size{0};
            size_t   len{0};
            bool     mapped{false};

            Attachment(const String& fname)
                : fname(fname.dup()),
                  mimetype(String(utils::mimetype(fname.peek())).dup())
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

            void send(SocketAdaptor& sock, const String& boundary, int64_t timeout);

            inline const char *filename() const {
                const char *tmp = strrchr(fname.data(), '/');
                if (tmp) return tmp+1;
                return fname.data();
            }

            ~Attachment();
        };

        Email(Address addr, const String subject)
            : subject(subject.dup())
        {
            to(std::move(addr));
            String  tmp = utils::catstr("suil", utils::randbytes(4));
            boundry = utils::base64::encode(tmp);
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
        inline void attach(String fname, __T... other) {
            pattach(fname);
            attach(other...);
        }

        inline void content(const char *type) {
            body_type = String(type).dup();
        }

        inline void body(const char *msg, const char *type = nullptr) {
            String tmp(msg);
            body(tmp, type);
        }

        inline void body(String& msg, const char *type = nullptr) {
            if (type != nullptr)
                content(type);
            bodybuf << msg;
        }

        inline OBuffer& body() {
            return bodybuf;
        }

    private:
        String gethead(const Address &from);

        template <typename Proto>
        friend struct suil::Stmp;
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
        inline void pattach(const String& fname) {
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

        String                body_type{"text/plain"};
        String                subject;
        String                boundry;
        std::vector<Address>    receipts;
        std::vector<Address>    ccs;
        std::vector<Address>    bccs;
        CaseMap<Attachment> attachments;
        OBuffer                bodybuf;
    };

    namespace __internal {

        struct client : LOGGER(SMTP_CLIENT) {
            client(SocketAdaptor& sock)
                :sock(sock)
            {}

        private:
            template <typename Proto>
            friend struct suil::Stmp;

            bool sendaddresses(const std::vector<Email::Address>& addrs, int64_t timeout);
            bool sendhead(Email& msg, int64_t timeout);
            void send(const Email::Address& from, Email& msg, int64_t timeout);

            bool login(const String& domain, const String& username, const String& passwd);

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

            inline bool send_data(int64_t timeout, const String& str) {
                return sock.send(str.data(), str.size(), timeout) == str.size();
            }

            template <typename... __P>
            inline ssize_t sendline(int64_t timeout, const String data, __P... p) {
                if (send_data(timeout, data) && sendline(timeout, p...)) {
                    return sendflush(timeout);
                }
                return false;
            }

            ~client() {
                // close current Connection gracefully
                quit();
            }

            SocketAdaptor&    sock;
        };
    }

    template <typename Proto = SslSock>
    struct Stmp : LOGGER(SMTP_CLIENT) {
        /**
         * @brief Creates a new smtp mail server
         * @param server the server address
         * @param port the stmp port on the server
         */
        Stmp(const char *server, int port)
            : server(String(server).dup()),
              port(port),
              sender(proto)
        {}

        template <typename ...Params>
        inline void send(Email& msg, Email::Address from, Params... params) {
            if (!proto.isopen()) {
                // cannot send without logging
                throw Exception::create("Send email failed: not connected to server");
            }
            auto opts = iod::D(params...);
            do_send(msg, from, opts);
        }

        template <typename... Params>
        inline void login(const String username, const String passwd, Params... params) {
            if (proto.isopen()) {
                throw Exception::create("Already logged into mailserver");
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
        bool do_login(const String& user, const String& passwd, __P& params) {
            int64_t timeout = params.get(sym(timeout), 1500);
            String domain = params.get(sym(domain),  "localhost");

            ipaddr addr = ipremote(server.data(), port, 0, utils::after(timeout));
            if (errno != 0) {
                ierror("server address '%s:%d' could not be resolved: %",
                            server.data(), port, errno_s);
                return false;
            }

            // open Connection to server using underlying protocol (either raw TCP or SSL)
            if (!proto.connect(addr, timeout)) {
                ierror("connecting to server '%s:%d' failed: %s",
                          server.data(), port, errno_s);
                return false;
            }
            trace("Connection to email server '%s:%d' open", server.data(), port);

            // login to server
            return sender.login(domain, user, passwd);
        }

        Proto proto{};
        __internal::client sender;
        String  server;
        int       port;
    };

}
#endif //SUIL_SMTP_HPP
