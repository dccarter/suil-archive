//
// Created by dc on 02/11/18.
//

#include <lua/lua.hpp>
#include <suil/config.h>

namespace suil {

    Config::Config(suil::Config &&other) noexcept
        : L(other.L)
    { other.L = nullptr; }

    Config& Config::operator=(suil::Config &&other) noexcept
    {
        Ego.L= other.L;
        other.L = nullptr;
        return Ego;
    }

    Config::~Config()
    {
        if (Ego.L) {
            // lua_State open
            lua_close(Ego.L);
            Ego.L = nullptr;
        }
    }

    Config Config::load(const char *path)
    {
        Config config{};
        ltrace(&config, "loading configuration from path %s", path);
        config.L = luaL_newstate();
        luaL_openlibs(config.L);
        if (luaL_loadfile(config.L, path) || lua_pcall(config.L, 0, 0, 0)) {
            // loading configuration file failed
            throw Exception::create(
                    "loading configuration file '", path, "' failed - ", lua_tostring(config.L, -1));
        }

        return std::move(config);
    }

    bool Config::load(const char *path, suil::Config &into)
    {
        try {
            into = Config::load(path);
            return true;
        }
        catch (...) {
            // loading configuration failed
            lwarn(&into, "%s", Exception::fromCurrent().what());
            return false;
        }
    }

    static String lua_getStack(int& level, lua_State *L, const char *str)
    {
        auto tmp = String{str}.dup();
        auto keyChain = tmp.split(".");
        level = 0;
        for (auto& key : keyChain) {
            // iterate the key
            if (level == 0)
                lua_getglobal(L, key);
            else
                lua_getfield(L, -1, key);

            if (lua_isnil(L, -1)) {
                // value does not exist
                return utils::catstr("field '", key, "' of '", str, "' does not exist");
            }
            // advance to selected object
            level++;
        }
        return nullptr;
    }

    String Config::loadValue(const char *key)
    {
        if (Ego.L == nullptr) {
            // invalid configuration loaded
            return utils::catstr("cannot load key '", key, "', not configuration loaded");
        }

        // get the key value to the top of lua stack
        return lua_getStack(Ego.level, Ego.L, key);
    }

    void Config::unloadValue()
    {
        // pop all loaded values
        lua_pop(Ego.L, Ego.level);
    }

    json::Object Config::operator[](const char *key)
    {
        auto what = loadValue(key);
        if (what) {
            unloadValue();
            throw Exception::keyNotFound(what());
        }

        json::Object tmp;
        switch (lua_type(L, -1)) {
            case LUA_TBOOLEAN:
                tmp = json::Object(lua_toboolean(L, -1));
                break;
            case LUA_TNUMBER:
                tmp = json::Object(lua_tonumber(L, -1));
                break;
            case LUA_TSTRING:
                tmp = json::Object(lua_tostring(L, -1));
                break;
            case LUA_TNIL:
            case LUA_TNONE:
                break;
            default:
                unloadValue();
                throw Exception::create("unsupported typed found at key '", key, "'");
        }
        unloadValue();
        return tmp;
    }

    String Config::getValue(const char *key, int &out)
    {
        if (!lua_isnumber(L, -1)) {
            out = 0;
            return utils::catstr("'", key, "' is not a number");
        }
        out = (int) lua_tonumber(L, -1);
        return nullptr;
    }

    String Config::getValue(const char *key, bool &out)
    {
        out = (bool) lua_toboolean(Ego.L, -1);
        return nullptr;
    }

    String Config::getValue(const char *key, double &out)
    {
        if (!lua_isnumber(L, -1)) {
            out = 0;
            return utils::catstr("'", key, "' is not a number");
        }
        out = (double) lua_tonumber(L, -1);
        return nullptr;
    }

    String Config::getValue(const char *key, suil::String &out)
    {
        if (!lua_isstring(L, -1)) {
            out = nullptr;
            return utils::catstr("'", key, "' is not a string");
        }
        out = String{lua_tostring(L, -1)};
        return nullptr;
    }

    String Config::getValue(const char *key, std::string &out)
    {
        if (!lua_isstring(L, -1)) {
            out = nullptr;
            return utils::catstr("'", key, "' is not a string");
        }
        out = std::string(lua_tostring(L, -1));
        return nullptr;
    }
}