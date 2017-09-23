//
// Created by dc on 8/2/17.
//

#ifndef SUIL_ORM_HPP
#define SUIL_ORM_HPP

#include "iod/utils.hh"
#include "sys.hpp"
#include "log.hpp"

namespace suil {
    namespace sql {

        namespace _internals {

            auto remove_members_with_attribute = [](const auto& o, const auto& a)
            {
                typedef std::decay_t<decltype(a)> A;
                return iod::foreach2(o) | [&] (auto& m) {
                    typedef typename std::decay_t<decltype(m)>::attributes_type attrs;
                    return iod::static_if<!iod::has_symbol<attrs,A>::value>(
                            [&](){ return m; },
                            [&](){}
                    );
                };
            };

            auto extract_members_with_attribute = [](const auto& o, const auto& a)
            {
                typedef std::decay_t<decltype(a)> A;
                return iod::foreach2(o) | [&] (auto& m) {
                    typedef typename std::decay_t<decltype(m)>::attributes_type attrs;
                    return iod::static_if<iod::has_symbol<attrs,A>::value>(
                            [&](){ return m; },
                            [&](){}
                    );
                };
            };

            template <typename T>
            using remove_auto_increment_t =
            decltype(remove_members_with_attribute(std::declval<T>(), sym(AUTO_INCREMENT)));
            template <typename T>
            using remove_read_only_fields_t =
            decltype(remove_members_with_attribute(std::declval<T>(), sym(READ_ONLY)));
            template <typename T>
            using extract_primary_keys_t =
            decltype(extract_members_with_attribute(std::declval<T>(), sym(PRIMARY_KEY)));
            template <typename T>
            using remove_primary_keys_t =
            decltype(remove_members_with_attribute(std::declval<T>(), sym(PRIMARY_KEY)));
        }

        template<typename __C, typename __O>
        struct orm {
            typedef __O object_t;
            typedef _internals::remove_auto_increment_t<__O> without_auto_inc_type;
            typedef _internals::extract_primary_keys_t<__O>  primary_keys;
            static_assert(!std::is_same<primary_keys, void>::value,
                "ORM requires that at least 1 member of CRUD be a primary key");

            orm(const std::string& tbl, __C& conn)
                : conn(conn.get()),
                  table(tbl)
            {}

            template<typename  __T>
            int find(int id, __T& o)
            {
                buffer_t qb(32);
                bool first = true;

                qb << "select ";
                iod::foreach(o) |
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

            template<typename __T>
            int insert(const __T& o)
            {
                buffer_t qb(32), vb(32);

                qb << "insert into " << table << "(";

                bool first = true;
                int i = 1;
                auto values = iod::foreach(without_auto_inc_type()) |
                [&](auto& m) {
                    if (!first) {
                        qb << ", ";
                        vb << ", ";
                    }
                    first = false;
                    qb << m.symbol.name();
                    __C::params(vb, i++);

                    return m.symbol() = m.symbol().member_access(o);
                };

                qb << ") values (" << vb << ")";

                // execute query
                auto req = conn(qb);
                iod::apply(values, req);

                return req.last_insert_id();
            }

            template <typename __F>
            void forall(__F f) {
                buffer_t qb(32);
                qb << "select * from " << table;
                conn(qb)() | f;
            }

            template <typename __T>
            int update(const __T& o) {
                auto pk = iod::intersect(o, primary_keys());
                static_assert(decltype(pk)::size() > 0,
                        "primary key required in order to update an object.");

                buffer_t qb(32);
                qb << "update " << table << " set ";
                bool first = true;
                int i = 1;

                auto values = iod::foreach2(o) |
                [&](auto& m) {
                    if (!first) {
                        qb << ", ";
                    }
                    first = false;
                    qb << m.symbol().name() << " = ";
                    __C::params(qb, i++);

                    return m;
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

                return req.last_insert_id();
            }

            template <typename __T>
            void remove(__T& o) {
                buffer_t qb(32);

                qb << "delete from " << table << " where ";
                bool first = true;
                int i = 1;

                auto values = iod::foreach(primary_keys()) |
                [&](auto& m) {
                    if (!first) {
                        qb << " and ";
                    }
                    first = false;
                    qb << m.symbol.name() << " = ";
                    __C::params(qb, i++);

                    return m.symbol() = o[m.symbol()];
                };
                // execute query
                iod::apply(values, conn(qb));
            }

            /**
             * @brief check if table associated with orm exists
             * @return true if table exists
             */
            inline bool exists() {
                return conn.has_table(table);
            }

            ~orm() {
                /* return connection reference */
                conn.put();
            }

        private:
            __C& conn;
            const std::string& table;
        };
    }
}
#endif //SUIL_ORM_HPP