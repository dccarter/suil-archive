//
// Created by dc on 14/12/17.
//

#ifndef SUIL_KVSTORE_HPP
#define SUIL_KVSTORE_HPP

#include <suil/redis.hpp>
#include <suil/wire.hpp>

namespace suil {

    struct redis_store {

        redis_store(suil::redis::base_client& cli)
            : cli(cli)
        {}

        template <typename __K, size_t VN>
        bool get(const __K key, suil::blob_t<VN>& value) {
            suil::redis::response resp = cli.send("GET", key);
            if (resp) {
                suil::zcstring vx{resp.peek(0)};
                value.fromhex(vx);
                return true;
            }
            return false;
        };

        template <typename __K, size_t VN>
        bool set(const __K key, const suil::blob_t<VN>& value) {
            return cli.send("SET", key, value).status();
        };

        template <typename __K>
        bool get(const __K key, suil::breadboard& bb) {
            bb.reset();
            suil::redis::response resp = cli.send("GET", key);
            if (resp) {
                suil::zcstring vx{resp.peek(0)};
                return bb.fromhexstr(vx);
            }
            return false;
        };

        template <typename __K>
        bool exists(const __K key) {
            auto resp = cli("EXISTS", key);
            if (resp) {
                return  (int) resp == 1;
            }
            return false;
        }

        template <typename __K>
        bool set(const __K key, const suil::breadboard& bb) {
            return cli.send("SET", key, bb).status();
        };

        suil::redis::base_client&operator()() {
            return cli;
        }

    private:
        suil::redis::base_client& cli;
    };
}

#endif //SUIL_KVSTORE_HPP
