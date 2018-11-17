//
// Created by dc on 8/2/17.
//

#ifndef SUIL_SQLITE_HPP
#define SUIL_SQLITE_HPP

#include <memory>
#include <sqlite3.h>

#include <iod/sio.hh>
#include <iod/callable_traits.hh>
#include <suil/sql/middleware.h>

namespace suil {
    namespace sql {

        define_log_tag(SQLITE_CONN);
        define_log_tag(SQLITE_DB);

        struct SQLiteStmt {

            static void free_sqlite3_stmt(void *s) {
                sqlite3_finalize((sqlite3_stmt *) s);
            }

            typedef std::shared_ptr<sqlite3_stmt> stmt_ptr;

            SQLiteStmt()
            {}

            SQLiteStmt(sqlite3* db, sqlite3_stmt* stmt)
                : m_db(db),
                  stmt_(stmt),
                  stmt_ptr_(stmt_ptr(stmt, free_sqlite3_stmt))
            {}

            template <typename... A>
            bool operator>>(iod::sio<A...>& o) {
                if (empty())
                    return false;
                row_to_sio(o);
                return true;
            }

            template <typename T>
            bool operator>>(T& o) {
                if (empty())
                    return false;
                this->readColumn(0, o);
                return true;
            }

            inline int last_insert_id() {
                return sqlite3_last_insert_rowid(m_db);
            }

            bool empty() {
                return last_ret_ != SQLITE_ROW;
            }

            template<typename... T>
            auto& operator()(T&&... args) {
                sqlite3_reset(stmt_);
                sqlite3_clear_bindings(stmt_);
                int i = 1;
                iod::foreach(std::forward_as_tuple(args...))
                | [&] (auto& m) {
                    int err  = this->bind(stmt_, i, m);
                    if (err != SQLITE_OK) {
                        throw std::runtime_error(
                                std::string("sqlite_bind: ") + sqlite3_errmsg(m_db));
                    }
                    i++;
                };

                last_ret_ = sqlite3_step(stmt_);
                if (last_ret_ != SQLITE_ROW and last_ret_ != SQLITE_DONE) {
                    throw std::runtime_error(sqlite3_errstr(last_ret_));
                }

                return *this;
            }

            template <typename __F>
            void operator|(__F f) {
                while (last_ret_ == SQLITE_ROW) {
                    typedef iod::callable_arguments_tuple_t<__F> tp;
                    typedef std::remove_reference_t<std::tuple_element_t<0, tp>> T;
                    T o;
                    row_to_sio(o);
                    f(o);
                    last_ret_ = sqlite3_step(stmt_);
                }
            }

            template <typename... __A>
            void row_to_sio(iod::sio<__A...>& o) {
                int ncols = sqlite3_column_count(stmt_);
                int filled[sizeof...(__A)];
                for (unsigned i = 0; i < sizeof...(__A); i++)
                    filled[i] = 0;

                for (int i = 0; i < ncols; i++) {
                    const char* cname = sqlite3_column_name(stmt_, i);
                    bool found = false;
                    int j = 0;
                    iod::foreach(o)
                    | [&] (auto& m) {
                        if (!found and !filled[j] && !strcmp(cname, m.symbol().name())) {
                            this->readColumn(i, m.value());
                            found = true;
                            filled[j] = 1;
                        }
                        j++;
                    };
                }
            }

            template <typename __V>
            void append_to(__V& v) {
                (*this) | [&v](typename __V::value_type& o) {
                    v.push_back(o);
                };
            }

            template <typename __T>
            struct typed_iterator {
                template <typename __F>
                void operator|(__F f) const {
                    (*_this) | [&f](__T& t) {
                        f(t);
                    };
                };

                SQLiteStmt* _this;
            };

            void readColumn(int pos, int& v) {
                v = sqlite3_column_int(stmt_, pos);
            }

            void readColumn(int pos, float& v) {
                v = sqlite3_column_double(stmt_, pos);
            }

            void readColumn(int pos, double& v) {
                v = sqlite3_column_double(stmt_, pos);
            }

            void readColumn(int pos, int64_t& v) {
                v = sqlite3_column_int64(stmt_, pos);
            }

            void readColumn(int pos, std::string& v) {
                auto str = sqlite3_column_text(stmt_, pos);
                auto n = sqlite3_column_bytes(stmt_, pos);
                v = std::move(std::string((const char*)str, n));
            }

            void readColumn(int pos, String & v) {
                auto str = sqlite3_column_text(stmt_, pos);
                auto n = sqlite3_column_bytes(stmt_, pos);
                v = String((const char*)str, n, false).dup();
            }

            int bind(sqlite3_stmt* stmt, int pos, double d) const {
                return sqlite3_bind_double(stmt, pos, d);
            }
            int bind(sqlite3_stmt* stmt, int pos, int i) const {
                return sqlite3_bind_int(stmt, pos, i);
            }
            int bind(sqlite3_stmt* stmt, int pos, null_t) const {
                return sqlite3_bind_null(stmt, pos);
            }
            int bind(sqlite3_stmt* stmt, int pos, const char *s) const {
                return sqlite3_bind_text(stmt, pos, s, strlen(s), nullptr);
            }
            int bind(sqlite3_stmt* stmt, int pos, std::string& s) const {
                return sqlite3_bind_text(stmt, pos, s.data(), s.size(), nullptr);
            }
            int bind(sqlite3_stmt* stmt, int pos, String& s) const {
                return sqlite3_bind_text(stmt, pos, s.data(), s.size(), nullptr);
            }
            int bind(sqlite3_stmt* stmt, int pos, strview& s) const {
                return sqlite3_bind_text(stmt, pos, s.data(), s.size(), nullptr);
            }

            sqlite3*        m_db;
            sqlite3_stmt*   stmt_;
            stmt_ptr        stmt_ptr_;
            int             last_ret_;
        };

        struct SQLiteConnetion : LOGGER(SQLITE_CONN) {
            static void free_sqlite3_db(void *p) {
                sqlite3_close_v2((sqlite3*) p);
            }

            typedef std::shared_ptr<sqlite3>     m_dbptrt;
            typedef Map <SQLiteStmt>     stmt_map_t;
            typedef std::shared_ptr<stmt_map_t>  stmt_map_ptr_t;

            SQLiteConnetion()
                :m_db(nullptr),
                 m_stmtCache(new stmt_map_t())
            {}

            void connect(const char *filename, int flags = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE) {
                int r = sqlite3_open_v2(filename, &m_db, flags, nullptr);
                if (r != SQLITE_OK) {
                    throw std::runtime_error(
                            std::string("sqlite3_open_v2: ") + filename + " " +
                            sqlite3_errstr(r));
                }

                m_dbptr = m_dbptrt(m_db, free_sqlite3_db);
            }

            template <typename E>
            inline void formart_error(E&) const {}

            template <typename E, typename T1, typename... Args>
            inline void formart_error(E& err, T1 a, Args... args) const {
                err << a;
                formart_error(err, args...);
            }

            SQLiteStmt operator()(OBuffer& req) {
                // create temporary zero copy str, does
                // not own buffer
                String tmp(req, false);
                trace("%s", (char *)req);

                auto it = m_stmtCache->find(tmp);
                if (it != m_stmtCache->end()) {
                    return it->second;
                }

                sqlite3_stmt *stmt;
                int err = sqlite3_prepare_v2(m_db, req.data(), req.size(), &stmt, nullptr);
                if (err != SQLITE_OK) {
                    throw std::runtime_error(
                            std::string("sqlite3_prepare_v2 : ") +
                            sqlite3_errmsg(m_db) + ", statement: " + (char *) req
                    );
                }

                // take buffer
                String key(req);
                auto ret = m_stmtCache->insert(it,
                                               std::make_pair(std::move(key), SQLiteStmt(m_db, stmt)));

                return ret->second;
            }

            SQLiteStmt operator()(const char *req) {
                // create temporary zero copy str, does
                // not own buffer
                String tmp(req);
                trace("%s", req);

                auto it = m_stmtCache->find(tmp);
                if (it != m_stmtCache->end()) {
                    return it->second;
                }

                // forced to create string
                OBuffer b(0);
                b << req;
                return (*this)(b);
            }

            bool has_table(String& name) {
                int has = 0;
                auto stmt = (*this)("SELECT COUNT(*) FROM sqlite_master WHERE type'table' AND name='?'");
                return (stmt(name) >> has) && has == 1;
            }

            inline SQLiteConnetion& get() {
                return (*this);
            }

            inline void put() {}

            static inline void params(OBuffer& req, int /* UNUSED */) {
                req << "?";
            }

            template <typename __T>
            static inline const char* type_to_string(const __T&, std::enable_if<std::is_integral<__T>::value>* = 0)
            { return "INTEGER"; }

            template <typename __T>
            static inline const char* type_to_string(const __T&, std::enable_if<std::is_floating_point<__T>::value>* = 0)
            { return "REAL"; }

            static inline const char* type_to_string(const std::string&)
            { return "TEXT"; }

            ~SQLiteConnetion() {
                close();
            }

        private:
            inline void close() {
                if (m_db) {
                    sqlite3_close_v2(m_db);
                    m_db = nullptr;
                }
            }

            sqlite3*          m_db;
            m_dbptrt          m_dbptr;
            stmt_map_ptr_t    m_stmtCache;
        };

        struct SQLiteDb : LOGGER(SQLITE_DB) {
            typedef SQLiteConnetion Connection;

            SQLiteDb()
            {}

            operator Connection& () {
                return connection();
            }

            Connection& connection() {
                return conn;
            }

            template <typename...O>
            inline void init(const char *db, O... opts) {
                auto options = iod::D(opts...);
                configure(options, db);
            }

            template <typename O>
            void configure(O& opts, const char *db) {
                path = ::strdup(db);

                if (path != nullptr) {
                    /* database not already initialized */
                    conn.connect(path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
                    if (opts.has(sym(SYNCHRONOUS))) {
                        OBuffer qb(32);
                        qb << "PRAGMA synchronous = " << opts.get(sym(SYNCHRONOUS), 2);
                        conn(qb);
                    }
                    trace("SQLite: `%s` Connection initialized", path);
                }
                else {
                    trace("SQLite: %s already initialized", path);
                }
            }

            ~SQLiteDb() {
                if (path) {
                    free(path);
                    path = nullptr;
                }
            }

            Connection conn;
            char       *path{nullptr};
        };

        namespace  mw {
            using SQLite = sql::Middleware<SQLiteDb>;
        }
    }
}
#endif //SUIL_SQLITE_HPP
