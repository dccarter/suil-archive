//
// Created by dc on 31/05/17.
//

#ifndef SUIL_SYS_HPP
#define SUIL_SYS_HPP
#include <time.h>
#include <endian.h>
#include <fcntl.h>
#include <libgen.h>
#include <uuid/uuid.h>
#include <unordered_map>
#include <sys/param.h>
#include <set>
#include <regex>
#include <signal.h>

#include <boost/utility/string_view.hpp>
#include <iod/json.hh>
#include <iod/parse_command_line.hh>

#include <suil/base.h>
#include <suil/zstring.h>
#include <suil/buffer.h>

namespace suil {

    namespace __internal {
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

        template<typename T>
        using remove_unwired =
        decltype(remove_members_with_attribute(std::declval<T>(), sym(unwire)));
    }

    template <typename Type>
    struct Wrapper {
        Wrapper(Type& w)
            : wrapped(std::move(w))
        {}

        Wrapper(Type&& w)
            : wrapped(std::move(w))
        {}

        Wrapper()
        {}

        operator Type&() {
            return wrapped;
        }
        Type&operator()() {
            return wrapped;
        }
        Type&self() { return wrapped; }
    private:
        Type wrapped{};
    };

    struct null_t {};

    namespace utils {
        /**
         * compute the deadline time in milliseconds given a timeout
         * @param tout the timeout whose deadline will be computed
         * @return the computed deadline, if the given timeout is less
         * than 0, the deadline will be -1
         */
        static inline int64_t after(int64_t tout) {
            return tout < 0 ? -1 : mnow() + tout;
        }

        static inline int64_t tabs(int64_t add) {
            return after(add);
        }
    }

    struct NetworkBuffer {
        using destory_t = std::function<void(void*)>;

        NetworkBuffer&operator=(const NetworkBuffer&) = delete;
        NetworkBuffer(const NetworkBuffer&) = delete;

        NetworkBuffer(void *d, size_t size, size_t off = 0, destory_t dctor = nullptr)
            : data(d),
              offset(off),
              len(size),
              dctor(dctor)
        {}

        NetworkBuffer()
            : NetworkBuffer(nullptr, 0)
        {}

        NetworkBuffer(NetworkBuffer&& other)
            : data(other.data),
              offset(other.offset),
              len(other.size()),
              dctor(other.dctor)
        {
            other.data  = nullptr;
            other.offset = 0;
            other.dctor  = nullptr;
        }

        NetworkBuffer&operator=(NetworkBuffer&& other)
        {
            data = other.data;
            offset = other.offset;
            len = other.size();
            dctor = std::move(other.dctor);

            other.data  = nullptr;
            other.offset = 0;
            other.dctor  = nullptr;
            return Ego;
        }

        inline size_t size() const {
            return len;
        }

        inline void* get() const {
            return ((char *)data + offset);
        }

        inline operator void*() const {
            return get();
        }

        operator bool() const {
            return data != nullptr;
        }

        void destory() {
            if (data != nullptr) {
                if (dctor) {
                    dctor(data);
                }
                else {
                    free(data);
                }
                data = nullptr;
            }
        }

        inline ~NetworkBuffer() {
            destory();
        }

    private:
        void     *data{nullptr};
        size_t   offset{0};
        size_t   len{0};
        destory_t dctor{nullptr};
    };

    namespace utils {
        /**
         * hex representation to binary representation
         * @param c hex representation
         * @return binary representation
         *
         * @throws OutOfRangeError
         */
        uint8_t c2i(char c);

        /**
         * binary representation to hex representation
         * @param c binary representation
         * @param caps true if hex representation should be in CAP's
         * @return hex representation
         */
        char i2c(uint8_t c, bool caps = false);

        namespace __internal {

            template<typename __R>
            struct scoped_res {
                explicit scoped_res(__R &res)
                        : res(res) {}

                scoped_res(const scoped_res &) = delete;

                scoped_res(scoped_res &&) = delete;

                scoped_res &operator=(scoped_res &) = delete;

                scoped_res &operator=(const scoped_res &) = delete;

                ~scoped_res() {
                    res.close();
                }

            private suil_ut:
                __R &res;
            };

            template<typename... A>
            inline void catstr(OBuffer &out, A &... args) {
                (out << ... << args);
            }
        }
    }

 /**
 * declare a scoped resource, creating variable \param n and assigning it to \param x
 * The scoped resource must have a method close() which will be invoked at the end
 * of the scope
 */
#define scoped(n, x) auto& n = x ; suil::utils::__internal::scoped_res<decltype( n )> _##n { n }

        /**
         * case insensitive sapi \class String keyed map
         * @tparam T the type of values held in this map
         */
        template <typename T>
        using CaseMap = std::unordered_map<String, T, hasher, map_case_eq>;
        /**
         * case sensitive sapi \class String keyed map
         * @tparam T the type of values held in this map
         */
        template <typename T>
        using Map = std::unordered_map<String, T, hasher, map_eq>;
        /**
         * case sensive std string keyed map
         * @tparam T the type of values held in this map
         */
        template <typename _T>
        using StdMap = std::unordered_map<std::string, _T, hasher, std_map_eq>;

    namespace utils {

        /**
         * @brief Converts a string to a decimal number. Floating point
         * numbers are not supported
         * @param str the string to convert to a number
         * @param base the numbering base to the string is in
         * @param min expected minimum value
         * @param max expected maximum value
         * @return a number converted from given string
         */
        int64_t strtonum(const String &str, int base, long long int min, long long int max);

        /**
         * compares \param orig string against the other string \param o
         *
         * @param orig the original string to match
         * @param o the string to compare to
         *
         * @return true if the given strings are equal
         */
        inline bool strmatchany(const char *orig, const char *o) {
            return strcmp(orig, o) == 0;
        }

        /**
         * match any of the given strings to the given string \param l
         * @tparam A always const char*
         * @param l the string to match others against
         * @param r the first string to match against
         * @param args comma separated list of other string to match
         * @return true if any ot the given string is equal to \param l
         */
        template<typename... A>
        inline bool strmatchany(const char *l, const char *r, A... args) {
            return strmatchany(l, r) || strmatchany(l, std::forward<A>(args)...);
        }

        /**
         * compare the first value \p l against all the other values (smilar to ||)
         * @tparam T the type of the values
         * @tparam A type of other values to match against
         * @param l the value to match against
         * @param args comma separated list of values to match
         * @return true if any
         */
        template<typename T, typename... A>
        inline bool matchany(const T l, A... args) {
            static_assert(sizeof...(args), "at least 1 argument is required");
            return ((args == l) || ...);
        }

        /**
         * takes two or more parameters and concatenates them into a single stream
         * @tparam T1 type of first parameter
         * @tparam T2 type of second parameter
         * @tparam A variadic types of other parameters
         * @param a first parameter to concatenate
         * @param b second parameter to concatenate
         * @param args all the other parameters
         * @return all the inputs concatenated into a single \class String
         *
         * @note internally this uses an \class OBuffer to build a string so
         * any type that can be streamed to the buffer can be passed as input
         */
        template<typename T1, typename T2, typename... A>
        String catstr(const T1 a, const T2 b, A... args) {
            OBuffer out(32);
            __internal::catstr(out, a, b, args...);
            return String(out);
        }

        /**
         * converts the given string to a number
         * @tparam T the type of number to convert to
         * @param str the string to convert to a number
         * @return the converted number
         *
         * @throws \class Exception when the string is not a valid number
         */
        template<typename T>
        auto to_number(const String& str) -> typename std::enable_if<std::is_integral<T>::value, T>::type {
            return (T)utils::strtonum(str, 10, INT64_MIN, INT64_MAX);
        }

        /**
         * converts the given string to a number
         * @tparam T the type of number to convert to
         * @param str the string to convert to a number
         * @return the converted number
         *
         * @throws runtime_error when the string is not a valid number
         */
        template<typename T>
        auto to_number(const String& str) -> typename std::enable_if<std::is_floating_point<T>::value, T>::type {
            double f;
            char *end;
            f = strtod(str.data(), &end);
            if (errno || *end != '\0')  {
                throw std::runtime_error(errno_s);
            }
            return (T) f;
        }

        /**
         * convert given number to string
         * @tparam T the type of number to convert
         * @param v the number to convert
         * @return converts the number to string using std::to_string
         */
        template<typename T>
        inline auto tostr(T v) -> typename std::enable_if<std::is_arithmetic<T>::value, String>::type {
            auto tmp = std::to_string(v);
            return String(tmp.c_str(), tmp.size(), false).dup();
        }

        /**
         * converts given string to string
         * @param str
         * @return a peek of the given string
         */
        inline String tostr(const String& str) {
            return std::move(str.peek());
        }

        /**
         * converts the given c-style string to a \class String
         * @param str the c-style string to convert
         * @return a \class String which is a duplicate of the
         * given c-style string
         */
        inline String tostr(const char *str) {
            return String{str}.dup();
        }

        /**
         * converts the given std string to a \class String
         * @param str the std string to convert
         * @return a \class String which is a duplicate of the
         * given std string
         */
        inline String tostr(const std::string& str) {
            return String(str.c_str(), str.size(), false).dup();
        }

        /**
         * cast \class String to number of given type
         * @tparam T the type of number to cast to
         * @param data the string to cast
         * @param to the reference that will hold the result
         */
        template <typename T, typename = typename  std::enable_if<std::is_arithmetic<T>::value>::type>
        inline void cast(const String& data, T& to) {
            to = utils::to_number<T>(data);
        }

        /**
         * cast the given \class String to a boolean
         * @param data the string to cast
         * @param to reference to hold the result. Will be true if the string is
         * 'true' or '1'
         */
        inline void cast(const String& data, bool& to) {
            to = (data.compare("1") == 0) ||
                 (data.compare("true") == 0);
        }

        /**
         * casts the given \class String to a c-style string
         * @param data the string to cast
         * @param to the destination pointer, it returns a pointer
         * to the strings buffer
         */
        inline void cast(const String& data, const char*& to) {
            to = data();
        }

        /**
         * casts the given \class String into an std string
         * @param data the string to cast
         * @param to reference to hold the output string
         */
        inline void cast(const String& data, std::string& to) {
            to = std::move(std::string(data.data(), data.size()));
        }

        /**
         * \see String::peek
         * @param data
         * @param to
         */
        inline void cast(const String& data, String& to) {
            to = data.peek();
        }

        /**
         * generete a uuid using the uuid_generate_time_safe function
         * @param id the buffer to hold the generated uuid
         * @return
         */
        inline unsigned char* uuid(uuid_t id) {
            if (uuid_generate_time_safe(id)) {
                return nullptr;
            }
            return id;
        }


        /**
         * parse the given string into a uuid_t object
         *
         * @param str the string parse which must be a valid uuid
         * @param out the uuid_t reference to hold the parsed uuid
         *
         * @return true if the given string parsed successfully as a uuid
         */
        inline bool parseuuid(const String& str, uuid_t& out) {
            return !str.empty() && uuid_parse(str.data(), out);
        }

        /**
         * checks to see if the given string is a valid uuid
         * @param str the string to check as a valid uuid
         * @return true if the string is a valid uuid, flse otherwise
         */
        inline bool uuidvalid(const String& str) {
            uuid_t tmp;
            return parseuuid(str, tmp);
        }

        /**
         * dump the given \param uuid to string. if the \param uuid is null
         * a new uuid is genereted
         *
         * @param uuid the uuid to format as a string
         *
         * @return the string representation of the given uuid
         */
        String uuidstr(uuid_t uuid = nullptr);

        /**
         * utility to find the MIME type of a file from a list
         * of sapi known MIME types
         *
         * @param filename the filename whose MIME time must be decode
         *
         * @return the MIME type if found, otherwise null is returned
         */
        const char *mimetype(const String&& filename);

        void *memfind(void *src, size_t slen, const void *needle, size_t len);

        template <typename T>
        inline T read(void *buf) {
            T t;
            memcpy(&t, buf, sizeof(T));
            return t;
        }

        template <typename T>
        inline void write(void *buf, T v) {
            memcpy(buf, &v, sizeof(T));
        }

        template <typename T>
        inline T env(const char *name, T def = T{}) {
            const char *v = std::getenv(name);
            if (v != nullptr) {
                cast(String(v), def);
            }
            return def;
        }

        inline const char* env(const char *name, const char *def = nullptr) {
            const char *v = std::getenv(name);
            if (v != nullptr) {
                def = v;
            }
            return def;
        }

        inline String env(const char *name, String def = String{}) {
            const char *v = std::getenv(name);
            if (v != nullptr) {
                def = std::move(String{v}.dup());
            }

            return std::move(def);
        }

        /**
         * Load key/value configuration from environment variables
         * @tparam Config The type of configuration to load
         * @param config The configuration object to load into
         * @param prefix the prefix to use when loading confuration
         */
        template <typename Config>
        static void envconfig(Config& config, const char *prefix = "") {
            iod::foreach2(config)|
            [&](auto& m) {
                String key;
                if (prefix)
                    key = utils::catstr(prefix, m.symbol().name());
                else
                    key = String{m.symbol().name()}.dup();
                // convert key to uppercase
                key.toupper();
                m.value() = utils::env(key(), m.value());
            };
        }

        String urlencode(const String& str);

        static inline String urlencode(const char* str) {
            String tmp(str);
            return urlencode(tmp);
        }

        String urldecode(const char *src, size_t len);

        inline String urldecode(const String& str) {
            return urldecode(str.data(), str.size());
        }

        void   randbytes(uint8_t *out, size_t size);

        String randbytes(size_t size);

        size_t hexstr(const uint8_t *, size_t, char *out, size_t len);

        String hexstr(const uint8_t *, size_t);

        void   bytes(const String &str, uint8_t *out, size_t olen);

        String SHA_HMAC256(String &, const uint8_t *, size_t, bool b64 = false);

        static inline String SHA_HMAC256(String &secret, String &msg, bool b64 = false) {
            return utils::SHA_HMAC256(secret, (const uint8_t *) msg.data(), msg.size(), b64);
        }

        static inline String SHA_HMAC256(String &secret, OBuffer &msg, bool b64 = false) {
            return SHA_HMAC256(secret, (const uint8_t *) msg.data(), msg.size(), b64);
        }

        String md5(const uint8_t *, size_t);

        static inline String md5(const char *str) {
            return utils::md5((const uint8_t *) str, strlen(str));
        }

        static inline String md5(const String &zc) {
            return utils::md5((const uint8_t *) zc.data(), zc.size());
        }

        static inline String md5(OBuffer &b) {
            return utils::md5((const uint8_t *) b.data(), b.size());
        }

        String sha256(const uint8_t *data, size_t len, bool b64 = false);

        static inline String sha256(const String &data, bool b64 = false) {
            return utils::sha256((const uint8_t *) data.data(), data.size(), b64);
        }

        static inline String sha256(const OBuffer &data, bool b64 = false) {
            return utils::sha256((const uint8_t *) data.data(), data.size(), b64);
        }

        String AES_Encrypt(String &key, const uint8_t *buf, size_t size, bool b64 = true);

        template<typename __C, typename __Opts>
        inline void apply_options(__C& o, __Opts& opts) {
            /* the target object here is also an sio*/
            if (opts.size()) {
                iod::foreach(opts) |
                [&](auto &m) {
                    /* use given metadata to to set options */
                    m.symbol().member_access(o) = m.value();
                };
            }
        }

        template<typename __C, typename... __Opts>
        inline void apply_config(__C& obj, __Opts... opts) {
            if constexpr (sizeof...(opts) > 0) {
                auto options = iod::D(opts...);
                utils::apply_options(obj, options);
            }
        }

        template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
        inline void sio_zero(T& o) {
            o = 0;
        }

        template<typename T>
        inline void sio_zero(iod::Nullable<T>& o) {
            o.isNull = false;
        }

        template<typename T>
        inline void sio_zero(T& o) {
        }

        inline void sio_zero(bool& o) {
            o = false;
        }

        template<typename... T>
        void sio_zero(iod::sio<T...>& o) {
            iod::foreach(o) | [&](auto m) {
                sio_zero(m.symbol().member_access(o));
            };
        }

        inline int clz(uint64_t num) {
            if (num) {
                return  __builtin_clz(num);
            }
            return 0;
        }

        inline int ctz(uint64_t num) {
            if (num) {
                return  __builtin_ctz(num);
            }
            return sizeof(uint64_t);
        }

        inline uint64_t np2(uint64_t num) {
            return (uint64_t)(1<<(sizeof(num)-utils::ctz(num)));
        }

        inline void closepipe(int p[2]) {
            close(p[0]);
            close(p[1]);
        }

        namespace regex {
            inline bool match(std::regex& reg, const char *data, size_t len = 0) {
                return std::regex_match(std::string(data, len), reg);
            }

            inline bool match(const char *rstr, const char *data, size_t len = 0) {
                if (len == 0) len = strlen(data);
                std::regex reg(rstr);
                return match(reg, data, len);
            }

            inline bool match(const char *rstr, const String& data) {
                std::regex reg(rstr);
                return match(reg, data(), data.size());
            }

            inline bool match(const char *rstr, const OBuffer& data) {
                std::regex reg(rstr);
                return match(reg, data.data(), data.size());
            }
        }
    }

    template <typename T>
    inline String::operator T() const {
        T to;
        utils::cast(*this, to);
        return to;
    };

    template <typename T>
    inline String& String::operator+=(const T t) {
        Ego = utils::catstr(Ego, t);
        return Ego;
    }

    template <typename T>
    inline String String::operator+(const T t) {
        return utils::catstr(Ego, t);
    }

    template <typename Key, typename Value>
    struct KVPair {
        typedef decltype(iod::D(
                prop(key,     Key),
                prop(val,     Value)
        )) Type;
    };
}

inline suil::String operator "" _zc(const char* str, size_t len) {
    if (len) {
        return suil::String{str, len, false}.dup();
    }
    return nullptr;
}

#endif //SUIL_SYS_HPP
