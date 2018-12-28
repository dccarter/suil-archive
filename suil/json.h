/*
  Copyright (C) 2011 Joseph A. Adams (joeyadams3.14159@gmail.com)
  All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef SUIL_JSON_H
#define SUIL_JSON_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <initializer_list>

#include <iod/json.hh>
#include <suil/utils.h>
#include <suil/logging.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_STRING,
    JSON_NUMBER,
    JSON_ARRAY,
    JSON_OBJECT,
} JsonTag;

typedef struct JsonNode JsonNode;

struct JsonNode
{
    /* only if parent is an object or array (NULL otherwise) */
    JsonNode *parent;
    JsonNode *prev, *next;

    /* only if parent is an object (NULL otherwise) */
    char *key; /* Must be valid UTF-8. */

    JsonTag tag;
    union {
        /* JSON_BOOL */
        bool bool_;

        /* JSON_STRING */
        char *string_; /* Must be valid UTF-8. */

        /* JSON_NUMBER */
        double number_;

        /* JSON_ARRAY */
        /* JSON_OBJECT */
        struct {
            JsonNode *head, *tail;
        } children;
    };
};

/*** Encoding, decoding, and validation ***/

//JsonNode   *json_decode         (const char *json);
//char       *json_encode         (const JsonNode *node);
//char       *json_encode_string  (const char *str);
//char       *json_stringify      (const JsonNode *node, const char *space);
//void        json_delete         (JsonNode *node);
//bool        json_validate       (const char *json);

/*** Lookup and traversal ***/

JsonNode   *json_find_element   (JsonNode *array, int index);
JsonNode   *json_find_member    (JsonNode *object, const char *key);
JsonNode   *json_find_member    (JsonNode *object, const char *key, size_t keyLen);

JsonNode   *json_first_child    (const JsonNode *node);

#define json_foreach(i, object_or_array)            \
	for ((i) = json_first_child(object_or_array);   \
		 (i) != NULL;                               \
		 (i) = (i)->next)

/*** Construction and manipulation ***/

JsonNode *json_mknull(void);
JsonNode *json_mkbool(bool b);
JsonNode *json_mkstring(const char *s);
JsonNode *json_mknstring(const char *s, size_t size);
JsonNode *json_mknumber(double n);
JsonNode *json_mkarray(void);
JsonNode *json_mkobject(void);

void json_append_element(JsonNode *array, JsonNode *element);
void json_prepend_element(JsonNode *array, JsonNode *element);
void json_append_member(JsonNode *object, const char *key, JsonNode *value);
void json_prepend_member(JsonNode *object, const char *key, JsonNode *value);

void json_remove_from_parent(JsonNode *node);

/*** Debugging ***/

/*
 * Look for structure and encoding problems in a JsonNode or its descendents.
 *
 * If a problem is detected, return false, writing a description of the problem
 * to errmsg (unless errmsg is NULL).
 */
bool json_check(const JsonNode *node, char errmsg[256]);

namespace suil {
    namespace json {
        template <typename T>
        struct CopyOut {
            CopyOut(CopyOut&&) = delete;
            CopyOut(const CopyOut&) = delete;
            CopyOut& operator=(CopyOut&&) = delete;
            CopyOut& operator=(const CopyOut&) = delete;
            CopyOut(T& ref)
                : val(ref)
            {}
            T& val;
        };

        struct Object_t {};
        static const Object_t Obj{};
        struct Array_t {};
        static const Array_t Arr{};
        struct Object {
            using ArrayEnumerator  = std::function<bool(suil::json::Object)>;
            using ObjectEnumerator = std::function<bool(const char *key, suil::json::Object)>;

            class iterator {
            public:
                iterator(JsonNode *node) : mNode(node) {}

                iterator operator++();

                bool operator!=(const iterator &other) { return mNode != other.mNode; }

                const std::pair<const char *, Object> operator*() const;

            private:
                JsonNode *mNode;
            };

            using const_iterator = const iterator;

            Object();

            Object(nullptr_t)
            : Object(){}

            Object(const String &str);

            Object(const std::string &str);

            Object(const char *str);

            Object(double d);

            Object(bool b);

            Object(Object_t);

            template<typename... Values>
            Object(Object_t _, Values... values)
                    : Object(_) {
                Ego.set(std::forward<Values>(values)...);
            }

            template <typename ...T>
            Object(const iod::sio<T...>& obj)
                : Object(json::Obj)
            {
                Ego.set(obj);
            }

            template <typename ...T>
            Object(const iod::sio<T...>&& obj)
                : Object(obj)
            {}

            Object(Array_t);

            template<typename... Values>
            Object(Array_t _, Values... values)
                    : Object(_) {
                Ego.push(std::forward<Values>(values)...);
            }

            template<typename T>
            Object(std::vector<T> &vs)
                    : Object(json::Arr) {
                for (auto v: vs)
                    Ego.push(v);
            }

            template<typename T>
            Object(const std::vector<T> &&vs)
                : Object(vs)
            {}

            template<typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
            Object(T t)
                : Object((double) t)
            {}

            Object(const Object &o) noexcept
                : mNode(o.mNode),
                  ref(true)
            { }

            Object(Object &&o) noexcept
                : mNode(o.mNode),
                  ref(o.ref)
            { o.mNode = nullptr; }

            Object &operator=(Object &&o) noexcept;

            void push(Object &&);

            Object push(const Object_t &);

            Object push(const Array_t &);

            template <typename ...T>
            void push(const iod::sio<T...>& obj) {
                Ego.push(json::Obj).set(obj);
            }

            template <typename T>
            void push(const std::vector<T>& v) {
                for(auto& a: v)
                    Ego.push(a);
            }

            template <typename T>
            void push(const iod::Nullable<T>& a) {
                if (!a.isNull)
                    Ego.push(*a);
            }

            template<typename T, typename... Values>
            void push(T t, Values... values) {
                if constexpr (std::is_same<json::Object, T>::value)
                    push(std::move(t));
                else
                    push(Object(t));
                if constexpr (sizeof...(values))
                    push(std::forward<Values>(values)...);
            }

            template <typename ...T>
            void set(const iod::sio<T...>& obj) {
                iod::foreach(obj)|[&](auto& m) {
                    if (!m.attributes().has(iod::_json_skip)) {
                        // only set non-skipped values
                        Ego.set(m.symbol().name(), m.value());
                    }
                };
            }

            template <typename T>
            void set(const char *key, const std::vector<T>& v) {
                Ego.set(key, json::Arr).push(v);
            }

            template <typename T>
            void set(const char *key, const iod::Nullable<T>& a) {
                if (a.isNull)
                    Ego.set(key, nullptr);
                else
                    Ego.set(key, *a);
            }

            void set(const char *key, Object &&);

            Object set(const char *key, const Object_t &);

            Object set(const char *key, const Array_t &);

            template<typename T, typename... Values>
            void set(const char *key, T value, Values... values) {
                if constexpr (std::is_same<json::Object, T>::value)
                    set(key, std::move(value));
                else
                    set(key, Object(value));
                if constexpr (sizeof...(values))
                    set(std::forward<Values>(values)...);
            }

            Object operator[](int index) const;

            Object operator[](const char *key) const;
            Object operator[](const String&& key) const;

            operator bool() const;

            operator String() const {
                return String{(const char *) Ego};
            }

            operator std::string() const {
                return std::string{(const char *) Ego};
            }

            operator const char *() const;

            operator double() const;

            static Object decode(const char *str, size_t &sz);

            void encode(iod::encode_stream &ss) const;

            template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
            inline operator T() const {
                return (T) ((double) Ego);
            }

            template <typename T>
            operator std::vector<T>() const {
                if (Ego.isNull() || Ego.empty())
                    return std::vector<T>{};
                if (!Ego.isArray())
                    throw Exception::create("JSON object is not an array");
                std::vector<T> out;
                Ego|[&](suil::json::Object obj) {
                    // cast to vector's value type
                    T tmp;
                    Ego.copyOut<T>(tmp, obj);
                    out.push_back(std::move(tmp));
                    return false;
                };

                return std::move(out);
            }

            template <typename T>
            operator iod::Nullable<T>() const {
                iod::Nullable<T> ret;
                if (!Ego.isNull() && !Ego.empty()) {
                    T tmp;
                    Ego.copyOut<T>(tmp, Ego);
                    ret = std::move(tmp);
                }
                return std::move(ret);
            }

            template <typename ...T>
            operator iod::sio<T...>() const {
                iod::sio<T...> ret{};
                if (Ego.empty())
                    return std::move(ret);
                iod::foreach(ret) | [&](auto& m) {
                    // find all properties
                    using _Tp = typename std::remove_reference<decltype(m.value())>::type;
                    if (!m.attributes().has(iod::_json_skip)) {
                        // find attribute in current object
                        auto obj = Ego[m.symbol().name()];
                        if (obj.isNull()) {
                            if (!iod::isNullable(m.value())  && !m.attributes().has(iod::_optional))
                                // value cannot be null
                                throw Exception::create("property '", m.symbol().name(), "' cannot be null");
                            else
                                return;
                        }

                        copyOut<_Tp>(m.value(), obj);
                    }
                };

                return std::move(ret);
            }

            template <typename T>
            inline void copyOut(T& out, const Object& obj) const {
                if constexpr (std::is_same<T, suil::String>::value)
                    out = ((String) obj).dup();
                else
                    out = (T) obj;
            }

            template <typename T>
            inline T operator ||(T&& t) {
                if (Ego.empty())
                    return std::forward<T>(t);
                else
                    return (T) Ego;
            }

            inline String operator ||(String&& s) {
                if (Ego.empty())
                    return std::move(s);
                else
                    return ((String) Ego).dup();
            }

            bool empty() const;

            bool isBool() const;

            bool isObject() const;

            bool isNull() const;

            bool isNumber() const;

            bool isString() const;

            bool isArray() const;

            JsonTag type() const;

            void operator|(ArrayEnumerator f) const;

            void operator|(ObjectEnumerator f) const;

            iterator begin();

            iterator end() { return iterator(nullptr); }

            const_iterator begin() const;

            const_iterator end() const { return const_iterator(nullptr); }

            ~Object();

        private suil_ut:

            Object(JsonNode *node, bool ref = true)
                    : mNode(node),
                      ref(ref) {};
            JsonNode *mNode{nullptr};
            bool ref{false};
        };
    }
} // namespace suil

namespace iod {

    template<>
    inline json_internals::json_parser& json_internals::json_parser::fill<suil::String>(suil::String& s) {
        Ego >> '"';

        int start = pos;
        int end = pos;

        while (true) {
            while (!eof() and str[end] != '"')
                end++;

            // Count the prev backslashes.
            int sb = end - 1;
            while (sb >= 0 and str[sb] == '\\')
                sb--;

            if ((end - sb) % 2) break;
            else
                end++;
        }
        s = suil::String(str.data() + start, (size_t)(end-start), false).dup();
        pos = end+1;
        return *this;
    }

    template<>
    inline json_internals::json_parser& json_internals::json_parser::fill<suil::json::Object>(suil::json::Object& o) {
        // just parse into a json object
        size_t tmp = str.size();
        o = suil::json::Object::decode(&str[pos], tmp);
        pos += tmp;
    }

    // Decode \o from a json string \b.
    template<typename ...T>
    inline void json_decode(sio<T...> &o, const suil::OBuffer& b) {
        iod::stringview str(b.data(), b.size());
        json_decode(o, str);
    }

    template<typename O>
    inline void json_decode(std::vector<O>&o, const suil::OBuffer& b) {
        iod::stringview str(b.data(), b.size());
        json_decode(o, str);
    }

    template<typename ...T>
    inline void json_decode(sio<T...> &o, const suil::String& zc_str) {
        iod::stringview str(zc_str.data(), zc_str.size());
        json_decode(o, str);
    }

    template<typename ...T>
    inline void json_decode(sio<T...> &o, const json_string& jstr) {
        iod::stringview str(jstr.str.data(), jstr.str.size());
        json_decode(o, str);
    }

    template<typename S>
    inline void json_decode(suil::json::Object& o, const S& s) {
        size_t size{s.size()};
        o = suil::json::Object::decode(s.data(), size);
        if (size != s.size())
            throw suil::Exception("decoding json string failed at pos ", size);
    }

    namespace json_internals {

        template<typename S>
        inline void json_encode_(const suil::String& s, S &ss) {
            stringview sv(s.data(), s.size());
            json_encode_(sv, ss);
        }

        template<typename S>
        inline void json_encode_(const iod::json_string& s, S &ss) {
            if (!s.str.empty()) {
                ss << s.str;
            }
            else {
                ss << "{}";
            }
        }

        template <typename S>
        inline void json_encode_(const suil::json::Object& o, S &ss) {
            o.encode(ss);
        }
    }

    inline std::string json_encode(const suil::json::Object& o) {
        encode_stream ss;
        json_internals::json_encode_(o, ss);
        return ss.move_str();
    }
}

namespace suil {

    namespace json {
        template<typename O>
        inline std::string encode(const Map <O> &m) {
            if (m.empty()) {
                return "{}";
            }

            iod::encode_stream ss;
            bool first{false};
            for (auto &e: m) {
                if (first) ss << ", ";
                ss << '"' << e.first << "\": ";
                iod::json_internals::json_encode_(e.second, ss);
                first = false;
            }

            return ss.move_str();
        }

        template<typename O>
        inline std::string encode(const O &o) {
            return iod::json_encode(o);
        }

        template<typename S, typename O>
        static bool trydecode(const S &s, O &o) {
            iod::stringview sv(s.data(), s.size());
            try {
                iod::json_decode(o, sv);
                return true;
            }
            catch (...) {
                sdebug("decoding json string failed: %s", Exception::fromCurrent().what());
                return false;
            }
        }

        template<typename S, typename O>
        inline void decode(const S &s, O &o) {
            iod::stringview sv(s.data(), s.size());
            iod::json_decode(o, sv);
        }
    }

    template <typename... T>
    OBuffer& OBuffer::operator<<(const iod::sio<T...>& o) {
        return (Ego << json::encode(o));
    }
}

#endif // SUIL_JSON_H