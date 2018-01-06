//
// Created by dc on 11/12/17.
//
// Modified implementation of https://github.com/octo/credis

#ifndef SUIL_REDIS_HPP
#define SUIL_REDIS_HPP

#include <deque>
#include <suil/net.hpp>
#include <suil/wire.hpp>

namespace suil {

#define SUIL_REDIS_CRLF                 "\r\n"
#define SUIL_REDIS_STATUS_OK            "OK"
#define SUIL_REDIS_PREFIX_ERROR         '-'
#define SUIL_REDIS_PREFIX_VALUE         '+'
#define SUIL_REDIS_PREFIX_STRING        '$'
#define SUIL_REDIS_PREFIX_ARRAY         '*'
#define SUIL_REDIS_PREFIX_INTEGER       ':'

    namespace redis {
        define_log_tag(REDIS);

        struct Commmand {
            template <typename... Params>
            Commmand(const char *cmd, Params... params)
                : buffer(32+strlen(cmd))
            {
                prepare(cmd, params...);
            }

            zcstring operator()() const {
                return std::move(zcstring(buffer.data(),
                                          MIN(64, buffer.size()),
                                          false).dup());
            }

            template <typename Param>
            Commmand& operator<<(const Param param) {
                addparam(param);
                return *this;
            }

            template <typename... Params>
            void addparams(Params&... params) {
                addparam(params...);
            }

        private:
            friend struct base_client;

            zcstring prepared() const {
                return zcstring{buffer.data(), buffer.size(), false};
            }

            template <typename... Params>
            void prepare(const char *cmd, Params&... params) {
                buffer << SUIL_REDIS_PREFIX_ARRAY << (1+sizeof...(Params)) << SUIL_REDIS_CRLF;
                addparams(cmd, params...);
            }

            void addparam() {}

            template <typename __T, typename std::enable_if<std::is_arithmetic<__T>::value>::type* = nullptr>
            void addparam(const __T& param) {
                std::string str = std::to_string(param);
                zcstring tmp(str.data(), str.size(), false);
                addparam(tmp);
            }

            void addparam(const char* param) {
                zcstring str(param);
                addparam(str);
            }

            void addparam(const zcstring& param) {
                buffer << SUIL_REDIS_PREFIX_STRING << param.size() << SUIL_REDIS_CRLF;
                buffer << param << SUIL_REDIS_CRLF;
            }

            void addparam(const breadboard& param) {
                auto raw = param.raw();
                size_t  sz =  raw.second << 1;
                buffer << SUIL_REDIS_PREFIX_STRING << sz << SUIL_REDIS_CRLF;
                buffer.reserve(sz);
                sz = utils::hexstr(raw.first, raw.second,
                                   &buffer.data()[buffer.size()], buffer.capacity());
                buffer.seek(sz);
                buffer << SUIL_REDIS_CRLF;
            }

            template <size_t B>
            void addparam(const Blob<B>& param) {
                auto raw = &param.cbin();
                size_t  sz =  param.size() << 1;
                buffer << SUIL_REDIS_PREFIX_STRING << sz << SUIL_REDIS_CRLF;
                buffer.reserve(sz);
                sz = utils::hexstr(raw, param.size(),
                                   &buffer.data()[buffer.size()], buffer.capacity());
                buffer.seek(sz);
                buffer << SUIL_REDIS_CRLF;
            }

            template <typename __T, typename... Params>
            void addparam(const __T& param, const Params&... params) {
                addparam(param);
                addparam(params...);
            }

            zbuffer buffer;
        };

        struct Reply {
            Reply(char prefix, zcstring&& data)
                : prefix(prefix),
                  data(data)
            {}

            Reply(zbuffer&& rxb)
                : recvd(std::move(rxb)),
                  prefix(rxb.data()[0]),
                  data(&rxb.data()[1], rxb.size()-3, false)
            {
                // null terminate
                data.data()[data.size()] = '\0';
            }

            operator bool() const {
                return (prefix != SUIL_REDIS_PREFIX_ERROR && !data.empty());
            }

            inline bool status(const char *expect = SUIL_REDIS_STATUS_OK) const {
                return (prefix == SUIL_REDIS_PREFIX_VALUE) && (data.compare(expect) == 0);
            }

            const char* error() const {
                if (prefix == SUIL_REDIS_PREFIX_ERROR) return  data.data();
                return "";
            }

            bool special() const {
                return prefix == '[' || prefix == ']';
            }

        private:
            friend struct Response;
            friend struct base_client;
            zcstring data{nullptr};
            zbuffer recvd;
            char     prefix{'-'};
        };

        struct Response {

            Response(){}

            Response(Reply&& rp)
            {
                entries.emplace_back(std::move(rp));
            }

            operator bool() const {
                if (entries.empty()) return false;
                else if(entries.size() > 1) return true;
                else  return entries[0];
            }

            inline const char* error() const {
                if (entries.empty()) {
                    return "";
                } else if(entries.size() > 1){
                    return entries[entries.size()-1].error();
                }
                else {
                    return entries[0].error();
                }
            }

            inline bool status(const char *expect = SUIL_REDIS_STATUS_OK) {
                return !entries.empty() && entries[0].status(expect);
            }

            template <typename __T>
            const __T get(int index = 0) const {
                if (index > entries.size())
                    throw suil_error::create("index '", index,
                                             "' out of range '", entries.size(), "'");
                __T d{};
                castreply(index, d);
                return std::move(d);

            }

            template <typename __T, typename std::enable_if<std::is_arithmetic<__T>::value>::type * = nullptr>
            operator std::vector<__T>() const {
                std::vector<__T> tmp;
                for (int i=0; i < 0; i++) {
                    tmp.push_back(get<__T>(i));
                }
                return  std::move(tmp);
            }

            template <typename __T, typename std::enable_if<!std::is_arithmetic<__T>::value>::type * = nullptr>
            operator std::vector<__T>() const {
                std::vector<zcstring> tmp;
                for (int i=0; i < 0; i++) {
                    tmp.emplace_back(get<zcstring >(i));
                }
                return  std::move(tmp);
            }

            template <typename __T>
            operator __T() const {
                return  std::move(get<__T>(0));
            }

            const zcstring peek(int index = 0) {
                if (index > entries.size())
                    throw suil_error::create("index '", index,
                                             "' out of range '", entries.size(), "'");
                return entries[0].data.peek();
            }

            void operator|(std::function<bool(const zcstring&)> f) {
                for(auto& rp : entries) {
                    if (!f(rp.data)) break;
                }
            }

        private:

            template <typename __T, typename std::enable_if<std::is_arithmetic<__T>::value>::type* = nullptr>
            void castreply(int idx, __T& d) const {
                const zcstring& tmp = entries[idx].data;
                utils::cast(tmp, d);
            }

            void castreply(int idx, zcstring & d) const {
                const zcstring& tmp = entries[idx].data;
                d = tmp.dup();
            }

            friend struct base_client;
            friend struct transaction;

            std::vector<Reply> entries;
            zbuffer           buffer{128};
        };

        struct redisdb_config {
            int64_t     timeout;
            std::string password;
        };

        struct server_info {
            zcstring    version;
            server_info(){}

            server_info(const server_info& o) = delete;
            server_info&operator=(const server_info& o) = delete;

            server_info(server_info&& o)
                : version(std::move(o.version)),
                  buffer(std::move(o.buffer)),
                  params(std::move(o.params))
            {}

            server_info&operator=(server_info&& o) {
                version = std::move(o.version);
                buffer  = std::move(o.buffer);
                params  = std::move(o.params);
                return *this;
            }

            const zcstring&operator[](const char *key) const {
                zcstring tmp(key);
                return (*this)[key];
            }

            const zcstring&operator[](const zcstring& key) const {
                auto data = params.find(key);
                if (data != params.end()) {
                    return data->second;
                }
                throw suil_error::create("parameter '", key, "' does not exist");
            }

        private:
            friend  struct base_client;
            zmap<zcstring> params;
            zbuffer    buffer;
        };

        struct base_client : LOGGER(dtag(REDIS)) {

            Response send(Commmand& cmd) {
                return dosend(cmd, 1);
            }

            template <typename... Args>
            Response send(const zcstring& cd, Args... args) {
                Commmand cmd(cd(), args...);
                return std::move(send(cmd));
            }

            template <typename... Args>
            Response operator()(zcstring&& cd, Args... args) {
                return send(cd, args...);
            }

            void flush() {
                reset();
            }

            bool auth(const char *pass) {
                zcstring tmp(pass);
                return auth(tmp);
            }

            bool auth(const zcstring& pass) {
                Commmand cmd("AUTH", pass);
                auto resp = send(cmd);
                return resp.status();
            }

            bool ping() {
                Commmand cmd("PING");
                return send(cmd).status("PONG");
            }

            template <typename __T>
            __T get(zcstring&& key) {
                Response resp = send("GET", key);
                if (!resp) {
                    throw suil_error::create("redis GET '", key,
                                             "' failed: ", resp.error());
                }
                return (__T) resp;
            }

            template <typename __T>
            inline bool set(zcstring&& key, const __T val) {
                return send("SET", key, val).status();
            }

            int64_t incr(zcstring&& key, int by = 0) {
                Response resp;
                if (by == 0)
                    resp = send("INCR", key);
                else
                    resp = send("INCRBY", by);

                if (!resp) {
                    throw suil_error::create("redis INCR '", key,
                                             "' failed: ", resp.error());
                }
                return (int64_t) resp;
            }

            int64_t decr(zcstring&& key, int by = 0) {
                Response resp;
                if (by == 0)
                    resp = send("DECR", key);
                else
                    resp = send("DECRBY", by);

                if (!resp) {
                    throw suil_error::create("redis DECR '", key,
                                             "' failed: ", resp.error());
                }
                return (int64_t) resp;
            }

            template <typename __T>
            inline bool append(zcstring&& key, const __T val) {
                return send("APPEND", key, val);
            }

            zcstring substr(zcstring&& key, int start, int end) {
                Response resp = send("SUBSTR", key, start, end);
                if (resp) {
                    return resp.get<zcstring>(0);
                }
                throw suil_error::create("redis SUBSTR '", key,
                                         "' start=", start, ", end=",
                                         end, " failed: ", resp.error());
            }

            bool exists(zcstring&& key) {
                Response resp = send("EXISTS", key);
                if (resp) {
                    return (int) resp == 0;
                }
                else {
                    throw suil_error::create("redis EXISTS '", key,
                                             "' failed: ", resp.error());
                }
            }

            bool del(zcstring&& key) {
                Response resp = send("DEL", key);
                if (resp) {
                    return (int) resp == 0;
                }
                else {
                    throw suil_error::create("redis DEL '", key,
                                             "' failed: ", resp.error());
                }
            }

            std::vector<zcstring> keys(zcstring&& pattern) {
                Response resp = send("KEYS", pattern);
                if (resp) {
                    std::vector<zcstring> tmp = resp;
                    return std::move(tmp);
                }
                else {
                    throw suil_error::create("redis KEYS pattern = '", pattern,
                                             "' failed: ", resp.error());
                }
            }

            int expire(zcstring&& key, int64_t secs) {
                Response resp =  send("EXPIRE", key, secs);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw suil_error::create("redis EXPIRE  '", key, "' secs ", secs,
                                             " failed: ", resp.error());
                }
            }

            int ttl(zcstring&& key) {
                Response resp =  send("TTL", key);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw suil_error::create("redis TTL  '", key,
                                             "' failed: ", resp.error());
                }
            }

            template <typename... __T>
            int rpush(zcstring&& key, const __T... vals) {
                Response resp = send("RPUSH", key, vals...);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw suil_error::create("redis RPUSH  '", key,
                                             "' failed: ", resp.error());
                }
            }

            template <typename... __T>
            int lpush(zcstring&& key, const __T... vals) {
                Response resp = send("LPUSH", key, vals...);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw suil_error::create("redis LPUSH  '", key,
                                             "' failed: ", resp.error());
                }
            }

            int llen(zcstring&& key) {
                Response resp = send("LLEN", key);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw suil_error::create("redis LLEN  '", key,
                                             "' failed: ", resp.error());
                }
            }

            std::vector<zcstring> lrange(zcstring&& key, int start = 0, int end = -1) {
                Response resp = send("LRANGE", key, start, end);
                if (resp) {
                    std::vector<zcstring> tmp = resp;
                    return  std::move(tmp);
                }
                else {
                    throw suil_error::create("redis LRANGE  '", key,
                                             "' failed: ", resp.error());
                }
            }

            bool ltrim(zcstring&& key, int start = 0, int end = -1) {
                Response resp = send("LTRIM", key, start, end);
                if (resp) {
                    return resp.status();
                }
                else {
                    throw suil_error::create("redis LTRIM  '", key,
                                             "' failed: ", resp.error());
                }
            }

            template <typename __T>
            __T ltrim(zcstring&& key, int index) {
                Response resp = send("LINDEX", key, index);
                if (resp) {
                    return (__T) resp;
                }
                else {
                    throw suil_error::create("redis LINDEX  '", key,
                                             "' failed: ", resp.error());
                }
            }

            bool info(server_info&);

        protected:
            base_client(SocketAdaptor& adaptor, redisdb_config& config)
                : adaptor(adaptor),
                  config(config)
            {}

            Response dosend(Commmand& cmd, size_t nreply);

            friend struct transaction;
            inline void reset() {
                batched.clear();
            }
            inline void batch(Commmand& cmd) {
                batched.push_back(&cmd);
            }
            inline void batch(std::vector<Commmand>& cmds) {
                for (auto& cmd: cmds) {
                    batch(cmd);
                }
            }

            bool recvresp(zbuffer& out, std::vector<Reply>& stagging);

            bool readline(zbuffer& out);

            bool readlen(int64_t& len);

            zcstring commit(Response& resp);

            SocketAdaptor&         adaptor;
            std::vector<Commmand*> batched;
            redisdb_config&       config;
        };

        template <typename Sock>
        struct client : base_client {
            client(Sock&& insock, redisdb_config& config)
                : base_client(sock, config),
                  sock(std::move(insock))
            {}

            client()
                : base_client(sock, config)
            {}

            client(client&& o)
                : base_client(sock, o.config),
                  sock(std::move(o.sock))
            {}

            client&operator=(client&& o) {
                sock = std::move(o.sock);
                adaptor = sock;
                config  = o.config;
                return *this;
            }

            client&operator=(const client&) = delete;
            client(const client&) = delete;

            ~client() {
                // reset and close Connection
                reset();
                sock.close();
            }

        private:
            Sock sock;
        };

        template <typename Proto = TcpSock>
        struct redisdb : LOGGER(dtag(REDIS)) {
            template <typename... Args>
            redisdb(const char *host, int port, Args... args)
                : addr(ipremote(host, port, 0, utils::after(3000)))
            {
                utils::apply_config(config, args...);
            }

            client<Proto> connect(const char *passwd = nullptr, int db = 0) {
                Proto proto;
                trace("opening redis Connection");
                if (!proto.connect(addr, config.timeout)) {
                    throw suil_error::create("connecting to redis server '",
                            ipstr(addr), "' failed: ", errno_s);
                }
                trace("connected to redis server");
                client<Proto> cli(std::move(proto), config);

                if (passwd != nullptr) {
                    // authenticate
                    if (!cli.auth(passwd)) {
                        throw suil_error::create("redis - authorizing client failed");
                    }
                }
                else {
                    // ensure that the server is accepting commands
                    if (!cli.ping()) {
                        throw suil_error::create("redis - ping Request failed");
                    }
                }

                if (db != 0) {
                    idebug("changing database to %d", db);
                    auto resp = cli("SELECT", 1);
                    if (!resp) {
                        throw suil_error::create(
                                "redis - changing to selected database '",
                                db, "' failed: ", resp.error());
                    }
                }

                idebug("connected to redis server: %s", ipstr(addr));
                return std::move(cli);
            }

            const server_info& getinfo(client<Proto>& cli, bool refresh = true) {
                if (refresh || !srvinfo.version) {
                    if (!cli.info(srvinfo)) {
                        ierror("retrieving server information failed");
                    }
                }
                return srvinfo;
            }

        private:

            ipaddr  addr;
            redisdb_config config{1500, ""};
            server_info    srvinfo;
        };

        struct transaction : LOGGER(dtag(REDIS)) {
            template <typename... Params>
            transaction& operator()(const char *cmd, Params... params) {
                Commmand tmp(cmd, params...);
                commands.emplace_back(std::move(tmp));
                return *this;
            }

            Response& execute();

            inline void clear() {
                commands.clear();
                cachedresp.entries.clear();
                client.flush();
            }

            transaction(base_client& client)
                : client(client)
            {}

        private:

            std::vector<Commmand> commands;
            Response             cachedresp;
            base_client&         client;
        };
    }
}
#endif //SUIL_REDIS_HPP
