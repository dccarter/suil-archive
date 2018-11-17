//
// Created by dc on 8/2/17.
//

#ifndef SUIL_ORM_HPP
#define SUIL_ORM_HPP

#include <iod/utils.hh>
#include <suil/utils.h>
#include <suil/logging.h>

namespace suil {
    namespace sql {

        namespace __internal {

            template <typename T>
            using remove_auto_increment_t =
            decltype(suil::__internal::remove_members_with_attribute(std::declval<T>(), sym(AUTO_INCREMENT)));
            template <typename T>
            using remove_ignore_fields_t =
            decltype(suil::__internal::remove_members_with_attribute(std::declval<T>(), sym(ignore)));
            template <typename T>
            using remove_read_only_fields_t =
            decltype(suil::__internal::remove_members_with_attribute(std::declval<T>(), sym(READ_ONLY)));
            template <typename T>
            using extract_primary_keys_t =
            decltype(suil::__internal::extract_members_with_attribute(std::declval<T>(), sym(PRIMARY_KEY)));
            template <typename T>
            using remove_primary_keys_t =
            decltype(suil::__internal::remove_members_with_attribute(std::declval<T>(), sym(PRIMARY_KEY)));
        }

        template<typename __C, typename __O>
        struct Orm {
            typedef __O object_t;
            typedef __internal::remove_auto_increment_t<__O> without_auto_inc_type;
            typedef __internal::extract_primary_keys_t<__O>  primary_keys;
            template <typename __O2>
            using without_ignore2 = __internal::remove_ignore_fields_t<__O2>;
            typedef __internal::remove_ignore_fields_t<__O>  without_ignore;
            static_assert(!std::is_same<primary_keys, void>::value,
                "ORM requires that at least 1 member of CRUD be a primary key");

            Orm(const suil::String&& tbl, __C& conn)
                : conn(conn.get()),
                  table(std::move(tbl))
            {}

            template<typename  __T>
            bool find(int id, __T& o)
            {
                OBuffer qb(32);
                bool first = true;

                qb << "select ";
                iod::foreach2(without_ignore()) |
                [&](auto& m) {
                    if (!first) {
                        qb << ", ";
                    }
                    first = false;
                    qb << m.symbol().name();
                };
                qb << " from " << table << " where id = ";
                __C::params(qb, 1);

                // execute query
                return conn(qb)(id) >> o;
            }

            template <typename V>
            bool has(const char *col, const V& v) {
                OBuffer qb(32);
                qb << "SELECT COUNT("<<col << ") FROM " << table << " WHERE " << col << "= ";
                __C::params(qb, 1);
                int count{0};
                conn(qb)(v) >> count;
                return count != 0;
            }

            template<typename __T>
            bool insert(const __T& o)
            {
                OBuffer qb(32), vb(32);

                qb << "insert into " << table << "(";

                bool first = true;
                int i = 1;
                typedef decltype(without_auto_inc_type()) __tmp;
                auto values = iod::foreach2(without_ignore2<__tmp>()) |
                [&](auto& m) {
                    if (!first) {
                        qb << ", ";
                        vb << ", ";
                    }
                    first = false;
                    qb << m.symbol().name();
                    __C::params(vb, i++);

                    return m.symbol() = m.symbol().member_access(o);
                };

                qb << ") values (" << vb << ")";

                // execute query
                auto req = conn(qb);
                iod::apply(values, req);

                return req.status();
            }

            // initialize a table for this table
            bool cifne() {
                // pass the request to respective connection
                if (!conn.has_table(table())) {
                    without_ignore tmp{};
                    return conn.create_table(table, tmp);
                }
                return false;
            }

            // create table if not exist and seed with users
            bool cifne(std::vector<__O>& seed) {
                // create table if does not exist
                if (Ego.cifne()) {
                    // if created seed with data
                    if (!seed.empty()) {
                        for(auto& data: seed) {
                            if(!Ego.insert(data)) {
                                // inserting seed user failed
                                sdebug("inserting seed entry into table '%' failed", table());
                                return false;
                            }
                        }
                    }
                    return true;
                }
                return false;
            }

            template <typename __F>
            void forall(__F f) {
                OBuffer qb(32);
                qb << "select * from " << table;
                conn(qb)() | f;
            }

            std::vector<__O> getAll() {
                std::vector<__O> data;
                Ego.forall([&](__O& o) {
                    data.emplace_back(std::move(o));
                });

                return std::move(data);
            }

            template <typename __T>
            bool update(const __T& o) {
                auto pk = iod::intersect(o, primary_keys());
                static_assert(decltype(pk)::size() > 0,
                        "primary key required in order to update an object.");

                OBuffer qb(32);
                qb << "update " << table << " set ";
                bool first = true;
                int i = 1;
                typedef decltype(without_auto_inc_type()) __tmp;
                auto values = iod::foreach2(without_ignore2<__tmp>()) |
                [&](auto& m) {
                    if (!first) {
                        qb << ", ";
                    }
                    first = false;
                    qb << m.symbol().name() << " = ";
                    __C::params(qb, i++);

                    return m.symbol() = m.symbol().member_access(o);
                };

                qb << " where ";
                first = true;

                auto pks = iod::foreach2(pk) |
                [&](auto& m) {
                    if (!first) {
                        qb << " and ";
                    }
                    first = false;
                    qb << m.symbol().name() << " = ";
                    __C::params(qb, i++);

                    return m.symbol() = m.value();
                };

                // execute query
                auto req = conn(qb);
                iod::apply(values, pks, req);

                return req.status();
            }

            template <typename __T>
            void remove(__T& o) {
                OBuffer qb(32);

                qb << "delete from " << table << " where ";
                bool first = true;
                int i = 1;

                auto values = iod::foreach(primary_keys()) |
                [&](auto& m) {
                    if (!first) {
                        qb << " and ";
                    }
                    first = false;
                    qb << m.symbol().name() << " = ";
                    __C::params(qb, i++);

                    return m.symbol() = o[m.symbol()];
                };
                // execute query
                iod::apply(values, conn(qb));
            }

            ~Orm() {
                /* return Connection reference */
                conn.put();
            }

        private:
            __C& conn;
            suil::String table{nullptr};
        };
    }
}
#endif //SUIL_ORM_HPP
