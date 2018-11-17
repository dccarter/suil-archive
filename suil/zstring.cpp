//
// Created by dc on 09/11/18.
//

#include "utils.h"

namespace suil {

    String::String()
            : m_str(nullptr),
              m_len(0),
              m_own(false)
    {}

    String::String(const char *str)
            : m_cstr(str),
              m_len((uint32_t) (str ? strlen(str) : 0)),
              m_own(false)
    {}

    String::String(const strview str)
            : m_cstr(str.data()),
              m_len((uint32_t) (str.size())),
              m_own(false)
    {}

    String::String(const std::string &str, bool own)
            : m_cstr(own ? ::strndup(str.data(), str.size()) : str.data()),
              m_len((uint32_t) (str.size())),
              m_own(own)
    {}

    String::String(const char *str, size_t len, bool own)
            : m_cstr(str),
              m_len((uint32_t) len),
              m_own(own)
    {}

    String::String(char c, size_t n)
            : m_str((char *) ::malloc(n+1)),
              m_own(true),
              m_len((uint32_t) n)
    {
        memset(m_str, c, n);
        m_str[n] = '\0';
    }

    String::String(OBuffer &b, bool own) {
        m_len = (uint32_t) b.size();
        Ego.m_own = (uint8_t) ((own && (b.data() != nullptr)) ? 1 : 0);
        if (Ego.m_own) {
            m_str = b.release();
        } else {
            m_str = b.data();
        }
    }

    String::String(String &&s) noexcept
            : m_str(s.m_str),
              m_len(s.m_len),
              m_own(s.m_own),
              m_hash(s.m_hash) {
        s.m_str = nullptr;
        s.m_len = 0;
        s.m_own = false;
        s.m_hash = 0;
    }

    String &String::operator=(String &&s) noexcept {
        m_str = s.m_str;
        m_len = s.m_len;
        m_own = s.m_own;
        m_hash = s.m_hash;

        s.m_str = nullptr;
        s.m_len = 0;
        s.m_own = false;
        s.m_hash = 0;

        return *this;
    }

    String::String(const String &s)
            : m_str(s.m_own ? strndup(s.m_str, s.m_len) : s.m_str),
              m_len(s.m_len),
              m_own(s.m_own),
              m_hash(s.m_hash) {}

    String& String::operator=(const String &s) {
        m_str = s.m_own ? strndup(s.m_str, s.m_len) : s.m_str;
        m_len = s.m_len;
        m_own = s.m_own;
        m_hash = s.m_hash;
        return *this;
    }

    String String::dup() const {
        if (m_str == nullptr || m_len == 0)
            return nullptr;
        return std::move(String(strndup(m_str, m_len), m_len, true));
    }

    String String::peek() const {
        // this will return a dup of the string but as
        // just a reference or simple not owner
        return std::move(String(m_cstr, m_len, false));
    }

    inline void String::toupper() {
        for (int i = 0; i < m_len; i++) {
            m_str[i] = (char) ::toupper(m_str[i]);
        }
    }

    inline void String::tolower() {
        for (int i = 0; i < m_len; i++) {
            m_str[i] = (char) ::tolower(m_str[i]);
        }
    }

    bool String::empty() const {
        return m_str == nullptr || m_len == 0;
    }

    ssize_t String::find(char ch) const {
        if (Ego.m_len > 0) {
            int i{0};
            while (Ego.m_str[i++] != ch)
                if (i == Ego.m_len) return  -1;
            return i-1;
        }
        return -1;
    }

    ssize_t String::rfind(char ch) const {
        ssize_t index{-1};
        if (Ego.m_len > 0) {
            auto i = (int) Ego.m_len;
            while (Ego.m_str[--i] != ch)
                if (i == 0) return -1;
            return i;
        }
        return -1;
    }

    bool String::operator==(const String &s) const {
        if (m_str != nullptr && s.m_str != nullptr) {
            return (m_len == s.m_len) && ((m_str == s.m_str) ||
                                        (strncmp(m_str, s.m_str, m_len) == 0));
        }
        return m_str == s.m_str;
    }

    const char *String::c_str(const char *nil) const {
        if (m_cstr == nullptr || m_len == 0)
            return nil;
        return m_cstr;
    }

    String String::substr(size_t from, size_t nchars, bool zc) const {
        auto fits = (ssize_t) (Ego.m_len - from);
        nchars = nchars? nchars : fits;
        if (nchars && fits >= nchars) {
            auto tmp = String{&Ego.m_cstr[from], nchars, false};
            return zc? std::move(tmp) : tmp.dup();
        }
        return String{};
    }

    String::~String() {
        if (m_str && m_own) {
            ::free(m_str);
        }
        m_str = nullptr;
        m_own = false;
    }

    size_t hasher::operator()(const String& s) const {
        auto& ss = (String &) s;
        if (ss.m_hash == 0) {
            strview sv(s.data(), s.size());
            ss.m_hash = std::hash<strview>()(sv);
        }
        return s.m_hash;
    }

    inline size_t hasher::hash(const char *ptr, size_t len) const {
        strview sv(ptr, len);
        return std::hash<strview>()(sv);
    }

    String String::strip(char strip, bool ends) {
        char		*s, *p, *e;

        OBuffer b(Ego.size());
        void *tmp = b;
        p = (char *)tmp;
        s = Ego.data();
        e = Ego.data() + (Ego.size()-1);
        while (strip == *s) s++;
        while (strip == *e) e--;
        e++;

        if (ends) {
            memcpy(p, s, (e-s));
            b.seek(e-s);
        }
        else {
            for (; s < e; s++) {
                if (*s == strip)
                    continue;
                *p++ = *s;
            }
            b.seek(p - (char *)tmp);
        }

        // String will take ownership of the buffer
        return std::move(String(b));
    }

    const std::vector<char *> String::split(const char *delim) {
        int		count;
        char		*ap = nullptr, *ptr = Ego.data(), *eptr = ptr + Ego.size();
        std::vector<char*> out;

        count = 0;
        for (ap = strsep(&ptr, delim); ap != nullptr; ap = strsep(&ptr, delim)) {
            if (*ap != '\0' && ap != eptr) {
                out.push_back(ap);
                ap++;
                count++;
            }
        }

        return std::move(out);
    }
}

#ifdef unit_test
#include <catch/catch.hpp>

using namespace suil;

TEST_CASE("String test cases", "[common][String]") {
    // tests the String API's
    SECTION("Constructing a string") {
        // test sting constructor
        const char *cstr{"Hello World"};
        std::string str(cstr);
        strview     sv(str);

        String s1{};
        REQUIRE(s1.m_str == nullptr);
        REQUIRE(s1.m_len == 0);
        REQUIRE_FALSE(s1.m_own);

        String s2{cstr};
        REQUIRE(s2.m_str == cstr);
        REQUIRE(s2.m_len == 11);
        REQUIRE_FALSE(s2.m_own);

        String s3{str};
        REQUIRE(s3.m_str == str.data());
        REQUIRE(s3.m_len == 11);
        REQUIRE_FALSE(s3.m_own);

        String s4{sv};
        REQUIRE(s4.m_str == sv.data());
        REQUIRE(s4.m_len == 11);
        REQUIRE_FALSE(s4.m_own);

        String s5('h', 12);
        REQUIRE(s5.m_str != nullptr);
        REQUIRE(s5.m_len == 12);
        REQUIRE(strcmp(s5.m_cstr, "hhhhhhhhhhhh") == 0);
        REQUIRE(s5.m_own);

        String s6(str, true);
        REQUIRE(s6.m_str != nullptr);
        REQUIRE(s6.m_str != str.data());
        REQUIRE(s6.m_len == 11);
        REQUIRE(s6.m_own);

        OBuffer ob{16};
        ob << str;
        String s7(ob, false);
        REQUIRE(s7.m_str != nullptr);
        REQUIRE(s7.m_str == ob.data());
        REQUIRE(s7.m_len == 11);
        REQUIRE_FALSE(s7.m_own);

        String s8(ob);
        REQUIRE(s8.m_str != nullptr);
        REQUIRE(s8.m_len == 11);
        REQUIRE(s8.m_own);
        // buffer should have been released
        REQUIRE(ob.empty());
    }

    SECTION("String move/copy construct/assign") {
        // tests the string move/copy constructors and assignment operators
        std::string str{"Hello World"};
        String s1{str};
        String s2{str, true};

        WHEN("Moving the string") {
            // test string move constructor and assignment operator
            String tmp1{str};
            String s3(std::move(tmp1));
            REQUIRE(tmp1.m_str == nullptr);
            REQUIRE(tmp1.m_len == 0);
            REQUIRE_FALSE(tmp1.m_own);
            REQUIRE(s3.m_str == str.data());
            REQUIRE(s3.m_len == 11);
            REQUIRE_FALSE(s3.m_own);

            String tmp2{str, true};
            String s4(std::move(tmp2));
            REQUIRE(tmp2.m_str == nullptr);
            REQUIRE(tmp2.m_len == 0);
            REQUIRE_FALSE(tmp2.m_own);
            REQUIRE(s4.m_str != nullptr);
            REQUIRE(s4.m_str != str.data());
            REQUIRE(s4.m_len == 11);
            REQUIRE(s4.m_own);

            String tmp3{str};
            String s5 = std::move(tmp3);
            REQUIRE(tmp3.m_str == nullptr);
            REQUIRE(tmp3.m_len == 0);
            REQUIRE_FALSE(tmp3.m_own);
            REQUIRE(s5.m_str == str.data());
            REQUIRE(s5.m_len == 11);
            REQUIRE_FALSE(s5.m_own);

            String tmp4{str, true};
            String s6 = std::move(tmp4);
            REQUIRE(tmp4.m_str == nullptr);
            REQUIRE(tmp4.m_len == 0);
            REQUIRE_FALSE(tmp4.m_own);
            REQUIRE(s6.m_str != nullptr);
            REQUIRE(s6.m_str != str.data());
            REQUIRE(s6.m_len == 11);
            REQUIRE(s6.m_own);
        }

        WHEN("Copying the string") {
            // test string move constructor and assignment operator
            String tmp1{str};
            String s3(tmp1);
            REQUIRE(tmp1.m_str != nullptr);
            REQUIRE(tmp1.m_len == 11);
            REQUIRE_FALSE(tmp1.m_own);
            REQUIRE(s3.m_str == tmp1.data());
            REQUIRE(s3.m_len == 11);
            REQUIRE_FALSE(s3.m_own);

            String tmp2{str, true};
            String s4(tmp2);
            REQUIRE(tmp2.m_str != nullptr);
            REQUIRE(tmp2.m_len == 11);
            REQUIRE(tmp2.m_own);
            REQUIRE(s4.m_str != nullptr);
            REQUIRE(s4.m_str != tmp2.data());
            REQUIRE(s4.m_len == 11);
            REQUIRE(s4.m_own);

            String tmp3{str};
            String s5 = tmp3;
            REQUIRE(tmp3.m_str != nullptr);
            REQUIRE(tmp3.m_len == 11);
            REQUIRE_FALSE(tmp3.m_own);
            REQUIRE(s5.m_str == tmp3.data());
            REQUIRE(s5.m_len == 11);
            REQUIRE_FALSE(s5.m_own);

            String tmp4{str, true};
            String s6 = tmp4;
            REQUIRE(tmp4.m_str != nullptr);
            REQUIRE(tmp4.m_len == 11);
            REQUIRE(tmp4.m_own);
            REQUIRE(s6.m_str != nullptr);
            REQUIRE(s6.m_str != tmp4.data());
            REQUIRE(s6.m_len == 11);
            REQUIRE(s6.m_own);
        }
    }

    SECTION("String duplicate & peek", "[dup][peek]") {
        // test the two methods String::[dup/peek]
        WHEN("duplicating string") {
            String s1{"Hello World"};
            REQUIRE_FALSE(s1.m_own);
            REQUIRE_FALSE(s1.m_str == nullptr);
            REQUIRE(s1.m_len == 11);

            String s2{s1.dup()};
            REQUIRE_FALSE(s2.m_str == nullptr);
            REQUIRE_FALSE(s2.m_str == s1.m_str);
            REQUIRE(s2.m_own);
            REQUIRE(s2.m_len == 11);
            REQUIRE_FALSE(s1.m_str == nullptr);
            REQUIRE(strcmp(s2.m_str, s1.m_str) == 0);

            String s3{s2.dup()};
            REQUIRE_FALSE(s3.m_str == nullptr);
            REQUIRE_FALSE(s3.m_str == s2.m_str);
            REQUIRE(s3.m_own);
            REQUIRE(s3.m_len == 11);
            REQUIRE_FALSE(s2.m_str == nullptr);
            REQUIRE(strcmp(s3.m_str, s2.m_str) == 0);

            String s4{};
            String s5{s4.dup()};
            REQUIRE(s5.m_str == nullptr);
            REQUIRE(s5.m_len == 0);
            REQUIRE_FALSE(s5.m_own);
        }

        WHEN("peeking string") {
            String s1{"Hello World"};
            REQUIRE_FALSE(s1.m_own);
            REQUIRE_FALSE(s1.m_str == nullptr);
            REQUIRE(s1.m_len == 11);

            String s2{s1.peek()};
            REQUIRE_FALSE(s2.m_str == nullptr);
            REQUIRE(s2.m_str == s1.m_str);
            REQUIRE_FALSE(s2.m_own);
            REQUIRE(s2.m_len == 11);
            REQUIRE_FALSE(s1.m_str == nullptr);
            REQUIRE(strcmp(s2.m_str, s1.m_str) == 0);

            String s3{s2.peek()};
            REQUIRE_FALSE(s3.m_str == nullptr);
            REQUIRE(s3.m_str == s2.m_str);
            REQUIRE_FALSE(s3.m_own);
            REQUIRE(s3.m_len == 11);
            REQUIRE_FALSE(s2.m_str == nullptr);

            String s4{};
            String s5{s4.peek()};
            REQUIRE(s5.m_str == nullptr);
            REQUIRE(s5.m_len == 0);
            REQUIRE_FALSE(s5.m_own);
        }
    }

    SECTION("String to uppercase/lowercase", "[toupper][tolower]") {
        // test String::[toupper/tolower]
        std::string str{"Hello WORLD"};
        String s1{str, true};
        String s2{str, true};
        s1.toupper();
        s2.tolower();
        REQUIRE(strcmp(s1.m_str, "HELLO WORLD") == 0);
        REQUIRE(strcmp(s2.m_str, "hello world") == 0);
    }

    SECTION("String find", "[find][rfind]") {
        // test String::[find/rfind]
        String s1{"qThis is a string, gnirts a si sihTx"}; // 36
        REQUIRE(s1.find('i')  == 3);
        REQUIRE(s1.rfind('i') == 32);
        REQUIRE(s1.find('x')  == 35);
        REQUIRE(s1.rfind('q') == 0);
        REQUIRE(s1.find('z')  == -1);
        REQUIRE(s1.rfind('z') == -1);
    }

    SECTION("String substring/chunk", "[substr][chunk]") {
        // test String::[substr/chunk]
        WHEN("taking a substring") {
            String s1{"Hello long string Xx Hello long string"}; // 38

            // test invalid cases first
            auto s2 = s1.substr(38);
            REQUIRE(s2.m_str == nullptr);
            REQUIRE(s2.m_len == 0);
            auto s3 = s1.substr(7, 38);
            REQUIRE(s3.m_str == nullptr);
            REQUIRE(s3.m_len == 0);

            // test some valid cases
            auto s4 = s1.substr(0, 5);              // takes only 5 characters from string
            REQUIRE_FALSE(s4.m_str == nullptr);
            REQUIRE(s4.m_len == 5);
            REQUIRE_FALSE(s4.m_own);
            REQUIRE(s4.m_str == s1.m_str);
            auto s5 = s1.substr(7);                // takes substring from 7 to end of string
            REQUIRE_FALSE(s5.m_str == nullptr);
            REQUIRE(s5.m_len == 31);
            REQUIRE_FALSE(s5.m_own);
            REQUIRE(s5.m_str == &s1.m_str[7]);
            auto s6 = s1.substr(6, 11, false);     // takes substring from 7, taking 11 characters and copying the substring
            REQUIRE_FALSE(s6.m_str == nullptr);
            REQUIRE(s6.m_len == 11);
            REQUIRE(s6.m_own);
            REQUIRE_FALSE(s6.m_str == &s1.m_str[7]);
            REQUIRE(strcmp("long string", s6.m_str) == 0);
        }

        WHEN("taking a chunk of the string") {
            String s1{"One Xx Two xX Three"}; //19

            // invalid case's first
            auto s2 = s1.chunk('Z');
            REQUIRE(s2.m_str == nullptr);
            REQUIRE(s2.m_len == 0);
            auto s3 = s1.chunk('z', false);
            REQUIRE(s3.m_str == nullptr);
            REQUIRE(s3.m_len == 0);

            // valid case, chunk never copies
            auto  s4 = s1.chunk('x');
            REQUIRE_FALSE(s4.m_str == nullptr);
            REQUIRE_FALSE(s4.m_own);
            REQUIRE(s4.m_len == 14);
            REQUIRE(&s1.m_str[5] == s4.m_str);
            REQUIRE(strcmp(&s1.m_str[5], s4.m_str) == 0);
            auto  s6 = s1.chunk('x', true);
            REQUIRE_FALSE(s6.m_str == nullptr);
            REQUIRE_FALSE(s4.m_own);
            REQUIRE(s6.m_len == 8);
            REQUIRE(&s1.m_str[11] == s6.m_str);
            REQUIRE(strcmp(&s1.m_str[11], s6.m_str) == 0);
        }
    }

    SECTION("String split", "[split]") {
        // tests String::split
        std::string str{"One,two,three,four,five||one,,two,three||,,,,"}; // 44

        String s1{str, true};
        auto p1 = s1.split("/");
        REQUIRE(p1.size() == 1);
        REQUIRE(p1[0] == s1.m_str);
        REQUIRE(strlen(p1[0]) == 45);

        auto p2 = s1.split("||");
        REQUIRE(p2.size() == 3);
        REQUIRE(strlen(p2[0]) == 23);
        REQUIRE(p2[0] == s1.m_str);
        REQUIRE(strlen(p2[1]) == 14);
        REQUIRE(p2[1] == &s1.m_str[25]);
        REQUIRE(strlen(p2[2]) == 4);
        REQUIRE(p2[2] == &s1.m_str[41]);

        String s2{p2[0]};
        auto p3 = s2.split(",");
        REQUIRE(p3.size() == 5);
        REQUIRE(strcmp(p3[0], "One")   == 0);
        REQUIRE(strcmp(p3[1], "two")   == 0);
        REQUIRE(strcmp(p3[2], "three") == 0);
        REQUIRE(strcmp(p3[3], "four")  == 0);
        REQUIRE(strcmp(p3[4], "five")  == 0);

        String s3{p2[1]};
        auto p4 = s3.split(",");
        REQUIRE(p4.size() == 3);
        REQUIRE(strcmp(p4[0], "one")   == 0);
        REQUIRE(strcmp(p4[1], "two")   == 0);
        REQUIRE(strcmp(p4[2], "three") == 0);

        String s4{p2[2]};
        auto p5 = s4.split(",");
        REQUIRE(p5.empty());
    }

    SECTION("String strip/trim", "[strip][trim]") {
        // tests String::[strip/trim]
        String s1{"   String String Hello   "}; // 25

        auto s2 = s1.strip('q');                // nothing is stripped out if it not in the string
        REQUIRE(s2.m_own);
        REQUIRE(s2.m_len == 25);
        REQUIRE(strcmp(s2.m_str, s1.m_str) == 0);
        REQUIRE(s2.m_str != s1.m_str);

        s2 = s1.strip();                   // by default spaces are stripped out
        REQUIRE(s2.m_own);
        REQUIRE(s2.m_len == 17);
        REQUIRE(strcmp(s2.m_str, "StringStringHello") == 0);
        REQUIRE(s2.m_str != s1.m_str);

        s2 = s1.strip('S');                   // any character can be stripped out
        REQUIRE(s2.m_own);
        REQUIRE(s2.m_len == 23);
        REQUIRE(strcmp(s2.m_str, "   tring tring Hello   ") == 0);
        REQUIRE(s2.m_str != s1.m_str);

        s2 = s1.trim();                        // trim just calls strip with option to strip only the ends
        REQUIRE(s2.m_own);
        REQUIRE(s2.m_len == 19);
        REQUIRE(strcmp(s2.m_str, "String String Hello") == 0);
        REQUIRE(s2.m_str != s1.m_str);

        s2 = s1.trim('o');                    // nothing to trim since 'o' is not at the end
        REQUIRE(s2.m_own);
        REQUIRE(s2.m_len == 25);
        REQUIRE(strcmp(s2.m_str, s1.m_str) == 0);
        REQUIRE(s2.m_str != s1.m_str);
    }

    SECTION("String compare and comparison operators", "[compare]") {
        // test String:L:compare and other comparison operators
        String s1{"abcd"}, s2{"abcd"}, s3{"abcde"}, s4{"ABCD"}, s5{"abcD"};
        // case sensitive compare
        REQUIRE(s1.compare(s2) == 0);
        REQUIRE(s1.compare(s3) < 0);
        REQUIRE(s1.compare(s4) > 0);
        REQUIRE(s5.compare(s1) < 0);
        // case insensitive compare
        REQUIRE(s1.compare(s4, true) == 0);
        REQUIRE(s5.compare(s2, true) == 0);
        REQUIRE(s4.compare(s5, true) == 0);
        REQUIRE(s5.compare(s3, true) < 0);

        REQUIRE(s1 == s2);
        REQUIRE(s1 != s3);
        REQUIRE(s1 <  s3);
        REQUIRE(s3 >  s4);
        REQUIRE(s5 >= s4);
        REQUIRE(s5 <= s2);
    }

    SECTION("Other string API's", "[casting][c_str]") {
        // tests other string API's not tested on other sections
        String ss{}, ss1{"Hello World"};
        REQUIRE(ss.empty());
        REQUIRE(ss.size() == 0);
        REQUIRE_FALSE(ss1.empty());
        REQUIRE(ss1.size() == 11);
        REQUIRE(ss1.data() == ss1.m_cstr);

        WHEN("Getting c-style string with operator() or String::c_str") {
            // test getting the c-style string
            String s1{}, s2{"hello world"};
            // Useful when on empty strings
            REQUIRE(strlen(s1.c_str()) == 0);
            REQUIRE(strlen(s1()) == 0);
            REQUIRE(strlen(s1.c_str("nil")) == 3);
            REQUIRE(strcmp(s1.c_str("nil"), "nil") == 0);
            // on non-empty strings, the underlying buffer is returned
            auto b1 = s2.c_str(), b2 = s2.c_str("nil"), b3 = s2();
            REQUIRE(b1 == s2.m_str);
            REQUIRE(strlen(b1) == s2.m_len);
            REQUIRE(b2 == s2.m_str);
            REQUIRE(strlen(b2) == s2.m_len);
            REQUIRE(b3 == s2.m_str);
            REQUIRE(strlen(b3) == s2.m_len);
        }

        WHEN("Adding strings using operators") {
            // test += and +
            String s1{"Hello"}, s2{"World"};
            String s3 = s1 + " ";
            REQUIRE(s3.m_own);
            REQUIRE(s3.m_len == 6);
            REQUIRE(s3.compare("Hello ") == 0);
            // append s2 to s3
            s3 += s2;
            REQUIRE(s3.m_own);
            REQUIRE(s3.m_len == 11);
            REQUIRE(s3.compare("Hello World") == 0);
        }

        WHEN("Casting a string to other types using implicit cast operator") {
            // test implicit cast
            String s1{"true"}, s2{"120"}, s3{"Hello"}, s4{"0.00162"};

            // this cast work against us as they are implemented to check for empty buffer, use utils::cast
            bool to{false};
            REQUIRE((bool) s1);
            utils::cast(s1, to);
            REQUIRE(to);
            REQUIRE((bool) s2);
            utils::cast(s2, to);
            REQUIRE_FALSE(to);

            REQUIRE((int)s2 == 120);
            REQUIRE((float)s4 == 0.00162f);
            REQUIRE((double)s2 == 120);
            REQUIRE_THROWS((int)s1);
            REQUIRE_THROWS((float)s3);

            const char *out = (const char *)s3;
            REQUIRE(out == s3.m_cstr);
            std::string str = (std::string) s3;
            REQUIRE(s3.compare(str.data()) == 0);
            auto ss = (String) s3;
            REQUIRE(ss == s3);
            REQUIRE(ss.m_cstr == s3.m_cstr);
        }
    }
}
#endif