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

    SECTION("string to number invalid arguments", "[strtonum]") {
        int64_t num;
        zcstring str("167");
        // minimum can never be more than maximum
        REQUIRE_THROWS(utils::strtonum(str, 10, 100, 99));
        // number out of range
        REQUIRE_THROWS(utils::strtonum(str, 10, 99, 100));
        REQUIRE_THROWS(utils::strtonum(str, 10, 168, 200));
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

    SECTION("zcstring to type using explicit cast operator") {
        zcstring from{"1556.8"};
        int64_t i{0};
        float f{0.0};
        i = (int) from;
        f = (float) from;
        CHECK(i == 1556);
        CHECK(f == Approx(1556.8));
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
        zcstring db(base64::decode(b64));
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
            zcstring out = utils::md5(data[i][0]);
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
        zcstring hashed = utils::shaHMAC256(secret, sentence);
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

TEST_CASE("utils::filesystem", "[utils][fs]") {
    // creating a single directory
    REQUIRE_NOTHROW(utils::fs::mkdir("test"));

    SECTION("basic file operations" "[fs][file_ops]") {
        // directory should have been created
        REQUIRE(utils::fs::exists("test"));
        // shouldn't files that don't exist
        REQUIRE_FALSE(utils::fs::exists("test1"));
        // Create another directory
        REQUIRE_NOTHROW(utils::fs::touch("test/file.txt"));
        // File should have been created
        REQUIRE(utils::fs::exists("test/file.txt"));
        // Size should be 0
        REQUIRE(utils::fs::size("test/file.txt") == 0);
        // Delete file, should not exist there after
        REQUIRE_NOTHROW(utils::fs::remove("test/file.txt"));
        // shouldn't files that don't exist
        REQUIRE_FALSE(utils::fs::exists("test/file.txt"));
    }

    SECTION("basic directory operations", "[fs][dir_ops]") {
        // Using created test directory
        REQUIRE(utils::fs::isdir("test"));
        // Create directory within test
        REQUIRE_NOTHROW(utils::fs::mkdir("test/test1"));
        // Should exist
        REQUIRE(utils::fs::exists("test/test1"));
        // Can be deleted if empty
        REQUIRE_NOTHROW(utils::fs::remove("test/test1"));
        // Should not exist
        REQUIRE_FALSE(utils::fs::exists("test/test1"));
        // Directory with parents requires recursive flag
        REQUIRE_THROWS(utils::fs::mkdir("test/test1/child1"));
        REQUIRE_NOTHROW(utils::fs::mkdir("test/test1/child1", true));
        // Should exist
        REQUIRE(utils::fs::exists("test/test1/child1"));
        // Deleting non-empty directory also requires the recursive flag
        REQUIRE_THROWS(utils::fs::remove("test/test1"));
        REQUIRE_NOTHROW(utils::fs::remove("test/test1", true));
        // Should not exist
        REQUIRE_FALSE(utils::fs::exists("test/test1"));
        // Multiple directories can be created at once
        REQUIRE_NOTHROW(utils::fs::mkdir("test", {"test1", "test2"}));
        // Test should have 2 directories
        REQUIRE(utils::fs::ls("test").size() == 2);
        // Remove all directories
        REQUIRE_NOTHROW(utils::fs::remove("test", {"test1", "test2"}));
        REQUIRE(utils::fs::ls("test").empty());
        // Recursive again
        REQUIRE_NOTHROW(utils::fs::mkdir("test", {"test1/child1/gchild1",
                                                  "test1/child1/gchild2"
                                                  "test1/child2/gchild1/ggchild/ggchild1",}, true));
        // list recursively
        REQUIRE(utils::fs::ls("test", true).size()==8);
        // Remove recursively
        REQUIRE_NOTHROW(utils::fs::remove("test", true, true));

    }

    SECTION("Basic file IO", "[fs][file_io]") {
        const char *fname = "test/file.txt";
        // Reading non-existent file
        zcstring data;
        REQUIRE_THROWS((data = utils::fs::readall(fname)));
        // Create empty file
        utils::fs::touch(fname, 0777);
        data = utils::fs::readall(fname);
        REQUIRE(data.empty());
        // Write data to file
        data = "The quick brown fox";
        utils::fs::append(fname, data);
        REQUIRE(utils::fs::size("test/file.txt") == data.len);
        zcstring out = utils::fs::readall(fname);
        REQUIRE(data == out);
        // Append to file
        data = utils::catstr(data, "\n jumped over the lazy dog");
        utils::fs::append(fname, "\n jumped over the lazy dog");
        REQUIRE(utils::fs::size(fname) == data.len);
        out = utils::fs::readall(fname);
        REQUIRE(data == out);
        // Clear the file
        utils::fs::clear(fname);
        REQUIRE(utils::fs::size(fname) == 0 );
    }

    // Cleanup
    REQUIRE_NOTHROW(utils::fs::remove("test", true));
}

TEST_CASE("utils::zcstring", "[utils][zcstring]")
{
    SECTION("initializing a zcstring") {
        zcstring str;
        REQUIRE(str.empty());
        // initialize from other types
        zcstring str1("Hello World");
        REQUIRE_FALSE(str1.empty());
        REQUIRE(str1.own == 0);
        REQUIRE(strcmp(str1.str, "Hello World") == 0);

        std::string stdstr("Hello World");

        zcstring str2(stdstr);
        REQUIRE_FALSE(str2.empty());
        REQUIRE(str2.own == 0);
        REQUIRE(strcmp(str2.str, "Hello World") == 0);

        zcstring str3(stdstr, true);
        REQUIRE_FALSE(str3.empty());
        REQUIRE(str3.own == 1);
        REQUIRE(strcmp(str3.str, "Hello World") == 0);
        REQUIRE(str3.cstr != stdstr.data());

        zcstring str4(utils::strdup("Hello World"), 12, true);
        REQUIRE_FALSE(str4.empty());
        REQUIRE(str4.own == 1);
        REQUIRE(strcmp(str4.str, "Hello World") == 0);

        zbuffer b(15);
        b << "Hello World";
        zcstring str5(utils::strdup("Hello World"), 12, true);
        REQUIRE_FALSE(str5.empty());
        REQUIRE(str5.own == 1);
        REQUIRE(strcmp(str5.str, "Hello World") == 0);
    }
}