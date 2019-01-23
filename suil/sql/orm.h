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

        template<typename Connection, typename Type, typename Schema = Type>
        struct Orm {
            typedef Type object_t;
            typedef __internal::remove_auto_increment_t<Schema> WithoutAutoIncrement;
            typedef __internal::extract_primary_keys_t<Schema>  PrimaryKeys;

            template <typename O2>
            using WithoutIgnore2 = __internal::remove_ignore_fields_t<O2>;

            typedef __internal::remove_ignore_fields_t<Schema>  WithoutIgnore;

            static_assert(!std::is_same<PrimaryKeys, void>::value,
                "ORM requires that at least 1 member of CRUD be a primary key");

            Orm(const suil::String&& tbl, Connection& conn)
                : conn(conn.get()),
                  table(std::move(tbl))
            {}

            template<typename  T>
            bool find(int id, T& o)
            {
                OBuffer qb(32);
                bool first = true;
                qb << "select ";

                iod::foreach2(WithoutIgnore()) |
                [&](auto& m) {
                    if (!first) {
                        qb << ", ";
                    }
                    first = false;
                    qb << m.symbol().name();
                };
                qb << " from " << table << " where id = ";
                Connection::params(qb, 1);

                // execute query
                return conn(qb)(id) >> o;
            }

            template <typename V>
            bool has(const char *col, const V& v) {
                OBuffer qb(32);
                qb << "SELECT COUNT("<<col << ") FROM " << table << " WHERE " << col << "= ";
                Connection::params(qb, 1);
                int count{0};
                conn(qb)(v) >> count;
                return count != 0;
            }

            template<typename T>
            bool insert(const T& o)
            {
                OBuffer qb(32), vb(32);

                qb << "insert into " << table << "(";

                bool first = true;
                int i = 1;
                typedef decltype(WithoutAutoIncrement()) __tmp;
                auto values = iod::foreach2(WithoutIgnore2<__tmp>()) |
                [&](auto& m) {
                    if (!first) {
                        qb << ", ";
                        vb << ", ";
                    }
                    first = false;
                    qb << m.symbol().name();
                    Connection::params(vb, i++);

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
                    WithoutIgnore tmp{};
                    return conn.create_table(table, tmp);
                }
                return false;
            }

            // create table if not exist and seed with users
            bool cifne(std::vector<Type>& seed) {
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

            std::vector<Type> getAll() {
                std::vector<Type> data;
                Ego.forall([&](Type& o) {
                    data.emplace_back(std::move(o));
                });

                return std::move(data);
            }

            template <typename T>
            bool update(const T& o)
            {
                using _Schema = typename std::conditional<std::is_base_of<iod::MetaType, T>::value, typename T::Schema,T>::type;
                auto pk = iod::intersect(_Schema(), PrimaryKeys());
                static_assert(decltype(pk)::size() > 0,
                        "primary key required in order to update an object.");

                OBuffer qb(32);
                qb << "update " << table << " set ";
                bool first = true;
                int i = 1;
                typedef decltype(WithoutAutoIncrement()) __tmp;
                auto values = iod::foreach2(WithoutIgnore2<__tmp>()) |
                [&](auto& m) {
                    if (!first) {
                        qb << ", ";
                    }
                    first = false;
                    qb << m.symbol().name() << " = ";
                    Connection::params(qb, i++);

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
                    Connection::params(qb, i++);

                    return m.symbol() = m.symbol().member_access(o);
                };

                // execute query
                auto req = conn(qb);
                iod::apply(values, pks, req);

                return req.status();
            }

            template <typename T>
            void remove(T& o) {
                OBuffer qb(32);

                qb << "delete from " << table << " where ";
                bool first = true;
                int i = 1;

                auto values = iod::foreach(PrimaryKeys()) |
                [&](auto& m) {
                    if (!first) {
                        qb << " and ";
                    }
                    first = false;
                    qb << m.symbol().name() << " = ";
                    Connection::params(qb, i++);

                    return m.symbol() = m.symbol().member_access(o);
                };
                // execute query
                iod::apply(values, conn(qb));
            }

            ~Orm() {
                /* return Connection reference */
                conn.put();
            }



        private:
            Connection& conn;
            suil::String table{nullptr};
        };

        template<typename Connection, typename Type>
        using Orm2 = Orm<Connection, Type, typename Type::Schema>;
    }
}
#endif //SUIL_ORM_HPP
