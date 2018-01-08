//
// Created by dc on 8/2/17.
//

#ifndef SUIL_PGSQL_HPP
#define SUIL_PGSQL_HPP


#include <libpq-fe.h>
#include <netinet/in.h>
#include <deque>
#include <memory>

#include <suil/sql/middleware.hpp>
#include <suil/sql/orm.hpp>

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
            FLOAT8OID = 701,
            INT2ARRAYOID   = 1005,
            INT4ARRAYOID   = 1007,
            TEXTARRAYOID   = 1009,
            FLOAT4ARRAYOID = 1021
        };

        namespace __internal {

            inline Oid type_to_pgsql_oid_type(const char&)
            { return CHAROID; }
            inline Oid type_to_pgsql_oid_type(const short int&)
            { return INT2OID; }
            inline Oid type_to_pgsql_oid_type(const int&)
            { return INT4OID; }
            inline Oid type_to_pgsql_oid_type(const long int&)
            { return INT8OID; }
            inline Oid type_to_pgsql_oid_type(const long long&)
            { return INT8OID; }
            inline Oid type_to_pgsql_oid_type(const float&)
            { return FLOAT4OID; }
            inline Oid type_to_pgsql_oid_type(const double&)
            { return FLOAT8OID; }
            inline Oid type_to_pgsql_oid_type(const char*)
            { return TEXTOID; }
            inline Oid type_to_pgsql_oid_type(const std::string&)
            { return TEXTOID; }
            inline Oid type_to_pgsql_oid_type(const zcstring&)
            { return TEXTOID; }
            inline Oid type_to_pgsql_oid_type(const strview&)
            { return TEXTOID; }

            inline Oid type_to_pgsql_oid_type(const unsigned char&)
            { return CHAROID; }
            inline Oid type_to_pgsql_oid_type(const unsigned  short int&)
            { return INT2OID; }
            inline Oid type_to_pgsql_oid_type(const unsigned int&)
            { return INT4OID; }
            inline Oid type_to_pgsql_oid_type(const unsigned long long&)
            { return INT8OID; }

            template <typename __T, typename std::enable_if<std::is_integral<__T>::value>::type* = nullptr>
            inline Oid type_to_pgsql_oid_type_number(
                    const std::vector<__T>&)
            { return INT4ARRAYOID; }

            template <typename __T, typename std::enable_if<std::is_floating_point<__T>::value>::type* = nullptr>
            inline Oid type_to_pgsql_oid_type_number(
                    const std::vector<__T>&)
            { return FLOAT4ARRAYOID; }

            template <typename __T>
            inline Oid type_to_pgsql_oid_type(std::vector<__T>& n)
            { return type_to_pgsql_oid_type_number(n); }

            inline Oid type_to_pgsql_oid_type(const std::vector<zcstring>&)
            { return TEXTARRAYOID; }

            template <typename T>
            static const char* type_to_pgsql_string(const T& v) {
                Oid t = type_to_pgsql_oid_type(v);
                switch (t) {
                    case CHAROID:
                    case INT2OID:
                        return "smallint";
                    case INT4OID:
                        return "integer";
                    case INT8OID:
                        return "bigint";
                    case FLOAT4OID:
                        return "real";
                    case FLOAT8OID:
                        return "double precision";
                    case TEXTOID:
                        return "text";
                    case INT4ARRAYOID:
                        return "integer[]";
                    case FLOAT4ARRAYOID:
                        return "real[]";
                    case TEXTARRAYOID:
                        return "text[]";
                    default:
                        return "text";
                }
            }

            inline char *vhod_to_vnod(unsigned long long& buf, const char& v) {
                (*(char *) &buf) = v;
                return (char *) &buf;
            }
            inline char *vhod_to_vnod(unsigned long long& buf, const unsigned char& v) {
                (*(unsigned char *) &buf) = v;
                return (char *) &buf;
            }

            inline char *vhod_to_vnod(unsigned long long& buf, const short int& v) {
                (*(unsigned short int *) &buf) = htons((unsigned short) v);
                return (char *) &buf;
            }
            inline char *vhod_to_vnod(unsigned long long& buf, const unsigned short int& v) {
                (*(unsigned short int *) &buf) = htons((unsigned short) v);
                return (char *) &buf;
            }

            inline char *vhod_to_vnod(unsigned long long& buf, const int& v) {
                (*(unsigned int *) &buf) = htonl((unsigned int) v);
                return (char *) &buf;
            }
            inline char *vhod_to_vnod(unsigned long long& buf, const unsigned int& v) {
                (*(unsigned int *) &buf) = htonl((unsigned int) v);
                return (char *) &buf;
            }

            inline char *vhod_to_vnod(unsigned long long& buf, const float& v) {
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
            static char *vhod_to_vnod(unsigned long long& buf, const __T& v) {
                order8b_t& to = (order8b_t &)buf;
                order8b_t& from = (order8b_t &)v;
                to.u32_1 =  htonl(from.u32_2);
                to.u32_2 =  htonl(from.u32_1);

                return (char *) &buf;
            }

            template <typename __T>
            static inline void vhod_to_vnod_append(zbuffer& b,
                  const typename  std::enable_if<std::is_arithmetic<__T>::value, __T>::type & d)
            { b << d; }

            static inline void vhod_to_vnod_append(zbuffer& b, const zcstring &d) {
                b << '"' << d << '"';
            }

            template <typename __T>
            static char *vhod_to_vnod(zbuffer& b, const std::vector<__T>& data) {
                b << "{";
                bool  first{true};
                for (auto& d: data) {
                    if (!first) b << ',';
                    first = false;
                    vhod_to_vnod_append(b, d);
                }

                b << "}";

                return b.data();
            }

            template <typename __T>
            static typename std::enable_if<std::is_arithmetic<__T>::value, __T>::type
            __array_value(const char* in, size_t len)
            {
                zcstring tmp(in, len, false);
                __T out{0};
                utils::cast(tmp, out);
                return out;
            };

            static zcstring __array_value(const char* in, size_t len)
            {
                zcstring tmp(in, len, false);
                return std::move(tmp.dup());
            };

            template <typename __T>
            static void parse_array(std::vector<__T>& to, const char *from) {
                const char *start = strchr(from, '{'), *end = strchr(from, '}');
                if (start == nullptr || end == nullptr) return;

                size_t len{0};
                const char *s = start;
                const char *e = end;
                bool done{false};
                s++;
                while (!done) {
                    e = strchr(s, ',');
                    if (!e) {
                        done = true;
                        e = end;
                    }
                    len = e-s;
                    to.emplace_back(std::move(__array_value(s, len)));
                    e++;
                    s = e;
                }
            }

            inline void vnod_to_vhod(const char *buf, char& v) {
                v = *buf;
            }
            inline void vnod_to_vhod(const char *buf, unsigned char& v) {
                v = (unsigned char) *buf;
            }

            inline void vnod_to_vhod(const char *buf, short int& v) {
                v = (short int)ntohs(*((const unsigned short int*) buf));
            }
            inline void vnod_to_vhod(const char *buf, unsigned short int& v) {
                v = ntohs(*((const unsigned short int*) buf));
            }

            inline void vnod_to_vhod(const char *buf, int& v) {
                v = (unsigned int)(int)ntohl(*((const unsigned int*) buf));
            }
            inline void vnod_to_vhod(const char *buf, unsigned int& v) {
                v = ntohl(*((unsigned int*) buf));
            }

            inline void vnod_to_vhod(const char *buf, float& v) {
                float4_t f;
                f.u32 = ntohl(*((unsigned int*) buf));
                v = f.f32;
            }

            template <typename __T>
            static void vnod_to_vhod(const char *buf, __T& v) {
                order8b_t &to = (order8b_t &)v;
                order8b_t *from = (order8b_t *) buf;
                to.u32_1 = ntohl(from->u32_2);
                to.u32_2 = ntohl(from->u32_1);
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
                // collects all buffers that were needed to submit a transaction
                // and frees them after the transaction
                struct garbage_collector {
                    std::vector<void*> bag{};

                    ~garbage_collector() {
                        for(auto b: bag)
                            memory::free(b);
                    }
                } gc;

                int i = 0;
                iod::foreach(std::forward_as_tuple(args...)) |
                [&](auto& m) {
                    void *tmp = this->bind(values[i], oids[i], lens[i], bins[i], norder[i], m);
                    if (tmp != nullptr) {
                        /* add to garbage collector*/
                        gc.bag.push_back(tmp);
                    }
                    i++;
                };

                // Clear the results (important for reused statements)
                results.clear();
                if (async) {
                    int status = PQsendQueryParams(
                            conn,
                            stmt.data(),
                            (int) sizeof...(__T),
                            oids,
                            values,
                            lens,
                            bins,
                            0);
                    if (!status) {
                        ierror("ASYNC QUERY: %s failed: %s", stmt(), PQerrorMessage(conn));
                        throw std::runtime_error("executing async query failed");
                    }

                    bool wait = true, err = false;
                    while (!err && PQflush(conn)) {
                        trace("ASYNC QUERY: %s wait write %ld", stmt(), timeout);
                        if (wait_write()) {
                            ierror("ASYNC QUERY: % wait write failed: %s", stmt(), errno_s);
                            err  = true;
                            continue;
                        }
                    }

                    while (wait && !err) {
                        if (PQisBusy(conn)) {
                            trace("ASYNC QUERY: %s wait read %ld", stmt.data(), timeout);
                            if (wait_read()) {
                                ierror("ASYNC QUERY: %s wait read failed: %s", stmt.data(), errno_s);
                                err = true;
                                continue;
                            }
                        }

                        // asynchronously wait for results
                        if (!PQconsumeInput(conn)) {
                            ierror("ASYNC QUERY: %s failed: %s", stmt(), PQerrorMessage(conn));
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
                                ierror("ASYNC QUERY: %s failed: %s",
                                      stmt(), PQerrorMessage(conn));
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
                            stmt.data(),
                            (int) sizeof...(__T),
                            oids,
                            values,
                            lens,
                            bins,
                            0);
                    ExecStatusType status = PQresultStatus(result);

                    if ((status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK)) {
                        ierror("QUERY: %s failed: %s", stmt(), PQerrorMessage(conn));
                        PQclear(result);
                        results.fail();
                    }
                    else if ((PQntuples(result) == 0)) {
                        idebug("QUERY: %s has zero entries: %s",
                              stmt(), PQerrorMessage(conn));
                        PQclear(result);
                    }
                    else {

                        /* cache the results */
                        results.add(result);
                    }
                }

                return *this;
            }

            template <typename... __O>
            inline bool operator>>(iod::sio<__O...>& o) {
                if (results.empty()) return false;
                return row_to_sio(o);
            }

            template <typename __T>
            bool operator>>(__T& o) {
                if (results.empty()) return false;
                return results.read(o, 0);
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

            inline bool status() {
                return !results.failed();
            }

            inline bool size() const {
                return results.results.size();
            }

        private:

            inline int wait_read() {
                int sock = PQsocket(conn);
                if (sock < 0) {
                    ierror("invalid PGSQL socket");
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
                    ierror("invalid PGSQL socket");
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
            bool row_to_sio(iod::sio<__O...>& o) {
                if (results.empty()) return false;

                int ncols = PQnfields(results.result());
                bool status{true};
                iod::foreach(suil::sql::__internal::remove_ignore_fields_t<decltype(o)>()) |
                [&] (auto &m) {
                    if (status) {
                        int fnumber = PQfnumber(results.result(), m.symbol().name());
                        if (fnumber != -1) {
                            // column found
                            status = results.read(o[m], fnumber);
                        }
                    }
                };

                return status;
            }

            template <typename __V, typename std::enable_if<std::is_arithmetic<__V>::value>::type* = nullptr>
            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, __V& v) {
                val = __internal::vhod_to_vnod(norder, v);
                len  =  sizeof(__V);
                bin  = 1;
                oid  = __internal::type_to_pgsql_oid_type(v);
                return nullptr;
            }

            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, std::string& v) {
                val = v.data();
                oid  = TEXTOID;
                len  = (int) v.size();
                bin  = 0;
                return nullptr;
            }

            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, const std::string& v) {
                return bind(val, oid, len, bin, norder, *const_cast<std::string*>(&v));
            }

            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, char *s) {
                return bind(val, oid, len, bin, norder, (const char *)s);
            }

            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, const char *s) {
                val = s;
                oid  =  TEXTOID;
                len  = (int) strlen(s);
                bin  = 0;
                return nullptr;
            }

            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, zcstring& s) {
                val = s.data();
                oid  =  TEXTOID;
                len  = s.size();
                bin  = 1;
                return nullptr;
            }

            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, const zcstring& s) {
                return bind(val, oid, len, bin, norder, *const_cast<zcstring*>(&s));
            }

            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, strview& v) {
                val = v.data();
                oid  = TEXTOID;
                len  = (int) v.size();
                bin  = 1;
                return nullptr;
            }

            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, const strview& v) {
                return bind(val, oid, len, bin, norder, *const_cast<strview*>(&v));
            }

            template <typename __V>
            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, const std::vector<__V>& v) {
                zbuffer b(32);
                val  = __internal::vhod_to_vnod(b, v);
                oid  = __internal::type_to_pgsql_oid_type(v);
                len  = (int) b.size();
                bin  = 0;
                return b.release();
            }


            struct pgsql_result {
                using result_q_t = std::deque<PGresult*>;
                typedef result_q_t::const_iterator results_q_it;

                bool failure{false};
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

                template <typename __V, typename std::enable_if<std::is_arithmetic<__V>::value,void>::type* = nullptr>
                bool read(__V& v, int col) {
                    if (!empty()) {
                        char *data = PQgetvalue(*it, row, col);
                        if (data != nullptr) {
                            //__internal::vnod_to_vhod(data, v);
                            zcstring tmp(data);
                            utils::cast(data, v);
                            return true;
                        }
                    }
                    return false;
                }

                template <typename __T>
                bool read(std::vector<__T>& v, int col) {
                    if (!empty()) {
                        char *data = PQgetvalue(*it, row, col);
                        if (data != nullptr) {
                            int len = PQgetlength(*it, row, col);
                            __internal::parse_array(v, data);
                            return true;
                        }
                    }
                    return false;
                }

                bool read(std::string& v, int col) {
                    if (!empty()) {
                        char *data = PQgetvalue(*it, row, col);
                        if (data != nullptr) {
                            int len = PQgetlength(*it, row, col);
                            v.resize((size_t) len);
                            memcpy(&v[0], data, (size_t) len);
                            return true;
                        }
                    }
                    return false;
                }

                bool read(zcstring& v, int col) {
                    if (!empty()) {
                        char *data = PQgetvalue(*it, row, col);
                        if (data != nullptr) {
                            int len = PQgetlength(*it, row, col);
                            v = std::move(zcstring(data, len, false).dup());
                            return true;
                        }
                    }

                    return false;
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

                inline bool empty(size_t idx = 0) {
                    return failure || results.empty() || results.size() <= idx;
                };

                ~pgsql_result() {
                    clear();
                }

                inline void add(PGresult *res) {
                    results.push_back(res);
                }

                inline void fail() {
                    failure = true;
                }

                inline bool failed() const {
                    return failure;
                }
            };

            zcstring     stmt;
            PGconn       *conn;
            bool         async;
            int64_t      timeout;
            pgsql_result results;
        };

        struct PgSqlConnection: LOGGER(dtag(PGSQL_CONN)) {
            typedef std::vector<PgSqlConnection*>::iterator active_conns_iterator_t;
            typedef zmap <PGSQLStatement> stmt_map_t;
            typedef std::shared_ptr<stmt_map_t>  stmt_map_ptr_t;
            using free_conn_t = std::function<void(PgSqlConnection*)>;

            PgSqlConnection(PGconn *conn, zcstring& dname, bool async, int64_t timeout, free_conn_t free_conn)
                : conn(conn),
                  async(async),
                  timeout(timeout),
                  free_conn(free_conn),
                  dbname(dname)
            {}

            PgSqlConnection(const PgSqlConnection&) = delete;
            PgSqlConnection&operator=(const PgSqlConnection&) = delete;

            PGSQLStatement operator()(zbuffer& req) {
                /* temporary zero copy string */
                zcstring tmp(req, false);
                trace("%s", tmp());

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
                trace("%s", tmp());

                auto it = stmt_cache.find(tmp);
                if (it != stmt_cache.end()) {
                    return it->second;
                }

                zbuffer breq;
                breq << req;
                return (*this)(breq);
            }

            bool has_table(const char *name) {
                const char *schema = "public";
                return has_table(schema, name);
            }

            bool has_table(const char* schema, const char* name) {
                auto stmt = (*this)("SELECT COUNT(*) FROM pg_catalog.pg_tables"
                                            " WHERE schemaname=$1 AND tablename=$2;");
                int found = 0;
                stmt(schema, name) >> found;
                return found;
            }

            template <typename __T>
            bool create_table(const zcstring& name, __T o) {
                zbuffer b(64);
                b << "CREATE TABLE " << name << "(";

                bool first{true};
                iod::foreach2(o)
                | [&] (auto& m) {
                    if (!first) {
                        b << ", ";
                    }
                    first = false;

                    // add symbol name and type
                    b << m.symbol().name();

                    if (m.attributes().has(var(AUTO_INCREMENT))) {
                        // if symbol is auto increment, use serial type
                        b << " serial";
                    }
                    else {
                        // deduce type
                        b << ' ' << __internal::type_to_pgsql_string(m.value());
                    }

                    if (m.attributes().has(var(UNIQUE))) {
                        // symbol should marked as unique
                        b << " UNIQUE";
                    }

                    if (m.attributes().has(var(PRIMARY_KEY))) {
                        // symbol is a primary key
                        b << " PRIMARY KEY";
                    }
                };

                b << ")";
                try {
                    auto stmt = Ego(b);
                    stmt();
                    return true;
                }
                catch (...) {
                    ierror("create_table '%s' failed: %s",
                        name(), SuilError::getmsg(std::current_exception()));
                    return false;
                }
            }

            inline PgSqlConnection& get() {
                refs++;
                return (*this);
            }

            inline void put() {
                destroy(false);
            }

            inline void close() {
                destroy(false);
            }

            static inline void params(zbuffer& req, int i) {
                req << "$" << i;
            }

            inline ~PgSqlConnection() {
                if (conn) {
                    destroy(true);
                }
            }

        private:

            void destroy(bool dctor = false ) {
                if (conn == nullptr || --refs > 0) {
                    /* Connection still being used */
                    return;
                }

                trace("destroying Connection dctor %d %d", dctor, refs);
                if (async) {
                    PGresult *res;
                    while ((res = PQgetResult(conn)) != nullptr)
                        PQclear(res);
                }

                if (free_conn) {
                    /* call the function that will free the Connection */
                    free_conn(this);
                }
                else {
                    /* no function to free Connection, finish */
                    PQfinish(conn);
                }
                conn = nullptr;

                if (!dctor) {
                    deleting = true;
                    /* if the call is not from destructor destroy*/
                    delete this;
                }
            }

            friend struct PgSqlDb;
            PGconn      *conn;
            stmt_map_t  stmt_cache;
            bool        async{false};
            int64_t     timeout{-1};
            free_conn_t free_conn;
            active_conns_iterator_t handle;
            int         refs{1};
            bool        deleting{false};
            suil::zcstring dbname{"public"};
        };
        typedef std::vector<PgSqlConnection*> active_conns_t;

        struct PgSqlDb : LOGGER(dtag(PGSQL_DB)) {
            typedef PgSqlConnection Connection;
            PgSqlDb()
            {}

            Connection& operator()() {
                return connection();
            }

            Connection& connection() {
                PGconn *conn;
                if (conns.empty()) {
                    /* open a new Connection */
                    conn = open();
                }
                else {
                    conn_handle_t h = conns.back();
                    /* cancel Connection expiry */
                    h.alive = -1;
                    conn = h.conn;
                    conns.pop_back();
                }

                Connection *c = new Connection(
                        conn, dbname, async, timeout,
                        [&](Connection* _conn) {
                            free(_conn);
                        });
                return *c;
            }

            template<typename... __Opts>
            void init(const char *con_str, __Opts... opts) {
                auto options = iod::D(opts...);
                configure(options, con_str);
            }

            template <typename __O>
            void configure(__O& opts, const char *constr) {
                if (conn_str) {
                    /* database Connection already initialized */
                    throw std::runtime_error("database already initialized");
                }

                conn_str   = zcstring(constr).dup();
                async      = opts.get(var(ASYNC), false);
                timeout    = opts.get(var(TIMEOUT), -1);
                keep_alive = opts.get(var(EXPIRES), -1);
                dbname     = zcstring{opts.get(var(name), "public")}.dup();

                if (keep_alive > 0 && keep_alive < 3000) {
                    /* limit cleanup to 3 seconds */
                    iwarn("changing db Connection keep alive from %ld ms to 3 seconds", keep_alive);
                    keep_alive = 3000;
                }

                /* open and close connetion to verify the Connection string*/
                PGconn *conn = open();
                PQfinish(conn);
            }

            ~PgSqlDb() {
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
                conn = PQconnectdb(conn_str.data());
                if (conn == nullptr || (PQstatus(conn) != CONNECTION_OK)) {
                    ierror("CONNECT: %s", PQerrorMessage(conn));
                    throw std::runtime_error("connecting to database failed");
                }

                if (async) {
                    /* Connection should be set to non blocking */
                    if (PQsetnonblocking(conn, 1)) {
                        ierror("CONNECT: %s", PQerrorMessage(conn));
                        throw std::runtime_error("connecting to database failed");
                    }
                }

                return conn;
            }

            static coroutine void cleanup(PgSqlDb& db) {
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
                    ltrace(&db, "starting prune with %ld connections", db.conns.size());
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
                    ltrace(&db, "pruned %ld connections", pruned);

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
                        strace("scheduling cleaning...");
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
            Async<bool>   notify{false};
            bool          cleaning{false};
            zcstring      conn_str;
            zcstring      dbname{"public"};
        };

        namespace mw {
            using Postgres = sql::Middleware<PgSqlDb>;
        }
    }
}
#endif //SUIL_PGSQL_HPP
