//
// Created by dc on 02/11/18.
//

#ifndef SUIL_CONFIG_HPP
#define SUIL_CONFIG_HPP

#include <suil/utils.h>
#include <suil/logging.h>

struct lua_State;

namespace suil {

    define_log_tag(CONFIG);

    struct Config : LOGGER(CONFIG) {
        Config() = default;

        static Config load(const char *path);

        static bool load(const char *path, Config& into);

        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;

        Config(Config&& other) noexcept;
        Config& operator=(Config&& other) noexcept;

        template <typename T>
        T get(const char *key) {
            T tmp{};
            auto what = loadValue(key);
            if (what) {
                unloadValue();
                throw Exception::keyNotFound(what);
            }
            what = get(key, tmp);
            if (what) {
                unloadValue();
                throw Exception::keyNotFound(what);
            }
            return std::move(tmp);
        }

        template <typename T>
        T get(const char* key, T&& def) {
            if (loadValue(key)) {
                // return default
                return std::forward<T>(def);
            }

            T tmp{def};
            Ego.get(key, tmp);
            unloadValue();

            return std::move(tmp);
        }

        bool empty() const {
            return Ego.L == nullptr;
        }

        ~Config();

    private:

        String loadValue(const char *key);
        void unloadValue();

        String get(const char *key, String& out);
        String get(const char *key, bool& out);
        String get(const char *key, double& out);
        String get(const char *key, int& out);
        String get(const char *key, std::string& out);

        lua_State *L{nullptr};
        int        level{0};
    };
}
#endif //SUIL_CONFIG_HPP
