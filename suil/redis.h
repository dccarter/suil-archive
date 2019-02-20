//
// Created by dc on 11/12/17.
//
// Modified implementation of https://github.com/octo/credis

#ifndef SUIL_REDIS_HPP
#define SUIL_REDIS_HPP

#include <deque>
#include <suil/net.h>
#include <suil/blob.h>

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

            String operator()() const {
                return std::move(String(buffer.data(),
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
            friend struct BaseClient;

            String prepared() const {
                return String{buffer.data(), buffer.size(), false};
            }

            template <typename... Params>
            void prepare(const char *cmd, Params&... params) {
                buffer << SUIL_REDIS_PREFIX_ARRAY << (1+sizeof...(Params)) << SUIL_REDIS_CRLF;
                addparams(cmd, params...);
            }

            void addparam() {}

            template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
            void addparam(const T& param) {
                std::string str = std::to_string(param);
                String tmp(str.data(), str.size(), false);
                addparam(tmp);
            }

            void addparam(const char* param) {
                String str(param);
                addparam(str);
            }

            void addparam(const String& param) {
                buffer << SUIL_REDIS_PREFIX_STRING << param.size() << SUIL_REDIS_CRLF;
                buffer << param << SUIL_REDIS_CRLF;
            }

            void addparam(const Data& param) {
                size_t  sz =  param.size() << 1;
                buffer << SUIL_REDIS_PREFIX_STRING << sz << SUIL_REDIS_CRLF;
                buffer.reserve(sz);
                sz = utils::hexstr(param.cdata(), param.size(),
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

            template <typename T, typename... Params>
            void addparam(const T& param, const Params&... params) {
                addparam(param);
                addparam(params...);
            }

            OBuffer buffer;
        };

        struct Reply {
            Reply(char prefix, String&& data)
                : prefix(prefix),
                  data(data)
            {}

            Reply(OBuffer&& rxb)
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
            friend struct BaseClient;
            String data{nullptr};
            OBuffer recvd;
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

            template <typename T>
            const T get(int index = 0) const {
                if (index > entries.size())
                    throw Exception::create("index '", index,
                                             "' out of range '", entries.size(), "'");
                T d{};
                castreply(index, d);
                return std::move(d);

            }

            template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type * = nullptr>
            operator std::vector<T>() const {
                std::vector<T> tmp;
                for (int i=0; i < 0; i++) {
                    tmp.push_back(get<T>(i));
                }
                return  std::move(tmp);
            }

            template <typename T, typename std::enable_if<!std::is_arithmetic<T>::value>::type * = nullptr>
            operator std::vector<T>() const {
                std::vector<String> tmp;
                for (int i=0; i < 0; i++) {
                    tmp.emplace_back(get<String >(i));
                }
                return  std::move(tmp);
            }

            template <typename T>
            operator T() const {
                return  std::move(get<T>(0));
            }

            const String peek(int index = 0) {
                if (index > entries.size())
                    throw Exception::create("index '", index,
                                             "' out of range '", entries.size(), "'");
                return entries[0].data.peek();
            }

            void operator|(std::function<bool(const String&)> f) {
                for(auto& rp : entries) {
                    if (!f(rp.data)) break;
                }
            }

        private:

            template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
            void castreply(int idx, T& d) const {
                const String& tmp = entries[idx].data;
                utils::cast(tmp, d);
            }

            void castreply(int idx, String & d) const {
                const String& tmp = entries[idx].data;
                d = tmp.dup();
            }

            friend struct BaseClient;
            friend struct Transaction;

            std::vector<Reply> entries;
            OBuffer           buffer{128};
        };

        struct redisdb_config {
            int64_t     timeout;
            std::string passwd;
        };

        struct ServerInfo {
            String    version;
            ServerInfo(){}

            ServerInfo(const ServerInfo& o) = delete;
            ServerInfo&operator=(const ServerInfo& o) = delete;

            ServerInfo(ServerInfo&& o)
                : version(std::move(o.version)),
                  buffer(std::move(o.buffer)),
                  params(std::move(o.params))
            {}

            ServerInfo&operator=(ServerInfo&& o) {
                version = std::move(o.version);
                buffer  = std::move(o.buffer);
                params  = std::move(o.params);
                return *this;
            }

            const String&operator[](const char *key) const {
                String tmp(key);
                return (*this)[key];
            }

            const String&operator[](const String& key) const {
                auto data = params.find(key);
                if (data != params.end()) {
                    return data->second;
                }
                throw Exception::create("parameter '", key, "' does not exist");
            }

        private:
            friend  struct BaseClient;
            CaseMap<String> params;
            OBuffer      buffer;
        };

        struct BaseClient : LOGGER(REDIS) {

            Response send(Commmand& cmd) {
                return dosend(cmd, 1);
            }

            template <typename... Args>
            Response send(const String& cd, Args... args) {
                Commmand cmd(cd(), args...);
                return std::move(send(cmd));
            }

            template <typename... Args>
            Response operator()(String&& cd, Args... args) {
                return send(cd, args...);
            }

            void flush() {
                reset();
            }

            bool auth(const char *pass) {
                String tmp(pass);
                return auth(tmp);
            }

            bool auth(const String& pass) {
                Commmand cmd("AUTH", pass);
                auto resp = send(cmd);
                return resp.status();
            }

            bool ping() {
                Commmand cmd("PING");
                return send(cmd).status("PONG");
            }

            template <typename T>
            auto get(String&& key) -> T {
                Response resp = send("GET", key);
                if (!resp) {
                    throw Exception::create("redis GET '", key,
                                             "' failed: ", resp.error());
                }
                return (T) resp;
            }

            template <typename T>
            inline bool set(String&& key, const T val) {
                return send("SET", key, val).status();
            }

            int64_t incr(String&& key, int by = 0) {
                Response resp;
                if (by == 0)
                    resp = send("INCR", key);
                else
                    resp = send("INCRBY", by);

                if (!resp) {
                    throw Exception::create("redis INCR '", key,
                                             "' failed: ", resp.error());
                }
                return (int64_t) resp;
            }

            int64_t decr(String&& key, int by = 0) {
                Response resp;
                if (by == 0)
                    resp = send("DECR", key);
                else
                    resp = send("DECRBY", by);

                if (!resp) {
                    throw Exception::create("redis DECR '", key,
                                             "' failed: ", resp.error());
                }
                return (int64_t) resp;
            }

            template <typename T>
            inline bool append(String&& key, const T val) {
                return send("APPEND", key, val);
            }

            String substr(String&& key, int start, int end) {
                Response resp = send("SUBSTR", key, start, end);
                if (resp) {
                    return resp.get<String>(0);
                }
                throw Exception::create("redis SUBSTR '", key,
                                         "' start=", start, ", end=",
                                         end, " failed: ", resp.error());
            }

            bool exists(String&& key) {
                Response resp = send("EXISTS", key);
                if (resp) {
                    return (int) resp != 0;
                }
                else {
                    throw Exception::create("redis EXISTS '", key,
                                             "' failed: ", resp.error());
                }
            }

            bool del(String&& key) {
                Response resp = send("DEL", key);
                if (resp) {
                    return (int) resp != 0;
                }
                else {
                    throw Exception::create("redis DEL '", key,
                                             "' failed: ", resp.error());
                }
            }

            std::vector<String> keys(String&& pattern) {
                Response resp = send("KEYS", pattern);
                if (resp) {
                    std::vector<String> tmp = resp;
                    return std::move(tmp);
                }
                else {
                    throw Exception::create("redis KEYS pattern = '", pattern,
                                             "' failed: ", resp.error());
                }
            }

            int expire(String&& key, int64_t secs) {
                Response resp =  send("EXPIRE", key, secs);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw Exception::create("redis EXPIRE  '", key, "' secs ", secs,
                                             " failed: ", resp.error());
                }
            }

            int ttl(String&& key) {
                Response resp =  send("TTL", key);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw Exception::create("redis TTL  '", key,
                                             "' failed: ", resp.error());
                }
            }

            template <typename... T>
            int rpush(String&& key, const T... vals) {
                Response resp = send("RPUSH", key, vals...);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw Exception::create("redis RPUSH  '", key,
                                             "' failed: ", resp.error());
                }
            }

            template <typename... T>
            int lpush(String&& key, const T... vals) {
                Response resp = send("LPUSH", key, vals...);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw Exception::create("redis LPUSH  '", key,
                                             "' failed: ", resp.error());
                }
            }

            int llen(String&& key) {
                Response resp = send("LLEN", key);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw Exception::create("redis LLEN  '", key,
                                             "' failed: ", resp.error());
                }
            }

            template <typename T>
            auto lrange(String&& key, int start = 0, int end = -1) -> std::vector<T> {
                Response resp = send("LRANGE", key, start, end);
                if (resp) {
                    std::vector<T> tmp = resp;
                    return  std::move(tmp);
                }
                else {
                    throw Exception::create("redis LRANGE  '", key,
                                             "' failed: ", resp.error());
                }
            }

            bool ltrim(String&& key, int start = 0, int end = -1) {
                Response resp = send("LTRIM", key, start, end);
                if (resp) {
                    return resp.status();
                }
                else {
                    throw Exception::create("redis LTRIM  '", key,
                                             "' failed: ", resp.error());
                }
            }

            template <typename T>
            auto ltrim(String&& key, int index) -> T {
                Response resp = send("LINDEX", key, index);
                if (resp) {
                    return (T) resp;
                }
                else {
                    throw Exception::create("redis LINDEX  '", key,
                                             "' failed: ", resp.error());
                }
            }

            int hdel(const String&& hash, const String&& key) {
                Response resp = send("HDEL", hash, key);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw Exception::create("redis HDEL  '", key,
                                            "' failed: ", resp.error());
                }
            }

            bool hexists(String&& hash, String&& key) {
                Response resp = send("HEXISTS", hash, key);
                if (resp) {
                    return (int) resp != 0;
                }
                else {
                    throw Exception::create("redis HEXISTS  '", hash, " ", key,
                                            "' failed: ", resp.error());
                }
            }

            std::vector<String> hkeys(String&& hash) {
                Response resp = send("HKEYS", hash);
                if (resp) {
                    std::vector<String> keys = resp;
                    return  std::move(keys);
                }
                else {
                    throw Exception::create("redis HKEYS  '", hash,
                                            "' failed: ", resp.error());
                }
            }

            template <typename T>
            auto hvals(String&& hash) -> std::vector<T> {
                Response resp = send("HVALS", hash);
                if (resp) {
                    std::vector<T> vals = resp;
                    return  std::move(vals);
                }
                else {
                    throw Exception::create("redis HVALS  '", hash,
                                            "' failed: ", resp.error());
                }
            }

            template <typename T>
            auto hget(String&& hash, String&& key) -> T {
                Response resp = send("HGET", hash, key);
                if (!resp) {
                    throw Exception::create("redis HGET '", hash, " ", key,
                                            "' failed: ", resp.error());
                }
                return (T) resp;
            }

            template <typename T>
            inline bool hset(String&& hash, String&& key, const T val) {
                Response resp = send("HSET", hash, key, val);
                if (!resp) {
                    throw Exception::create("redis HSET '", hash, " ", key,
                                            "' failed: ", resp.error());
                }
                return (int) resp != 0;
            }

            inline size_t hlen(String&& hash) {
                return (size_t) send("HLEN", hash);
            }

            template <typename... T>
            int sadd(String&& set, const T... vals) {
                Response resp = send("SADD", set, vals...);
                if (resp) {
                    return (int) resp;
                }
                else {
                    throw Exception::create("redis SADD  '", set,
                                            "' failed: ", resp.error());
                }
            }

            template <typename T>
            auto smembers(String&& set) -> std::vector<T>{
                Response resp = send("SMEMBERS", set);
                if (resp) {
                    std::vector<T> vals = resp;
                    return  std::move(vals);
                }
                else {
                    throw Exception::create("redis SMEMBERS  '", set,
                                            "' failed: ", resp.error());
                }
            }

            template <typename T>
            auto spop(String&& set) -> T {
                Response resp = send("spop", set);
                if (!resp) {
                    throw Exception::create("redis spop '", set,
                                            "' failed: ", resp.error());
                }
                return (T) resp;
            }


            inline size_t scard(String&& set) {
                return (size_t) send("SCARD", set);
            }


            bool info(ServerInfo&);

        protected:
            BaseClient(SocketAdaptor& adaptor, redisdb_config& config)
                : adaptor(adaptor),
                  config(config)
            {}

            Response dosend(Commmand& cmd, size_t nreply);

            friend struct Transaction;
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

            bool recvresp(OBuffer& out, std::vector<Reply>& stagging);

            bool readline(OBuffer& out);

            bool readlen(int64_t& len);

            String commit(Response& resp);

            SocketAdaptor&         adaptor;
            std::vector<Commmand*> batched;
            redisdb_config&       config;
        };

        template <typename Sock>
        struct Client : BaseClient {
            Client(Sock&& insock, redisdb_config& config)
                : BaseClient(sock, config),
                  sock(std::move(insock))
            {}

            Client()
                : BaseClient(sock, config)
            {}

            Client(Client&& o)
                : BaseClient(sock, o.config),
                  sock(std::move(o.sock))
            {}

            Client&operator=(Client&& o) {
                sock = std::move(o.sock);
                adaptor = sock;
                config  = o.config;
                return *this;
            }

            Client&operator=(const Client&) = delete;
            Client(const Client&) = delete;

            ~Client() {
                // reset and close Connection
                reset();
                sock.close();
            }

        private:
            Sock sock;
        };

        template <typename Proto = TcpSock>
        struct RedisDb : LOGGER(REDIS) {
            template <typename... Args>
            RedisDb(const char *host, int port, Args... args)
                : addr(ipremote(host, port, 0, utils::after(3000)))
            {
                utils::apply_config(config, args...);
            }

            RedisDb()
            {}

            template <typename Opts>
            void configure(const char *host, int port, Opts& opts) {
                addr = ipremote(host, port, 0, utils::after(3000));
                utils::apply_options(Ego.config, opts);
            }

            Client<Proto> connect(int db = 0) {
                Proto proto;
                trace("opening redis Connection");
                if (!proto.connect(addr, config.timeout)) {
                    throw Exception::create("connecting to redis server '",
                            ipstr(addr), "' failed: ", errno_s);
                }
                trace("connected to redis server");
                Client<Proto> cli(std::move(proto), config);

                if (!config.passwd.empty()) {
                    // authenticate
                    if (!cli.auth(config.passwd.c_str())) {
                        throw Exception::create("redis - authorizing client failed");
                    }
                }
                else {
                    // ensure that the server is accepting commands
                    if (!cli.ping()) {
                        throw Exception::create("redis - ping Request failed");
                    }
                }

                if (db != 0) {
                    idebug("changing database to %d", db);
                    auto resp = cli("SELECT", 1);
                    if (!resp) {
                        throw Exception::create(
                                "redis - changing to selected database '",
                                db, "' failed: ", resp.error());
                    }
                }

                idebug("connected to redis server: %s", ipstr(addr));
                return std::move(cli);
            }

            const ServerInfo& getinfo(Client<Proto>& cli, bool refresh = true) {
                if (refresh || !srvinfo.version) {
                    if (!cli.info(srvinfo)) {
                        ierror("retrieving server information failed");
                    }
                }
                return srvinfo;
            }

        private:

            ipaddr         addr;
            redisdb_config config{1500, ""};
            ServerInfo     srvinfo;
        };

        struct Transaction : LOGGER(REDIS) {
            template <typename... Params>
            bool operator()(const char *cmd, Params... params) {
                if (Ego.in_multi) {
                    Response resp = client.send(cmd, params...);
                    return resp.status();
                }
                iwarn("Executing command '%s' in invalid transaction", cmd);
                return false;
            }

            Response& exec();

            bool discard();

            bool multi();

            Transaction(BaseClient& client);

            virtual ~Transaction();

        private:
            Response             cachedresp;
            BaseClient&          client;
            bool                 in_multi{false};
        };
    }
}
#endif //SUIL_REDIS_HPP
