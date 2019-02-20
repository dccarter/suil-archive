//
// Created by dc on 8/2/17.
//

#ifndef SUIL_PGSQL_HPP
#define SUIL_PGSQL_HPP


#include <libpq-fe.h>
#include <netinet/in.h>
#include <deque>
#include <memory>

#include <suil/blob.h>
#include <suil/channel.h>
#include <suil/sql/middleware.h>
#include <suil/sql/orm.h>

namespace suil {
    namespace sql {

        define_log_tag(PGSQL_DB);
        define_log_tag(PGSQL_CONN);

        enum {
            PGSQL_ASYNC = 0x01
        };

        enum pg_types_t {
            BYTEAOID = 17,
            CHAROID  = 18,
            INT8OID  = 20,
            INT2OID  = 21,
            INT4OID  = 23,
            TEXTOID  = 25,
            FLOAT4OID = 700,
            FLOAT8OID = 701,
            INT2ARRAYOID   = 1005,
            INT4ARRAYOID   = 1007,
            TEXTARRAYOID   = 1009,
            FLOAT4ARRAYOID = 1021,
            JSONBOID = 3802

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
            inline Oid type_to_pgsql_oid_type(const String&)
            { return TEXTOID; }
            inline Oid type_to_pgsql_oid_type(const strview&)
            { return TEXTOID; }
            inline Oid type_to_pgsql_oid_type(const iod::json_string&)
            { return JSONBOID; }
            template <size_t N>
            inline Oid type_to_pgsql_oid_type(const Blob<N>&)
            { return BYTEAOID; }
            inline Oid type_to_pgsql_oid_type(const unsigned char&)
            { return CHAROID; }
            inline Oid type_to_pgsql_oid_type(const unsigned  short int&)
            { return INT2OID; }
            inline Oid type_to_pgsql_oid_type(const unsigned int&)
            { return INT4OID; }
            inline Oid type_to_pgsql_oid_type(const unsigned long&)
            { return INT8OID; }
            inline Oid type_to_pgsql_oid_type(const unsigned long long&)
            { return INT8OID; }

            template <typename Args, typename std::enable_if<std::is_integral<Args>::value>::type* = nullptr>
            inline Oid type_to_pgsql_oid_type_number(const std::vector<Args>&)
            { return INT4ARRAYOID; }

            template <typename Args, typename std::enable_if<std::is_floating_point<Args>::value>::type* = nullptr>
            inline Oid type_to_pgsql_oid_type_number(const std::vector<Args>&)
            { return FLOAT4ARRAYOID; }

            template <typename Args>
            inline Oid type_to_pgsql_oid_type(const std::vector<Args>& n)
            { return type_to_pgsql_oid_type_number<Args>(n); }

            inline Oid type_to_pgsql_oid_type(const std::vector<String>&)
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
                    case JSONBOID:
                        return "jsonb";
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

            template <typename Args>
            static char *vhod_to_vnod(unsigned long long& buf, const Args& v) {
                order8b_t& to = (order8b_t &)buf;
                order8b_t& from = (order8b_t &)v;
                to.u32_1 =  htonl(from.u32_2);
                to.u32_2 =  htonl(from.u32_1);

                return (char *) &buf;
            }

            template <typename Args>
            static inline void vhod_to_vnod_append(OBuffer& b,
                  const typename  std::enable_if<std::is_arithmetic<Args>::value, Args>::type & d)
            { b << d; }

            static inline void vhod_to_vnod_append(OBuffer& b, const String &d) {
                b << '"' << d << '"';
            }

            template <typename Args>
            static char *vhod_to_vnod(OBuffer& b, const std::vector<Args>& data) {
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

            template <typename Args>
            static typename std::enable_if<std::is_arithmetic<Args>::value, Args>::type
            __array_value(const char* in, size_t len)
            {
                String tmp(in, len, false);
                Args out{0};
                utils::cast(tmp, out);
                return out;
            };

            static String __array_value(const char* in, size_t len)
            {
                String tmp(in, len, false);
                return std::move(tmp.dup());
            };

            template <typename Args>
            static void parse_array(std::vector<Args>& to, const char *from) {
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

            template <typename Args>
            static void vnod_to_vhod(const char *buf, Args& v) {
                order8b_t &to = (order8b_t &)v;
                order8b_t *from = (order8b_t *) buf;
                to.u32_1 = ntohl(from->u32_2);
                to.u32_2 = ntohl(from->u32_1);
            }
        };

        struct PGSQLStatement : LOGGER(PGSQL_CONN) {

            PGSQLStatement(PGconn *conn, String stmt, bool async, int64_t timeout = -1)
                : conn(conn),
                  stmt(std::move(stmt)),
                  async(async),
                  timeout(timeout)
            {}

            template <typename... Args>
            auto& operator()(Args&&... args) {
                const size_t size = sizeof...(Args)+1;
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
                            free(b);
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
                            (int) sizeof...(Args),
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
                                PQclear(result);
                                break;
                            case PGRES_COMMAND_OK:
                                trace("ASYNC QUERY: continue waiting for results")
                                PQclear(result);
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
                                PQclear(result);
                                err = true;
                        }
                    }

                    if (err) {
                        /* error occurred and was reported in logs */
                        throw Exception::create("query failed: ", PQerrorMessage(conn));
                    }

                    trace("ASYNC QUERY: received %d results", results.results.size());
                }
                else {
                    PGresult *result = PQexecParams(
                            conn,
                            stmt.data(),
                            (int) sizeof...(Args),
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

            template <typename... O>
            inline bool operator>>(iod::sio<O...>& o) {
                if (results.empty()) return false;
                return rowToSio(o);
            }

            template <typename Args>
            bool operator>>(Args& o) {
                if (results.empty()) return false;
                if constexpr (std::is_base_of<iod::MetaType,Args>::value) {
                    // meta
                    return Ego.rowToMeta(o);
                }
                else {
                    // just read results out
                    return results.read(o, 0);
                }
            }

            template <typename Args>
            bool operator>>(std::vector<Args>& l) {
                if (results.empty()) return false;

                do {
                    Args o;
                    if (Ego >> o) {
                        // push result to list of found results
                        l.push_back(std::move(o));
                    }

                } while (results.next());

                return !l.empty();
            }

            template <typename F>
            void operator|(F f) {
                if (results.empty()) return;

                typedef iod::callable_arguments_tuple_t<F> __tmp;
                typedef std::remove_reference_t<std::tuple_element_t<0, __tmp>> Args;
                do {
                    Args o;
                    if (Ego >> o)
                        f(o);
                } while (results.next());

                // reset the iterator
                results.reset();
            }

            inline bool status() {
                return !results.failed();
            }

            inline size_t size() const {
                return results.results.size();
            }

            inline bool empty() const {
                return results.empty();
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

            template <typename... O>
            bool rowToSio(iod::sio<O...> &o) {
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

            template <typename T>
            bool rowToMeta(T& o) {
                if (results.empty()) return false;

                int ncols = PQnfields(results.result());
                bool status{true};
                iod::foreach(suil::sql::__internal::remove_ignore_fields_t<typename T::Schema>()) |
                [&] (auto &m) {
                    if (status) {
                        int fnumber = PQfnumber(results.result(), m.symbol().name());
                        if (fnumber != -1) {
                            // column found
                            status = results.read(m.symbol().member_access(o), fnumber);
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

            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, String& s) {
                val = s.data();
                oid  =  TEXTOID;
                len  = (int) s.size();
                bin  = 1;
                return nullptr;
            }

            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, const String& s) {
                return bind(val, oid, len, bin, norder, *const_cast<String*>(&s));
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

            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, iod::json_string& v) {
                val = v.str.data();
                oid  = JSONBOID;
                len  = (int) v.str.size();
                bin  = 1;
                return nullptr;
            }

            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, const iod::json_string& v) {
                return bind(val, oid, len, bin, norder, *const_cast<iod::json_string*>(&v));
            }

            template <size_t N>
            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, Blob<N>& v) {
                val = (const char *)v.cbegin();
                oid  = BYTEAOID;
                len  = (int) v.size();
                bin  = 1;
                return nullptr;
            }

            template <size_t N>
            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, const Blob<N>& v) {
                return bind(val, oid, len, bin, norder, *const_cast<Blob<N>*>(&v));
            }

            template <typename __V>
            void* bind(const char*& val, Oid& oid, int& len, int& bin, unsigned long long& norder, const std::vector<__V>& v) {
                OBuffer b(32);
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
                            String tmp(data);
                            utils::cast(data, v);
                            return true;
                        }
                    }
                    return false;
                }

                template <typename Args>
                bool read(std::vector<Args>& v, int col) {
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

                inline bool read(iod::json_string& v, int col) {
                    return Ego.read(v.str, col);
                }

                template <size_t N>
                bool read(Blob<N>& v, int col) {
                    if (!empty()) {
                        char *data = PQgetvalue(*it, row, col);
                        if (data != nullptr) {
                            int len = PQgetlength(*it, row, col);
                            memcpy(&v[0], data, MIN((size_t)len, N));
                            return true;
                        }
                    }
                    return false;
                }

                bool read(String& v, int col) {
                    if (!empty()) {
                        char *data = PQgetvalue(*it, row, col);
                        if (data != nullptr) {
                            int len = PQgetlength(*it, row, col);
                            v = std::move(String(data, len, false).dup());
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

                inline bool empty(size_t idx = 0) const {
                    return failure || results.empty() || results.size() <= idx;
                };

                ~pgsql_result() {
                    clear();
                }

                inline void add(PGresult *res) {
                    if (res && PQntuples(res) > 0)
                        results.push_back(res);
                }

                inline void fail() {
                    failure = true;
                }

                inline bool failed() const {
                    return failure;
                }
            };

            String     stmt;
            PGconn       *conn;
            bool         async;
            int64_t      timeout;
            pgsql_result results;
        };

        struct PgSqlConnection: LOGGER(PGSQL_CONN) {
            typedef std::vector<PgSqlConnection*>::iterator active_conns_iterator_t;
            typedef CaseMap <PGSQLStatement> stmt_map_t;
            typedef std::shared_ptr<stmt_map_t>  stmt_map_ptr_t;
            using free_conn_t = std::function<void(PgSqlConnection*)>;

            PgSqlConnection(PGconn *conn, String& dname, bool async, int64_t timeout, free_conn_t free_conn)
                : conn(conn),
                  async(async),
                  timeout(timeout),
                  free_conn(free_conn),
                  dbname(dname)
            {}

            PgSqlConnection(){}

            PgSqlConnection(const PgSqlConnection&) = delete;
            PgSqlConnection&operator=(const PgSqlConnection&) = delete;

            PGSQLStatement operator()(OBuffer& req);

            PGSQLStatement operator()(const char *req);

            inline bool has_table(const char *name) {
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

            template <typename Args>
            bool create_table(const String& name, Args o) {
                OBuffer b(64);
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
                        name(), Exception::fromCurrent().what());
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

            static inline void params(OBuffer& req, int i) {
                req << "$" << i;
            }

            inline ~PgSqlConnection() {
                if (conn) {
                    destroy(true);
                }
            }

        private:

            void destroy(bool dctor = false );

            friend struct PgSqlDb;
            PGconn      *conn{nullptr};
            stmt_map_t  stmt_cache;
            bool        async{false};
            int64_t     timeout{-1};
            free_conn_t free_conn;
            active_conns_iterator_t handle;
            int         refs{1};
            bool        deleting{false};
            suil::String dbname{"public"};
        };
        typedef std::vector<PgSqlConnection*> active_conns_t;

        struct PgSqlTransaction : LOGGER(PGSQL_DB) {
            PgSqlTransaction(PgSqlConnection& conn)
                : conn(conn)
            { begin(); }

            bool begin();

            bool commit();

            bool savepoint(const char* name);

            bool release(const char* name);

            bool rollback(const char* sp = nullptr);

            inline virtual ~PgSqlTransaction() { commit(); }

            template <typename... Args>
            bool operator()(const String&& qstr, Args... args) {
                bool status = false;
                try {
                    // execute the statement
                    PGSQLStatement stmt = conn(qstr());
                    status = stmt(args...).status();
                }
                catch (...) {
                    ierror("'%s' failed: '%s'", qstr(), Exception::fromCurrent().what());
                }

                return status;
            }

        private:
            bool  valid{false};
            PgSqlConnection& conn;
        };

        struct PgSqlDb : LOGGER(PGSQL_DB) {
            typedef PgSqlConnection Connection;
            PgSqlDb()
            {}

            inline Connection& operator()() {
                return connection();
            }

            Connection& connection();

            template<typename... Opts>
            void init(const char *con_str, Opts... opts) {
                auto options = iod::D(opts...);
                configure(options, con_str);
            }

            template <typename O>
            void configure(O& opts, const char *constr) {
                if (conn_str) {
                    /* database Connection already initialized */
                    throw std::runtime_error("database already initialized");
                }

                conn_str   = String(constr).dup();
                async      = opts.get(var(ASYNC), false);
                timeout    = opts.get(var(TIMEOUT), -1);
                keep_alive = opts.get(var(EXPIRES), -1);
                dbname     = String{opts.get(var(name), "public")}.dup();

                if (keep_alive > 0 && keep_alive < 3000) {
                    /* limit cleanup to 3 seconds */
                    iwarn("changing db Connection keep alive from %ld ms to 3 seconds", keep_alive);
                    keep_alive = 3000;
                }

                /* open and close connetion to verify the Connection string*/
                PGconn *conn = open();
                PQfinish(conn);
            }

            ~PgSqlDb();

        private:

            PGconn *open();

            static coroutine void cleanup(PgSqlDb& db);

            void free(Connection* conn);

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
            Channel<bool> notify{false};
            bool          cleaning{false};
            String        conn_str;
            String        dbname{"public"};
        };

        namespace mw {
            using Postgres = sql::Middleware<PgSqlDb>;
        }

        template <typename T>
        using PgsqlOrm = Orm<PgSqlConnection, T>;
        template <typename T>
        using PgsqlMetaOrm = Orm2<PgSqlConnection, T>;
    }
}
#endif //SUIL_PGSQL_HPP
