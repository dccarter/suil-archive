#ifndef IOD_JSON_HH_
# define IOD_JSON_HH_

#include <vector>
#include <fstream>
#include <cassert>
#include <tuple>
#include <string>
#include <sstream>
#include <stdexcept>
#include <map>

#include "boost/lexical_cast.hpp"
#include "boost/utility/string_ref.hpp"
#include <iod/sio.hh>
#include <iod/foreach.hh>
#include <iod/symbols.hh>
#include <iod/pow_10.hh>
#include <iod/stringview.hh>

namespace iod {
    template <typename Obj>
    struct Nullable {
        using value_type = Obj;
        Nullable(value_type&& obj)
                : obj(std::move(obj)),
                  isNull(false)
        {}

        Nullable()
        {}

        operator bool() {
            return !isNull;
        }

        inline bool empty() {
            return isNull;
        }

        value_type& operator*() { return obj; }
        value_type* operator->() { return &obj; }
        const value_type& operator*() const { return obj; }
        value_type obj{};
        bool isNull{true};
    };

    template <typename T>
    inline bool isNullable(const Nullable<T>&) { return true; }
    template <typename T>
    inline bool isNullable(const T&) { return false; }

    template <typename T>
    struct Object : std::map<std::string, T> {
    };

    using namespace s;

    // Decode \o from a json string \str.
    template<typename ...T>
    inline void json_decode(sio<T...> &o, const stringview &str);

    // Decode \o from a json string \str.
    template<typename T, typename S>
    inline void json_decode(Nullable<T> &o, const S &str) {
        return json_decode(*o, str);
    }

    // Encode \o into a json string.
    template<typename ...T>
    inline std::string json_encode(const sio<T...> &o);

    // Encode \o into a json string.
    template<typename T>
    inline std::string json_encode(const std::vector<T> &v);

    template<typename T>
    inline std::string json_encode(const Nullable<T> &o) {
        return json_encode(*o);
    }

    // Encode \o into a stream.
    template<typename S, typename ...Tail>
    inline void json_encode(const sio<Tail...> &o, S &stream);

    template<typename S, typename T>
    inline void json_encode(const Nullable<T> &o, S &stream) {
        return json_encode(*o, stream);
    }


    struct json_string {
        std::string str;
        inline bool empty() const {
            return str.empty();
        }
    };

    inline std::string json_encode(const json_string &o);

    struct jsonvalue {};

    namespace json_internals {

        struct external_char_stream {

            external_char_stream(char *buf, int len)
                    : pos_(0),
                      buf_(buf),
                      max_len_(len) {}

            inline void append(const char t) {
                if (pos_ == max_len_)
                    throw std::runtime_error("Maximum json string lenght reached during encoding.");
                buf_[pos_] = t;
                pos_++;
            }

            inline void append(const stringview s) {
                if (pos_ + s.size() > max_len_)
                    throw std::runtime_error("Maximum json string lenght reached during encoding.");
                memcpy(buf_ + pos_, s.data(), s.size());
                pos_ += s.size();
            }

            int size() { return pos_; }

            int pos_;
            char *buf_;
            int max_len_;
        };

        static const int LBS = 500;

        struct stringstream {

            stringstream(int hint_size = 10)
                    : pos_(0) { str_.reserve(hint_size); }

            inline void append(const char t) {
                if (pos_ == LBS)
                    flush();
                buf_[pos_] = t;
                pos_++;
            }

            inline void append(const stringview s) {
                const char *begin = s.data();
                const char *end = s.data() + s.size();

                while (int(end - begin) > (LBS - pos_)) {
                    flush();
                    int to_write = std::min(int(end - begin), LBS);

                    memcpy(buf_, begin, to_write);
                    begin += to_write;
                    pos_ += to_write;
                }

                memcpy(buf_ + pos_, begin, end - begin);

                pos_ += end - begin;
            }

            inline void flush() {
                str_.resize(str_.size() + pos_);
                memcpy(&(str_)[0] + str_.size() - pos_, buf_, pos_);
                pos_ = 0;
            }

            const std::string &str() {
                if (pos_ > 0)
                    flush();
                return str_;
            }

            std::string move_str() {
                if (pos_ > 0)
                    flush();
                return std::move(str_);
            }

            int pos_;
            char buf_[LBS];
            std::string str_;
        };


        template<typename S>
        struct my_ostringstream : public S {
            using S::S;

            inline my_ostringstream &operator<<(const char t) {
                S::append(t);
                return *this;
            }

            inline my_ostringstream &operator<<(const unsigned char t) {
                (*this) << (int) t;
                return *this;
            }

            inline my_ostringstream &operator<<(const stringview &t) {
                S::append(t);
                return *this;
            }

            // Fixme add UTF8 encoding.

            inline my_ostringstream &operator<<(const std::string &t) {
                (*this) << stringview(t);
                return *this;
            }

            inline my_ostringstream &operator<<(const json_string &t) {
                (*this) << t.str;
                return *this;
            }

            inline my_ostringstream &operator<<(const char *t) {
                (*this) << stringview(t, strlen(t));
                return *this;
            }

            inline my_ostringstream &operator<<(const boost::string_ref &t) {
                (*this) << stringview(&t[0], t.size());
                return *this;
            }


            template<typename T>
            my_ostringstream &operator<<(const T &t) {
                std::string s = boost::lexical_cast<std::string>(t);
                (*this) << stringview(s.c_str(), s.size());
                return *this;
            }

            inline my_ostringstream &operator<<(int t) {
                std::string s = std::to_string(t);
                S::append(stringview(s.c_str(), s.size()));
                return *this;
            }
        };

        // Json encoder.
        // =============================================
        template<typename T, typename S, typename  std::enable_if<!std::is_base_of<jsonvalue, T>::value>::type* = nullptr>
        inline void json_encode_(const T &t, S &ss) {
            if constexpr (std::is_base_of<iod::MetaType,T>::value) {
                // meta type
                t.toJson(ss);
            }
            else {
                // any other type
                ss << t;
            }
        }

        template<typename T, typename S, typename  std::enable_if<std::is_base_of<jsonvalue, T>::value>::type* = nullptr>
        inline void json_encode_(const T &t, S &ss) {
            t.encjv(ss);
        }

        template<typename T, typename S>
        inline void json_encode_(const Nullable<T> &t, S &ss) {
            if (!t.empty())
                json_encode_(*t, ss);
            else
                ss << "null";
        }

        template<typename S>
        inline void json_encode_(const char *t, S &ss) {
            ss << '"' << t << '"';
        }

        template<typename S>
        inline void json_encode_(const bool& b, S &ss) {
            ss << (b? "true": "false");
        }

        template<typename S>
        inline void json_encode_(const stringview &s, S &ss) {
            ss << '"';
            ss << s;
            ss << '"';
        }

        template<typename S, typename SS>
        inline void json_encode_symbol(symbol<S>, SS &ss) {
            ss << '"';
            ss << stringview(S().name(), strlen(S().name()));
            ss << '"';
        }

        template<typename S>
        inline void json_encode_(const boost::string_ref &s, S &ss) {
            ss << '"';
            ss << s;
            ss << '"';
        }

        template<typename S>
        inline void json_encode_(const std::string &t, S &ss) {
            json_encode_(t.c_str(), ss);
        }

        // Forward declaration.
        template<typename S, typename ...Tail>
        inline void json_encode_(const sio<Tail...> &o, S &ss);

        template<typename T, typename S>
        inline void json_encode_(const std::vector<T> &array, S &ss) {
            ss << '[';
            for (const auto &t : array) {
                json_encode_(t, ss);
                if (&t != &array.back())
                    ss << ',';
            }
            ss << ']';
        }

        template<typename T, typename S>
        inline void json_encode_(const Object<T>& mp, S& ss) {
            ss << '{';
            bool fst{true};
            for (const auto&e: mp) {
                if (!fst) ss << ", ";
                ss << '"' << e.first << "\":";
                json_encode_(e.second, ss);
                fst = false;
            }
            ss << '}';
        }

        template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
        inline bool json_ignore(const T& t) {
            return t == 0;
        }

        template <typename T,
                typename std::enable_if<(std::is_same<const char*, T>::value||std::is_same<char*, T>::value)>::type* = nullptr>
        inline bool json_is_empty(T v) {
            return v == nullptr || strlen(v) == 0;
        }

        template <typename T,
                typename std::enable_if<!(std::is_same<const char*, T>::value||std::is_same<char*, T>::value)>::type* = nullptr>
        inline bool json_is_empty(const T& v) {
            if constexpr (!std::is_base_of<MetaType,T>::value)
                return v.empty();
            return false;
        }

        template <typename T, typename std::enable_if<!std::is_arithmetic<T>::value>::type* = nullptr>
        inline bool json_ignore(const T& v) {
            return json_is_empty<T>(v);
        }

        template<typename S, typename ...Tail>
        inline void json_encode_(const sio<Tail...> &o, S &ss) {
            ss << '{';
            int i = 0;
            bool first = true;
            foreach(o) | [&](auto m) {
                if (!m.attributes().has(_json_skip)) {
                    /* ignore empty entry */
                    auto val = m.value();
                    if (m.attributes().has(_ignore) && json_ignore<decltype(val)>(val)) return;

                    if (!first) { ss << ','; }
                    first = false;
                    json_encode_symbol(m.attributes().get(_json_key, m.symbol()), ss);
                    ss << ':';
                    json_encode_(m.value(), ss);
                }
                i++;
            };
            ss << '}';
        }

        // Json decoder.
        // =============================================

        template<typename T>
        struct fill_ {
            inline fill_(T &_r) : r(_r) {}

            T &r;
        };

        template<typename T>
        inline fill_<T> fill(T &t) { return fill_<T>(t); }

        template <typename T>
        struct generic_filler{
            generic_filler(T& t)
                    : t(t)
            {}
            T& t;
        };

        struct json_parser {
            struct spaces_ {
            } spaces;

            inline json_parser(std::istringstream &_stream) : str(_stream.str()), pos(0) {}

            inline json_parser(const std::string &_str) : str(_str.c_str(), _str.size()), pos(0) {}

            inline json_parser(const stringview &_str) : str(_str), pos(0) {}

            inline char peak() { return str[pos]; }

            inline char eof() { return pos == str.size(); }

            inline bool eat_null() {
                static const char *null = "null";
                if (str[pos] != 'n') return false;
                if (str[pos+1] != 'u') return false;
                if (str[pos+2] != 'l') return false;
                if (str[pos+3] != 'l') return false;
                pos+=4;
                return true;
            }

            inline char eat_one() { return pos++; }

            template<typename E>
            inline void format_error(E &) {}

            template<typename E, typename T1, typename... T>
            inline void format_error(E &err, T1 a, T... args) {
                err << a;
                format_error(err, args...);
            }

            template<typename... T>
            inline std::runtime_error json_error(T... message) {
                std::stringstream err;

                int w = 20;
                int b = pos > w ? pos - w : 0;
                int e = pos < int(str.size()) - w ? pos + w : int(str.size()) - 1;
                std::string near(str.data() + b, str.data() + e);
                err << std::endl << "Json parse error near " << near << std::endl;
                err << "                      ";
                for (int i = 0; i < pos - b - 1; i++) err << ' ';
                err << "^^^" << std::endl;
                format_error(err, message...);
                return std::runtime_error(err.str());
            }

            struct jdecit {
                jdecit(json_internals::json_parser& p)
                        : p(p),
                          pos(p.pos)
                {}

                operator bool() const {
                    return !empty();
                }

                operator char() const {
                    if (!empty()) return p.str[pos];
                    return '\0';
                }

                jdecit&operator++() {
                    if (empty()) pos++;
                    return *this;
                }

                char next(char term = '\0') {
                    char c = p.str[pos];
                    if (c == term) {
                        return '\0';
                    }
                    pos++;
                    return c;
                }

                const char *start() {
                    return &p.str[p.pos];
                }

                size_t size() {

                    if (_size == 0) {
                        int start = p.pos;
                        int end = p.pos;

                        while (!p.eof()) {
                            while (p.str[end] != '"')
                                end++;

                            // Count the prev backslashes.
                            int sb = end - 1;
                            while (sb >= 0 and p.str[sb] == '\\')
                                sb--;

                            if ((end - sb) % 2) break;
                            else
                                end++;
                        }
                        pos = end;
                        _size = end-start;
                    }

                    return _size;
                }

                size_t remaining() const {
                    return p.str.size() - pos;
                }

                void eat(char c) {
                    p.pos = this->pos;
                    p >> c;
                    pos += 1;
                }

                inline bool eat_null() {
                    int tmp{p.pos};
                    p.pos = this->pos;
                    if (p.eat_null()) {
                        this->pos += 4;
                        return true;
                    }
                    else {
                        p.pos = tmp;
                        return false;
                    }
                }

                void consume(size_t n) {
                    this->pos += n;
                    if (p.pos > p.str.size()) {
                        throw std::runtime_error("custom iterator overflowed");
                    }
                }

                bool empty(char c = '\0') const {
                    return p.str[pos] == '\0' || p.str[pos] != c;
                }

                ~jdecit() {
                    p.pos = pos;
                }

            private:
                json_internals::json_parser& p;
                int pos;
                int _size{0};
            };

            template<typename T>
            inline json_parser& fill(generic_filler<T>& gf) {
                jdecit it(*this);
                T::decjv(it, gf.t);
                return *this;
            }

            inline json_parser &fill(std::string &t) {
                int start = pos;
                int end = pos;
                t.clear();

                char buffer[128];
                int buffer_pos = 0;
                auto flush = [&]() {
                    t.append(buffer, buffer_pos);
                    buffer_pos = 0;
                };
                auto append_char = [&](char c) {
                    if (buffer_pos == sizeof(buffer)) flush();

                    buffer[buffer_pos] = c;
                    buffer_pos++;
                };
                auto append_str = [&](const char *str, int len) {
                    if (buffer_pos + len > int(sizeof(buffer))) flush();
                    if (len < int(sizeof(buffer))) {
                        memcpy(buffer + buffer_pos, str, len);
                        buffer_pos += len;
                    } else {
                        flush();
                        t.append(str, len);
                    }
                };

                while (true) {
                    while (!eof() and str[end] != '"' and str[end] != '\\')
                        end++;

                    if (eof()) throw json_error("Unexpected end of string when parsing a string.");
                    append_str(str.data() + start, end - start);

                    if (str[end] == '"') break;

                    end++;
                    switch (str[end]) {
                        case '\'':
                            append_char('\'');
                            break;
                        case '"':
                            append_char('"');
                            break;
                        case '\\':
                            append_char('\\');
                            break;
                        case '/':
                            append_char('/');
                            break;
                        case 'n':
                            append_char('\n');
                            break;
                        case 'r':
                            append_char('\r');
                            break;
                        case 't':
                            append_char('\t');
                            break;
                        case 'b':
                            append_char('\b');
                            break;
                        case 'f':
                            append_char('\f');
                            break;
                        case 'v':
                            append_char('\v');
                            break;
                        case '0':
                            append_char('\0');
                            break;
                        case 'u':
                            while (true) {
                                if (str.size() < end + 4)
                                    throw json_error("Unexpected end of string when decoding an utf8 character");
                                end++;

                                auto decode_hex_c = [this](char c) {
                                    if (c >= '0' and c <= '9') return c - '0';
                                    else return (10 + c - 'A');
                                };

                                const char *str2 = str.data() + end;
                                char x = (decode_hex_c(str2[0]) << 4) + decode_hex_c(str2[1]);
                                if (x) append_char(x);
                                append_char((decode_hex_c(str2[2]) << 4) + decode_hex_c(str2[3]));

                                end += 4;

                                if (str[end] == '\\' and str[end + 1] == 'u')
                                    end += 1;
                                else break;
                            }
                            break;
                    }

                    start = end;
                }
                flush();
                pos = end;
                return *this;
            }

            inline json_parser &fill(stringview &t) {
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

                t.str = str.data() + start;
                t.len = end - start;
                pos = end;
                return *this;
            }

            template<typename I, int N>
            inline json_parser &fill_int(I &val) {
                int sign = 1;
                if (std::is_signed<I>::value and str[pos] == '-') {
                    sign = -1;
                    eat_one();
                }
                else if (str[pos] == '+') { eat_one(); }

                int end = pos;

                val = 0;

                const char *s = str.data() + pos;

                int fz = 0;
                while (s[fz] == '0') {
                    fz++;
                    end++;
                }

                for (int i = fz; i < N + fz; i++) {
                    if (s[i] < '0' or s[i] > '9') break;
                    val = val * 10 + (s[i] - '0');
                    end++;
                }
                val *= sign;

                if (end == pos) throw json_error("Could not find the expected number.");

                pos = end;
                return *this;
            }

            inline json_parser &fill(float &val) {
                int sign = 1;
                if (str[pos] == '-') {
                    sign = -1;
                    eat_one();
                }
                else if (str[pos] == '+') { eat_one(); }

                float res = 0;

                int ent = 0;
                if (str[pos] != '.')
                    fill_int<int, 10>(ent);

                res = ent;

                if (str[pos] == '.') {
                    eat_one();
                    unsigned int floating = 0;
                    int start = pos;
                    fill_int<unsigned int, 10>(floating);
                    int end = pos;
                    res += float(floating) / pow_10(end - start);
                }

                if (str[pos] == 'e') {
                    eat_one();
                    int exp = 0;
                    fill_int<int, 10>(exp);
                    res *= pow_10(exp);
                }

                val = sign * res;
                return *this;
            }

            inline json_parser &fill(unsigned char &val) {
                int tmp{0};
                fill_int<int, 10>(tmp);
                val = (unsigned char) tmp;
                return *this;
            }

            inline json_parser &fill(int &val) { return fill_int<int, 10>(val); }

            inline json_parser &fill(unsigned int &val) { return fill_int<unsigned int, 10>(val); }

            inline json_parser &fill(bool &t) {
                if ((str.size()>(pos+4)) && (strncmp(&str[pos], "true", 4) ==0)) {
                    pos += 4;
                    t = true;
                }
                else if ((str.size()>(pos+5)) && (strncmp(&str[pos], "false", 5) ==0)) {
                    pos += 5;
                    t =  false;
                }
                else {
                    size_t max = str.size()<pos+10? str.size() : pos+10;
                    auto err = "cannot deserialize '" + str.substr(pos, max).to_std_string() + "' into boolean type";
                    throw std::runtime_error(err);
                }
            }

            template<typename T>
            inline json_parser &fill(T &t) {
                static_assert(!std::is_same<T, const char *>::value,
                              "Cannot json deserialize into an object with const char* members");
                static_assert(!std::is_same<T, const char[]>::value,
                              "Cannot json deserialize into an object with const char[] members");

                int end = pos;
                while (end != str.size() and str[end] != ',' and str[end] != '}' and str[end] != ']') end++;
                t = boost::lexical_cast<std::remove_reference_t<T>>(str.data() + pos, end - pos);
                pos = end;
                return *this;
            }

            // Fill a json_string object with the next json entity.
            inline json_parser &operator>>(json_string &t) {
                int start = pos;
                int end = pos;

                bool in_str = false;
                int parent_level = 0;
                int array_level = 0;
                bool done = false;

                while (!eof() and !done) {
                    if (parent_level == 0 and array_level == 0 and !in_str and
                        (str[pos] == ',' or str[pos] == '}' or str[pos] == ']'))
                        break;

                    if (str[pos] == '"') // strings.
                    {
                        pos++;
                        stringview str;
                        this->fill(str);
                    } else if (str[pos] == '{') // start a json object
                        parent_level++;
                    else if (str[pos] == '}') // end a json object
                        parent_level--;
                    else if (str[pos] == '[') // start a json array
                        array_level++;
                    else if (str[pos] == ']') // end a json array
                        array_level--;

                    pos++; // go to next char.

                    // skip spaces
                    while (!eof() and std::isspace(str[pos])) pos++;
                }

                end = pos;
                t.str.resize(end - start);
                memcpy((void *) t.str.data(), (void *) (str.data() + start), end - start);
                return *this;
            }

            template<typename T>
            inline json_parser &operator>>(fill_<T> &&t) {
                return fill(t.r);
            }

            template<typename T>
            inline json_parser &operator>>(generic_filler<T>&& t) {
                return fill(t);
            }

            inline json_parser &operator>>(char t) {
                if (str[pos] == t) {
                    pos++;
                    return *this;
                } else {
                    if (eof())
                        throw json_error("Expected ", t, " got eof");
                    else
                        throw json_error("Expected ", t, " got ", str[pos]);
                }
            }

            inline json_parser &operator>>(const char *t) {
                int start = pos;
                int end = pos;
                while (!eof() and (t[end - start] == str[end] or str[end] != '"')) end++;

                if (t[end - start] == '\0') {
                    pos = end;
                    return *this;
                } else
                    throw json_error("Expected ", t, " got something else.");
            }


            inline json_parser &operator>>(spaces_) {
                while (!eof() and str[pos] < 33) pos++;
                return *this;
            }

            int line_cpt, char_cpt;
            const char *cur;
            stringview str;
            int pos;
        };

        template<typename S>
        inline void iod_attr_from_json(S *, sio<> &, json_parser &) {
        }

        template<typename S, typename T, typename std::enable_if<!std::is_base_of<jsonvalue, T>::value>::type* = nullptr>
        inline void iod_from_json_(S *, T &t, json_parser &p) {
            if constexpr (std::is_base_of<iod::MetaType,T>::value) {
                // meta types must implement a method fromJson
                t = T::fromJson(p);
            }
            else {
                p >> fill(t);
            }
        }

        template<typename S, typename T, typename std::enable_if<std::is_base_of<jsonvalue, T>::value>::type* = nullptr>
        inline void iod_from_json_(S *, T &t, json_parser &p) {
            generic_filler<T> gf(t);
            p >>  fill(gf) ;
        }

        template<typename S>
        inline void iod_from_json_(S *, std::string &t, json_parser &p) {
            p >> '"' >> fill(t) >> '"';
        }

        template<typename S>
        inline void iod_from_json_(S *, stringview &t, json_parser &p) {
            p >> '"' >> fill(t) >> '"';
        }

        // Parse a json hashmap ordered the field in the object \o.
        template<typename T, typename ...Tail>
        inline void iod_attr_from_json_strict(sio<T, Tail...> &o, json_parser &p) {
            T *attr = &o;

            p >> p.spaces >> '"' >> attr->symbol().name() >> '"' >> p.spaces >> ':';

            iod_from_json_(attr->value(), p);
            if (sizeof...(Tail) != 0)
                p >> p.spaces >> ',';

            iod_attr_from_json(*static_cast<sio<Tail...> *>(&o), p);
        }

        // Parse a json hashmap.
        template<typename O>
        inline void iod_attr_from_json(const sio<> *, O &o, json_parser &p) {}

        template<typename T, typename ...Tail, typename O>
        inline void iod_attr_from_json(const sio<T, Tail...> *, O &o, json_parser &p) {
            p >> p.spaces;

            struct attr_info {
                bool filled;
                stringview name;
            };

            attr_info A[sio<T, Tail...>::size()];

            sio<T, Tail...> scheme;// = *(sio<T, Tail...>*)(42);
            int i = 0;
            foreach(scheme) | [&](const auto &m) {
                A[i].filled = false;
                stringview name(m.symbol().name(), strlen(m.symbol().name()));
                if (m.attributes().has(_json_key)) {
                    const char *new_name = m.attributes().get(_json_key, _json_key).name();
                    name = stringview(new_name, strlen(new_name));
                }
                A[i].name = name;
                i++;
            };

            while (p.peak() != '}') {
                stringview attr_name;
                p >> p.spaces >> '"' >> fill(attr_name) >> '"' >> p.spaces >> ':' >> p.spaces;

                int i = 0;
                bool attr_found = false;
                foreach(scheme) | [&](auto &m) {
                    if (!m.attributes().has(_json_skip) and !attr_found and attr_name == A[i].name) {
                        try {
                            if (!p.eat_null())
                                iod_from_json_(&m.value(), m.symbol().member_access(o), p);
                        }
                        catch (const std::exception &e) {
                            std::stringstream ss;
                            ss << "Error when decoding json attribute " << attr_name.to_std_string() << ": "
                               << e.what();
                            throw std::runtime_error(ss.str());
                        }
                        A[i].filled = true;
                        attr_found = true;
                    }
                    i++;
                };
                // if !attr_found, throw an error.
                if (!attr_found)
                    throw std::runtime_error(std::string("json_decode error: unexpected key ") +
                                             attr_name.to_std_string());

                p >> p.spaces;
                if (p.peak() == ',')
                    p.eat_one();
                else
                    break;
            }

            if (p.peak() != '}') {
                throw p.json_error("Expected } got ", p.peak());
            }

            i = 0;
            foreach(scheme) | [&](auto &m) {
                if (!m.attributes().has(_json_skip) and !m.attributes().has(_optional) and !A[i].filled)
                    throw std::runtime_error(std::string("json_decode error: missing field ") +
                                             m.symbol().name());
                i++;
            };
        }

        // Parse an array.
        template<typename S, typename T>
        inline void iod_from_json_(S *, std::vector<T> &array, json_parser &p) {
            p >> '[' >> p.spaces;
            if (p.peak() == ']') {
                p >> ']';
                return;
            }

            array.clear();
            while (p.peak() != ']') {
                T t;
                p >> p.spaces;
                iod_from_json_((typename S::value_type *) 0, t, p);
                array.push_back(std::move(t));
                p >> p.spaces;
                if (p.peak() == ']')
                    break;
                else
                    p >> ',';
            }

            p >> ']';
        }

        // Parse an map.
        template<typename S, typename T>
        inline void iod_from_json_(S *, Object<T>&obj, json_parser &p) {
            p >> '{' >> p.spaces;
            if (p.peak() == '}') {
                p >> '}';
                return;
            }

            obj.clear();
            while (p.peak() != '}') {
                stringview attr_name;
                p >> p.spaces >> '"' >> fill(attr_name) >> '"' >> p.spaces >> ':' >> p.spaces;
                T t;
                p >> p.spaces;
                iod_from_json_((typename S::value_type *) 0, t, p);
                obj.emplace(attr_name.to_std_string(), std::move(t));
                p >> p.spaces;
                if (p.peak() == '}')
                    break;
                else
                    p >> ',';
            }

            p >> '}';
        }

        template<typename O, typename... T>
        inline void iod_from_json_(sio<T...> *s, O &o, json_parser &p) {
            p >> p.spaces >> '{';
            iod_attr_from_json(s, o, p);
            p >> p.spaces >> '}';
        }

        template<typename S, typename O>
        inline void iod_from_json_(const S *s, O &o, const std::string &str) {
            json_parser p(str);
            if (str.size() > 0)
                iod_from_json_(s, o, p);
            else
                throw std::runtime_error("Empty string.");
        }

        template<typename S>
        inline void iod_from_json_(S *, json_string &s, json_parser &p) {
            p >> s;
        }

        template<typename S, typename T>
        inline void iod_from_json_(S *, Nullable<T> &t, json_parser &p) {
            iod_from_json_((T *)0, *t, p);
            t.isNull = false;
        }
    }

    using jdecit = json_internals::json_parser::jdecit;

    template<typename ...Tail>
    inline void json_decode(sio<Tail...> &o, const stringview &str, int &n_read) {
        if (o.size() == 0) return;
        json_internals::json_parser p(str);
        if (str.size() > 0)
            iod_from_json_((sio<Tail...> *) 0, o, p);
        else
            throw std::runtime_error("Empty string.");
        n_read = p.pos;
    }

    template<typename ...Tail>
    inline void json_decode(sio<Tail...> &o, const stringview &str) {
        if (o.size() == 0) return;
        json_internals::json_parser p(str);
        if (str.size() > 0)
            iod_from_json_((sio<Tail...> *) 0, o, p);
        else
            throw std::runtime_error("Empty string.");
    }

    template<typename ...Tail>
    inline void json_decode(sio<Tail...> &o, std::istringstream &stream) {
        if (o.size() == 0) return;
        json_internals::json_parser p(stream);
        if (stream.str().size() > 0)
            iod_from_json_((sio<Tail...> *) 0, o, p);
        else
            throw std::runtime_error("Empty string.");
    }

    template<typename S, typename O>
    inline void json_decode(O &o, std::istringstream &stream) {
        if (o.size() == 0) return;
        json_internals::json_parser p(stream);
        if (stream.str().size() > 0)
            iod_from_json_((S *) 0, o, p);
        else
            throw std::runtime_error("Empty string.");
    }

    template<typename S, typename O>
    inline void json_decode(O &o, const stringview &str) {
        if (S::size() == 0) return;
        json_internals::json_parser p(str);
        if (str.size() > 0)
            iod_from_json_((S *) 0, o, p);
        else
            throw std::runtime_error("Empty string.");
    }


    template<typename O>
    inline void json_decode(json_string &o, const stringview &str) {
        json_internals::json_parser p(str);
        if (str.size() > 0)
            iod_from_json_((json_string *) 0, o, p);
        else
            throw std::runtime_error("Empty string.");
    }

    template<typename O>
    inline void json_decode(std::vector<O>&o, const stringview &str) {
        json_internals::json_parser p(str);
        if (str.size() > 0)
            iod_from_json_((std::vector<O> *) 0, o, p);
        else
            throw std::runtime_error("Empty string.");
    }

    template<typename O>
    inline void json_decode(json_string &o, std::istringstream &stream) {
        json_internals::json_parser p(stream);
        if (stream.str().size() > 0)
            iod_from_json_((json_string *) 0, o, p);
        else
            throw std::runtime_error("Empty string.");
    }

    template<typename ...Tail>
    inline std::string json_encode(const sio<Tail...> &o) {
        json_internals::my_ostringstream<json_internals::stringstream> ss;
        json_internals::json_encode_(o, ss);
        return ss.move_str();
    }

    template<typename ...Tail>
    inline int json_encode(const sio<Tail...> &o, char *buf, int len) {
        json_internals::my_ostringstream<json_internals::external_char_stream> ss(buf, len);
        json_internals::json_encode_(o, ss);
        return ss.size();
    }

    inline std::string json_encode(const json_string &o) {
        return o.str;
    }

    template<typename S, typename ...Tail>
    inline void json_encode(const sio<Tail...> &o, S &stream) {
        json_internals::json_encode_(o, stream);
    }

    template<typename T>
    inline std::string json_encode(const std::vector<T> &v) {
        json_internals::my_ostringstream<json_internals::stringstream> ss;
        json_internals::json_encode_(v, ss);
        return ss.move_str();
    }
    using encode_stream = json_internals::my_ostringstream<json_internals::stringstream>;

    template <typename T>
    inline void zero(Nullable<T>& t) {
        t.isNull = true;
    }

    namespace json{
        using namespace json_internals;
        using parser   = json_parser;
        using jstream  = encode_stream;
    }
}

#endif
