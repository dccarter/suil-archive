//
// Created by dc on 8/2/17.
//

#ifndef SUIL_PGSQL_HPP
#define SUIL_PGSQL_HPP


#include <libpq-fe.h>
//#include <catalog/pg_type.h>
#include <netinet/in.h>
#include <deque>
#include <memory>

#include "middleware.hpp"

namespace suil {
    namespace sql {

        define_log_tag(PGSQL_DB);
        define_log_tag(PGSQL_CONN);

        enum {
            PGSQL_ASYNC = 0x01
        };

        enum pg_types_t {
            CHAROID = 18,
            INT8OID = 20,
            INT2OID = 21,
            INT4OID = 23,
            TEXTOID = 25,
            FLOAT4OID = 700,
            FLOAT8OID = 701
        };
        namespace _internals {
            Oid type_to_pgsql_oid_type(const char&)
            { return CHAROID; }
            Oid type_to_pgsql_oid_type(const short int&)
            { return INT2OID; }
            Oid type_to_pgsql_oid_type(const int&)
            { return INT4OID; }
            Oid type_to_pgsql_oid_type(const long long&)
            { return INT8OID; }
            Oid type_to_pgsql_oid_type(const float&)
            { return FLOAT4OID; }
            Oid type_to_pgsql_oid_type(const double&)
            { return FLOAT8OID; }
            Oid type_to_pgsql_oid_type(const char*)
            { return TEXTOID; }

            Oid type_to_pgsql_oid_type(const unsigned char&)
            { return CHAROID; }
            Oid type_to_pgsql_oid_type(const unsigned  short int&)
            { return INT2OID; }
            Oid type_to_pgsql_oid_type(const unsigned int&)
            { return INT4OID; }
            Oid type_to_pgsql_oid_type(const unsigned long long&)
            { return INT8OID; }

            char *vhod_to_vnod(unsigned long long& buf, const char& v) {
                (*(char *) &buf) = v;
                return (char *) &buf;
            }
            char *vhod_to_vnod(unsigned long long& buf, const unsigned char& v) {
                (*(unsigned char *) &buf) = v;
                return (char *) &buf;
            }

            char *vhod_to_vnod(unsigned long long& buf, const short int& v) {
                (*(unsigned short int *) &buf) = htons((unsigned short) v);
                return (char *) &buf;
            }
            char *vhod_to_vnod(unsigned long long& buf, const unsigned short int& v) {
                (*(unsigned short int *) &buf) = htons((unsigned short) v);
                return (char *) &buf;
            }

            char *vhod_to_vnod(unsigned long long& buf, const int& v) {
                (*(unsigned int *) &buf) = htonl((unsigned int) v);
                return (char *) &buf;
            }
            char *vhod_to_vnod(unsigned long long& buf, const unsigned int& v) {
                (*(unsigned int *) &buf) = htonl((unsigned int) v);
                return (char *) &buf;
            }

            char *vhod_to_vnod(unsigned long long& buf, const float& v) {
                (*(unsigned int *) &buf) = htonl(*((unsigned int*) &v));
                return (char *) &buf;
            }

            union order8b_t {
                double  d64;
                unsigned long long ull64;
                long long ll64;
                struct {
                    unsigned int u32_1;
                    unsigned int u32_2;
                };
            };

            union float4_t {
                float f32;
                unsigned int u32;
            };

            template <typename __T>
            char *vhod_to_vnod(unsigned long long& buf, const __T& v) {
                order8b_t& to = (order8b_t &)buf;
                order8b_t& from = (order8b_t &)v;
                to.u32_1 =  htonl(from.u32_1);
                to.u32_2 =  htonl(from.u32_2);

                return (char *) &buf;
            }

            void vnod_to_vhod(const char *buf, char& v) {
                v = *buf;
            }
            void vnod_to_vhod(const char *buf, unsigned char& v) {
                v = (unsigned char) *buf;
            }

            void vnod_to_vhod(const char *buf, short int& v) {
                v = (short int)ntohs(*((const unsigned short int*) buf));
            }
            void vnod_to_vhod(const char *buf, unsigned short int& v) {
                v = ntohs(*((const unsigned short int*) buf));
            }

            void vnod_to_vhod(const char *buf, int& v) {
                v = (unsigned int)(int)ntohl(*((const unsigned int*) buf));
            }
            void vnod_to_vhod(const char *buf, unsigned int& v) {
                v = ntohl(*((unsigned int*) buf));
            }

            void vnod_to_vhod(const char *buf, float& v) {
                float4_t f;
                f.u32 = ntohl(*((unsigned int*) buf));
                v = f.f32;
            }

            template <typename __T>
            void vnod_to_vhod(const char *buf, __T& v) {
                order8b_t &to = (order8b_t &)v;
                order8b_t *from = (order8b_t *) buf;
                to.u32_1 = ntohl(from->u32_1);
                to.u32_2 = ntohl(from->u32_2);
            }
        };

        struct PGSQLStatement : LOGGER(dtag(PGSQL_CONN)) {

            PGSQLStatement(PGconn *conn, zcstring stmt, bool async, int64_t timeout = -1)
                : conn(conn),
                  stmt(std::move(stmt)),
                  async(async),
                  timeout(timeout)
            {}

            template <typename... __T>
            auto& operator()(__T&&... args) {
                const size_t size = sizeof...(__T)+1;
                const char *values[size] = {nullptr};
                int  lens[size]    = {0};
                int  bins[size]    = {0};
                Oid  oids[size]    = {InvalidOid};

                // declare a buffer that will hold values transformed to network order
                unsigned long long norder[size] = {0};

                int i = 0;
                iod::foreach(std::forward_as_tuple(args...)) |
                [&](auto& m) {
                    this->bind(values[i], oids[i], lens[i], bins[i], norder[i], m);
                    i++;
                };

                // Clear the results (important for reused statements)
                results.clear();
                if (async) {
                    int status = PQsendQueryParams(
                            conn,
                            stmt.str,
                            (int) sizeof...(__T),
                            oids,
                            values,
                            lens,
                            bins,
                            1);
                    if (!status) {
                        error("ASYNC QUERY: %s failed: %s", stmt.cstr, PQerrorMessage(conn));
                        throw std::runtime_error("executing async query failed");
                    }

                    bool wait = true, err = false;
                    while (!err && PQflush(conn)) {
                        trace("ASYNC QUERY: %s wait write %ld", stmt.cstr, timeout);
                        if (wait_write()) {
                            error("ASYNC QUERY: % wait write failed: %s", stmt.cstr, errno_s);
                            err  = true;
                            continue;
                        }
                    }

                    while (wait && !err) {
                        if (PQisBusy(conn)) {
                            trace("ASYNC QUERY: %s wait read %ld", stmt.cstr, timeout);
                            if (wait_read()) {
                                error("ASYNC QUERY: %s wait read failed: %s", stmt.cstr, errno_s);
                                err = true;
                                continue;
                            }
                        }

                        // asynchronously wait for results
                        if (!PQconsumeInput(conn)) {
                            error("ASYNC QUERY: %s failed: %s", stmt.cstr, PQerrorMessage(conn));
                            err = true;
                            continue;
                        }

                        PGresult *result = PQgetResult(conn);
                        if (result == nullptr) {
                            /* async query done */
                            wait = false;
                            continue;
                        }

                        switch (PQresultStatus(result)) {
                            case PGRES_COPY_OUT:
                            case PGRES_COPY_IN:
                            case PGRES_COPY_BOTH:
                            case PGRES_NONFATAL_ERROR:
                                break;
                            case PGRES_COMMAND_OK:
                                trace("ASYNC QUERY: continue waiting for results");
                                break;
                            case PGRES_TUPLES_OK:
#if PG_VERSION_NUM >= 90200
                            case PGRES_SINGLE_TUPLE:
#endif
                                results.add(result);
                                break;

                            default:
                                error("ASYNC QUERY: % failed: %s",
                                      stmt.cstr, PQerrorMessage(conn));
                                wait = false;
                        }
                    }

                    if (err) {
                        /* error occurred and was reported in logs */
                        throw std::runtime_error("waiting for async query failed");
                    }

                    trace("ASYNC QUERY: received %d results", results.results.size());
                }
                else {
                    PGresult *result = PQexecParams(
                            conn,
                            stmt.str,
                            (int) sizeof...(__T),
                            oids,
                            values,
                            lens,
                            bins,
                            1);
                    ExecStatusType status = PQresultStatus(result);
                    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
                        error("QUERY: %s failed: %s", stmt.cstr, PQerrorMessage(conn));
                        PQclear(result);
                        throw std::runtime_error("executing query failed");
                    }
                    else {
                        /* cache the results */
                        results.add(result);
                    }
                }

                return *this;
            }

            template <typename... __O>
            bool operator>>(iod::sio<__O...>& o) {
                if (results.empty()) return false;
                row_to_sio(o);

                return true;
            }

            template <typename __T>
            bool operator>>(__T& o) {
                if (results.empty()) return false;
                results.read(o, 0);
            }

            template <typename __F>
            void operator|(__F f) {
                if (results.empty()) return;

                typedef iod::callable_arguments_tuple_t<__F> __tmp;
                typedef std::remove_reference_t<std::tuple_element_t<0, __tmp>> __T;
                do {
                    __T o;
                    row_to_sio(o);
                    f(o);
                } while (results.next());

                // reset the iterator
                results.reset();
            }

            inline int last_insert_id() {
                results.results.size();
            }

        private:

            inline int wait_read() {
                int sock = PQsocket(conn);
                if (sock < 0) {
                    error("invalid PGSQL socket");
                    return -EINVAL;
                }

                int64_t tmp = timeout < 0? -1 : mnow() + timeout;
                int events = fdwait(sock, FDW_IN, tmp);
                if (events&FDW_ERR) {
                    return -1;
                } else if (events&FDW_IN) {
                    return 0;
                }
                errno = ETIMEDOUT;
                return ETIMEDOUT;
            }

            inline int wait_write() {
                int sock = PQsocket(conn);
                if (sock < 0) {
                    error("invalid PGSQL socket");
                    return -EINVAL;
                }

                int64_t tmp = timeout < 0? -1 : mnow() + timeout;
                int events = fdwait(sock, FDW_OUT, tmp);
                if (events&FDW_ERR) {
                    return -1;
                } else if (events&FDW_OUT) {
                    return 0;
                }

                errno = ETIMEDOUT;
                return ETIMEDOUT;
            }

            template <typename... __O>
            void row_to_sio(iod::sio<__O...>& o) {
                if (results.empty()) return;

                int ncols = PQnfields(results.result());

                iod::foreach(o) |
                [&] (auto &m) {
                    int fnumber = PQfnumber(results.result(), m.symbol().name());
                    if (fnumber != -1) {
                        // column found
                        results.read(m.value(), fnumber);
                    }
                };
            }

            template <typename __V>
            void bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, __V& v) {
                val = _internals::vhod_to_vnod(norder, v);
                len  =  sizeof(__V);
                bin  = 1;
                oid  = _internals::type_to_pgsql_oid_type(v);
            }

            void bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, std::string& v) {
                val = v.data();
                oid  = TEXTOID;
                len  = (int) v.size();
                bin  = 0;
            }

            void bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, const std::string& v) {
                bind(val, oid, len, bin, norder, *const_cast<std::string*>(&v));
            }

            void bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, char *s) {
                val = s;
                oid  =  TEXTOID;
                len  = (int) strlen(s);
                bin  = 0;
            }

            void bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, const char *s) {
                bind(val, oid, len, bin, norder, *const_cast<char *>(s));
            }

            struct pgsql_result {
                using result_q_t = std::deque<PGresult*>;
                typedef result_q_t::const_iterator results_q_it;

                result_q_t   results;
                results_q_it it;
                int row{0};

                inline PGresult *result() {
                    if (it != results.end()) return *it;
                }

                bool next() {
                    if (it != results.end()) {
                        row++;
                        if (row >= PQntuples(*it)) {
                            row = 0;
                            it++;
                        }
                    }

                    return it != results.end();
                }

                template <typename __V>
                void read(__V& v, int col) {
                    if (!empty()) {
                        char *data = PQgetvalue(*it, row, col);
                        _internals::vnod_to_vhod(data, v);
                    }
                }

                void read(std::string& v, int col) {
                    if (!empty()) {
                        char *data = PQgetvalue(*it, row, col);
                        int len = PQgetlength(*it, row, col);
                        v.resize((size_t) len);
                        memcpy(&v[0], data, (size_t) len);
                    }
                }

                void clear() {
                    results_q_it tmp = results.begin();
                    while (tmp != results.end()) {
                        PQclear(*tmp);
                        results.erase(tmp);
                        tmp = results.begin();
                    }
                    reset();
                }

                inline void reset() {
                    it = results.begin();
                    row = 0;
                }

                inline bool empty() {
                    return results.empty();
                };

                ~pgsql_result() {
                    clear();
                }

                inline void add(PGresult *res) {
                    results.push_back(res);
                }
            };

            zcstring     stmt;
            PGconn       *conn;
            bool         async;
            int64_t      timeout;
            pgsql_result results;
        };

        struct pgsql_connection: LOGGER(dtag(PGSQL_CONN)) {
            typedef std::vector<pgsql_connection*>::iterator active_conns_iterator_t;
            typedef zcstr_map_t <PGSQLStatement> stmt_map_t;
            typedef std::shared_ptr<stmt_map_t>  stmt_map_ptr_t;
            using free_conn_t = std::function<void(pgsql_connection*)>;

            pgsql_connection(PGconn *conn, bool async, int64_t timeout, free_conn_t free_conn)
                : conn(conn),
                  async(async),
                  timeout(timeout),
                  free_conn(free_conn)
            {}

            pgsql_connection(const pgsql_connection&) = delete;
            pgsql_connection&operator=(const pgsql_connection&) = delete;

            PGSQLStatement operator()(buffer_t& req) {
                /* temporary zero copy string */
                zcstring tmp(req, false);
                trace("%s", tmp.cstr);

                auto it = stmt_cache.find(tmp);
                if (it != stmt_cache.end()) {
                    return it->second;
                }

                /* statement not cached, create new */
                zcstring key(req);
                /* the key will be copied to statement so it won't be delete
                 * when the statement is deleted */
                auto ret = stmt_cache.insert(it,
                            std::make_pair(std::move(key),
                            PGSQLStatement(conn, key, async, timeout)));

                return ret->second;
            }

            PGSQLStatement operator()(const char *req) {
                /* temporary zero copy string */
                zcstring tmp(req);
                trace("%s", tmp.cstr);

                auto it = stmt_cache.find(tmp);
                if (it != stmt_cache.end()) {
                    return it->second;
                }

                buffer_t breq;
                breq << req;
                return (*this)(breq);
            }

            inline pgsql_connection& get() {
                refs++;
                return (*this);
            }

            inline void put() {
                destroy(false);
            }

            inline void close() {
                destroy(false);
            }

            static inline void params(buffer_t& req, int i) {
                req << "$" << i;
            }

            inline ~pgsql_connection() {
                if (conn) {
                    destroy(true);
                }
            }

        private:

            void destroy(bool dctor = false ) {
                if (conn == nullptr || --refs > 0) {
                    /* connection still being used */
                    return;
                }

                trace("destroying connection dctor %d %d", dctor, refs);
                if (async) {
                    PGresult *res;
                    while ((res = PQgetResult(conn)) != nullptr)
                        PQclear(res);
                }

                if (free_conn) {
                    /* call the function that will free the connection */
                    free_conn(this);
                }
                else {
                    /* no function to free connection, finish */
                    PQfinish(conn);
                }
                conn = nullptr;

                if (!dctor) {
                    deleting = true;
                    /* if the call is not from destructor destroy*/
                    delete this;
                }
            }

            friend struct pgsql_db;
            PGconn      *conn;
            stmt_map_t  stmt_cache;
            bool        async{false};
            int64_t     timeout{-1};
            free_conn_t free_conn;
            active_conns_iterator_t handle;
            int         refs{1};
            bool        deleting{false};
        };
        typedef std::vector<pgsql_connection*> active_conns_t;

        struct pgsql_db : LOGGER(dtag(PGSQL_DB)) {
            typedef pgsql_connection Connection;
            pgsql_db()
            {}

            Connection& operator()() {
                return connection();
            }

            Connection& connection() {
                PGconn *conn;
                if (conns.empty()) {
                    /* open a new connection */
                    conn = open();
                }
                else {
                    conn_handle_t h = conns.back();
                    /* cancel connection expiry */
                    h.alive = -1;
                    conn = h.conn;
                    conns.pop_back();
                }

                Connection *c = new Connection(
                        conn, async, timeout,
                        [&](Connection* _conn) {
                            free(_conn);
                        });
                return *c;
            }

            template<typename... __Opts>
            void init(const char *con_str, __Opts... opts) {
                if (conn_str) {
                    throw std::runtime_error("database already initialized");
                }
                conn_str = std::move(zcstring(con_str).copy());

                auto options = iod::D(opts...);
                async      = options.get(var(ASYNC), false);
                timeout    = options.get(var(TIMEOUT), -1);
                keep_alive = options.get(var(EXPIRES), -1);
                if (keep_alive > 0 && keep_alive < 3000) {
                    /* limit cleanup to 3 seconds */
                    warn("changing db connection keep alive from %ld ms to 3 seconds", keep_alive);
                    keep_alive = 3000;
                }

                /* open and close connetion to verify the connection string*/
                PGconn *conn = open();
                PQfinish(conn);
            }

            ~pgsql_db() {
                if (cleaning) {
                    /* unschedule the cleaning coroutine */
                    trace("notifying cleanup routine to exit");
                    notify << true;
                    /* wait for the cleaning coroutine to end*/
                    while (cleaning)
                        yield();
                }

                trace("cleaning up %lu connections", conns.size());
                auto it = conns.begin();
                while (it != conns.end()) {
                    (*it).cleanup();
                    conns.erase(it);
                    it = conns.begin();
                }
            }

        private:

            PGconn *open() {
                PGconn *conn;
                conn = PQconnectdb(conn_str.cstr);
                if (conn == nullptr || (PQstatus(conn) != CONNECTION_OK)) {
                    error("CONNECT: %s", PQerrorMessage(conn));
                    throw std::runtime_error("connecting to database failed");
                }

                if (async) {
                    /* connection should be set to non blocking */
                    if (PQsetnonblocking(conn, 1)) {
                        error("CONNECT: %s", PQerrorMessage(conn));
                        throw std::runtime_error("connecting to database failed");
                    }
                }

                return conn;
            }

            static coroutine void cleanup(pgsql_db& db) {
                /* cleanup all expired connections */
                bool status;
                int64_t expires = db.keep_alive + 5;
                if (db.conns.empty())
                    return;

                db.cleaning = true;

                do {
                    /* this will */
                    status = (db.notify[expires](1) | Void);
                    if (status) continue;

                    /* was not forced to exit */
                    auto it = db.conns.begin();
                    /* un-register all expired connections and all that will expire in the
                     * next 500 ms */
                    int64_t t = mnow() + 500;
                    int pruned = 0;
                    linfo(&db, "starting prune with %ld connections", db.conns.size());
                    while (it != db.conns.end()) {
                        if ((*it).alive <= t) {
                            (*it).cleanup();
                            db.conns.erase(it);
                            it = db.conns.begin();
                        } else {
                            /* there is no point in going forward */
                            break;
                        }

                        if ((++pruned % 100) == 0) {
                            /* avoid hogging the CPU */
                            yield();
                        }
                    }
                    linfo(&db, "pruned %ld connections", pruned);

                    if (it != db.conns.end()) {
                        /*ensure that this will run after at least 3 second*/
                        expires = std::max((*it).alive - t, (int64_t)3000);
                    }

                } while (!db.conns.empty());

                db.cleaning = false;
            }

            void free(Connection* conn) {
                conn_handle_t h {conn->conn, -1};

                if (keep_alive != 0) {
                    /* set connections keep alive */
                    h.alive = mnow() + keep_alive;
                    conns.push_back(h);

                    if (!cleaning && keep_alive > 0) {
                        sinfo("scheduling cleaning...");
                        /* schedule cleanup routine */
                        go(cleanup(*this));
                    }
                }
                else {
                    /* cleanup now*/
                    PQfinish(h.conn);
                }
            }

            struct conn_handle_t {
                PGconn  *conn;
                int64_t alive;
                inline void cleanup() {
                    if (conn) {
                        PQfinish(conn);
                    }
                }
            };

            std::deque<conn_handle_t> conns;
            bool          async{false};
            int64_t       keep_alive{-1};
            int64_t       timeout{-1};
            async_t<bool> notify{false};
            bool          cleaning{false};
            zcstring      conn_str;
        };

        using Postgres = sql::middleware<pgsql_db>;
    }
}
#endif //SUIL_PGSQL_HPP
