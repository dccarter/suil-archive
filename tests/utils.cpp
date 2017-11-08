//
// Created by dc on 10/13/17.
//
#include <suil/sys.hpp>
#include <catch/catch.hpp>

#include "test_symbols.h"

using namespace suil;

TEST_CASE("utils::cast", "[utils][cast]")
{
    SECTION("zcstring to number") {
        int64_t num;
        double   real;

        zcstring str("100");
        utils::cast(str, num);
        REQUIRE(num == 100);
        utils::cast(str, real);
        REQUIRE(real == 100.0);

        str = "-5689.1299999";
        utils::cast(str, num);
        REQUIRE(num == -5689);
        utils::cast(str, real);
        REQUIRE(real == Approx(-5689.1299999));
    }

    SECTION("invalid zcstring to number") {
        uint64_t num;
        double   real;

        // has letters
        zcstring str("100za3");
        REQUIRE_THROWS(utils::cast(str, num));
        REQUIRE_THROWS(utils::cast(str, real));

        // starts with letters
        str = "za3100";
        REQUIRE_THROWS(utils::cast(str, num));
        REQUIRE_THROWS(utils::cast(str, real));
    }

    SECTION("zcstring to std::string") {
        zcstring from{"Hello World"};
        std::string to;
        utils::cast(from, to);
        CHECK("Hello World" == to);
    }

    SECTION("zcstring to zcstring") {
        zcstring from{"Hello World"};
        zcstring to{};
        utils::cast(from, to);
        // same as input string
        CHECK(to.compare("Hello World") == 0);
        // exact reference to from string
        REQUIRE(to == from);
    }
}

TEST_CASE("utils::tozcstr", "[utils][tozcstr]")
{
    SECTION("number to zcstring") {
        int64_t num{100};
        double  real{100.05};

        // Converting non  float
        zcstring str = utils::tozcstr(num);
        CHECK(str.compare("100") == 0);
        // Converting float
        str = utils::tozcstr(real);
        CHECK(str.compare("100.05") == 0);

        // Converting from negative numbers
        num = -1005;
        real = -1.000009e3;
        str = utils::tozcstr(num);
        CHECK(str.compare("-1005") == 0);
        str = utils::tozcstr(real);
        CHECK(str.compare("-1000.009000") == 0);
    }

    SECTION("other strings to zcstring") {
        std::string s{"hello"};
        const char *cs{"hello"};
        zcstring zc{"hello"};
        zcstring tmp{};

        tmp = utils::tozcstr(s);
        CHECK(tmp.compare("hello") == 0);
        tmp = utils::tozcstr(cs);
        CHECK(tmp.compare("hello") == 0);
        tmp = utils::tozcstr(zc);
        CHECK(tmp.compare("hello") == 0);
        REQUIRE(tmp == zc);
    }
}

TEST_CASE("utils::base64", "[utils][base64]")
{
    zcstring raw{"Hello World!"}; // SGVsbG8gV29ybGQh
    zcstring b64{};

    SECTION("base encode decode") {
        b64 = base64::encode(raw);
        CHECK(b64.compare("SGVsbG8gV29ybGQh") == 0);
        REQUIRE(raw == base64::decode(b64));

        raw = "12345678";  // MTIzNDU2Nzg=
        b64 = base64::encode(raw);
        CHECK(b64.compare("MTIzNDU2Nzg=") == 0);
        REQUIRE(raw == base64::decode(b64));
    }

    SECTION("different character lengths") {
        raw = "1234567";  // MTIzNDU2Nw==
        b64 = base64::encode(raw);
        CHECK(b64.compare("MTIzNDU2Nw==") == 0);
        REQUIRE(raw == base64::decode(b64));

        raw = "123456";  // MTIzNDU2
        b64 = base64::encode(raw);
        CHECK(b64.compare("MTIzNDU2") == 0);
        REQUIRE(raw == base64::decode(b64));

        raw = "12345";  // MTIzNDU=
        b64 = base64::encode(raw);
        CHECK(b64.compare("MTIzNDU=") == 0);
        REQUIRE(raw == base64::decode(b64));
    }

    SECTION("data with spaces") {
        raw = "Sentence with spaces";  // U2VudGVuY2Ugd2l0aCBzcGFjZXM=
        b64 = base64::encode(raw);
        CHECK(b64.compare("U2VudGVuY2Ugd2l0aCBzcGFjZXM=") == 0);
        REQUIRE(raw == base64::decode(b64));
    }

    SECTION("punctuation in data") {
        // U2VudGVuY2UsIHdpdGghJCYjQCpefmAvPyIie1soKV19fFwrPS1fLC47Og==
        raw = "Sentence, with!$&#@*^~`/?\"\"{[()]}|\\+=-_,.;:";
        b64 = base64::encode(raw);
        CHECK(b64.compare("U2VudGVuY2UsIHdpdGghJCYjQCpefmAvPyIie1soKV19fFwrPS1fLC47Og==") == 0);
        REQUIRE(raw == base64::decode(b64));
    }

    SECTION("binary data") {
        struct binary { uint64_t a; uint8_t  b[4]; };
        binary b{0xFFEEDDCCBBAA0011, {0x01, 0x02, 0x03, 0x04}};
        zcstring ob((char *)&b, sizeof(binary), false);
        b64 = base64::encode((const uint8_t *) &b, sizeof(binary));
        buffer_t tmp(base64::decode(b64));
        zcstring db(tmp);
        REQUIRE(ob == db);
    }
}

TEST_CASE("utils::url", "[!hide][utils][url]")
{
    SECTION("encode/decode url") {
        // TODO
    }
}

TEST_CASE("utils::options", "[utils][options][config][apply]")
{
    struct options { bool a; int b; std::string c; };
    options opts{};
    /* applying empty options should not throw an exception */
    REQUIRE_NOTHROW(utils::apply_config(opts));

    utils::apply_config(opts,
                        (test::s::_a = true),
                        (test::s::_b = 4),
                        (test::s::_c = "Hello World"));
    REQUIRE(opts.a);
    REQUIRE(opts.b == 4);
    REQUIRE(opts.c == "Hello World");
}

TEST_CASE("utils::hashing", "[utils][hashing]")
{
    SECTION("MD5 hashing", "[MD5]") {
        zcstring data[][2] = {
            {"The quick brown fox",  "a2004f37730b9445670a738fa0fc9ee5"},
            {"Jumped over the fire", "b6283454ffe0eb2b97c8d8d1b94062dc"},
            {"Simple world",         "1b5b2eb127a6f47cad4ddcde1b5a5205"},
            {"Suilman of SuilCity",  "f6ce722986cb80b57f153faf0ccf51f1"}
        };

        for (int i=0; i < 4; i++) {
            zcstring out = utils::md5Hash(data[i][0]);
            REQUIRE(out.compare(data[i][1]) == 0);
        }
    }

    SECTION("HMAC_Sha256 hashing", "[HMAC_Sha256]") {
        // 8559f3e178983e4e83b20b5688f335c133c88015c51f3a9f7fcfde5cd4f613dc
        zcstring sentence =
                "The quick brown fox jumped over the lazy dog.\n"
                "The lazy dog just lied there thinking 'the world is \n"
                "circle, i'll get that quick brown fox'";
        zcstring secret = "awfulSecret12345";

        // compute the hash
        zcstring hashed = utils::HMAC_Sha256(secret, sentence);
        REQUIRE(hashed.compare("8559f3e178983e4e83b20b5688f335c133c88015c51f3a9f7fcfde5cd4f613dc") == 0);
    }
}

TEST_CASE("utils::string_ops", "[utils][string_ops]")
{
    SECTION("split string", "[splitstr]") {
        zcstring from(zcstring("   Spilt With Spaces!!!!  ").dup());
        auto split = utils::strsplit(from, " ");
        REQUIRE(split.size() == 3);

        from = zcstring("Split,With,CSV    ,Other").dup();
        split = utils::strsplit(from, ",");
        REQUIRE(split.size() == 4);

        from = zcstring("Split||With||Two||Bars|Not|One||Bar").dup();
        split = utils::strsplit(from, "||");
        REQUIRE(split.size() == 7);

        from = zcstring("Spliting, with,_multiple=delims").dup();
        split = utils::strsplit(from, ", _=");
        REQUIRE(split.size() == 4);
    }

    SECTION("strip string", "[strstrip][strtrim]") {
        zcstring from("    The string to trim   ");
        zcstring to = utils::strtrim(from);
        REQUIRE(to.compare("The string to trim") == 0);

        from = "&&&The&string&&to&&Strip of All &";
        to = utils::strstrip(from, '&');
        REQUIRE(to.compare("ThestringtoStrip of All ") == 0);
    }

    SECTION("concat strings", "[catstr]") {
        zcstring str(utils::catstr("Hello", ", ", "World", '!', 50, '!'));
        REQUIRE(str.compare("Hello, World!50!") == 0);
    }

    SECTION("match any", "[strmatchany]") {
        // should fail if no of the strings matches first one
        REQUIRE(!utils::strmatchany("One", "Two", "Three", "One!!!"));
        // Will pass if any of the strings matches
        REQUIRE(utils::strmatchany("One", "Two", "Three", "One"));
    }
}