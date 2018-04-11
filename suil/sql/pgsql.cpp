//
// Created by dc on 3/21/18.
//

#include <suil/sql/pgsql.hpp>

namespace suil::sql {

    PGSQLStatement PgSqlConnection::operator()(zbuffer& req) {
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

    PGSQLStatement PgSqlConnection::operator()(const char *req) {
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

    void PgSqlConnection::destroy(bool dctor) {
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

    bool PgSqlTransaction::begin() {
        if (Ego.valid) return Ego("BEGIN;");
        return false;
    }

    bool PgSqlTransaction::commit() {
        if (Ego.valid) {
            Ego.valid = true;
            return Ego("COMMIT;");
        }
        return false;
    }

    bool PgSqlTransaction::rollback(const char *sp) {
        if (sp) {
            if (!Ego.valid) return false;
            return Ego("ROLLBACK TO SAVEPOINT $1;", sp);
        } else {
            if (Ego.valid) {
                Ego.valid = false;
                return Ego("ROLLBACK;");
            }
        }
        return false;
    }

    bool PgSqlTransaction::savepoint(const char *name) {
        if (Ego.valid) return Ego("SAVEPOINT $1;", name);
        return false;
    }

    bool PgSqlTransaction::release(const char *name) {
        if (Ego.valid) return Ego("RELEASE SAVEPOINT $1", name);
        return false;
    }

    PgSqlDb::~PgSqlDb() {
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

    PgSqlDb::Connection& PgSqlDb::connection() {
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

    PGconn* PgSqlDb::open() {
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

    void PgSqlDb::cleanup(PgSqlDb& db) {
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

    void PgSqlDb::free(Connection* conn) {
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

}