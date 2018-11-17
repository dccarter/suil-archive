//
// Created by dc on 07/06/18.
//

#include <snappy/snappy.h>

#include "logging.h"
#include "compression.h"

namespace suil {

    namespace utils {

        size_t compress(const uint8_t input[], size_t isz, uint8_t output[], size_t osz) {
            snappy::RawCompress((const char*)input, isz, (char *)output, &osz);
            return osz;
        }

        suil::Data compress(const uint8_t input[], size_t isz) {
            size_t osz = snappy::MaxCompressedLength(isz);
            auto buffer = (uint8_t *) malloc(osz);
            if (!buffer) {
                // allocating buffer memory failed
                throw Exception::create("allocating compression sink memory failed: ", errno_s);
            }

            osz = compress(input, isz, buffer, osz);
            if (osz) {
                // compressed successfully
                return Data{buffer, osz, true};
            }
            return Data{};
        }

        bool uncompress(const uint8_t input[], size_t isz, uint8_t output[], size_t osz) {
            size_t needs{0};
            if (!snappy::GetUncompressedLength((const char*)input, isz, &needs)) {
                // error couldn't get uncompressed length
                serror("retrieving compressed buffer uncompressed length failed");
                return false;
            }

            if (needs > osz) {
                // error output  buffer length is too small
                throw Exception::create("output buffer length is too small, needs ", needs, " bytes");
            }
            return snappy::RawUncompress((const char*) input, isz, (char *) output);
        }

        Data uncompress(const uint8_t input[], size_t isz) {
            size_t needs{0};
            if (!snappy::GetUncompressedLength((const char*)input, isz, &needs)) {
                // error couldn't get uncompressed length
                serror("retrieving compressed buffer uncompressed length failed");
                return Data{};
            }

            auto buffer = (uint8_t *) malloc(needs);
            if (!buffer) {
                // allocating buffer memory failed
                throw Exception::create("allocating compression sink memory failed: ", errno_s);
            }

            if (!snappy::RawUncompress((const char*)input, isz, (char *)buffer)) {
                // compression failed
                serror("compressing raw buffer failed");
                free(buffer);
                return Data{};
            }
            return Data{buffer, needs, true};
        }
    }
}

#ifdef unit_test

#include <catch/catch.hpp>

using namespace suil;

TEST_CASE("suil::utils::compression", "[utils][compression]")
{
    SECTION("Compress/Uncompress", "[utils][compress][uncompress]") {
        // Just a wrapper on top of snappy which has a good unit test coverage
        String s1{"Hello World"};
        Data compressed   = utils::compress(s1);
        REQUIRE(compressed.size());
        auto uncompressed = utils::uncompress(compressed);
        REQUIRE(uncompressed.size());
        String s2{(const char*)uncompressed.data(), uncompressed.size(), false};
        REQUIRE(s1 == s2);

        OBuffer bb{256};
        bb << "Hello World" << (float) 10e-3 << (uint8_t) 0xaf;
        uncompressed = bb.cdata();
        REQUIRE_NOTHROW((compressed = utils::compress(uncompressed)));
        REQUIRE(compressed.size());
        auto uncompressed2 = utils::uncompress(compressed);
        REQUIRE(uncompressed2.size());
        REQUIRE(uncompressed == uncompressed2);

        String lstr = R"(
"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut
labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco
laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in
voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut
labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco
laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in
voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut
labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco
laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in
voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut
labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco
laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in
voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut
labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco
laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in
voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat
non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.)";
        REQUIRE_NOTHROW((compressed = utils::compress(lstr)));
        REQUIRE(compressed.size());
        REQUIRE_NOTHROW((uncompressed = utils::uncompress(compressed)));
        String lstr2{(const char*)uncompressed.data(), uncompressed.size(), false};
        REQUIRE(lstr == lstr2);
    };
}
#endif