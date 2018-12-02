//
// Created by dc on 09/11/18.
//

#include "buffer.h"
#include "base64.h"

namespace suil {

    namespace utils {

        String base64::encode(const uint8_t *data, size_t sz) {
            OBuffer ob{};
            encode(ob, data, sz);
            return String(ob);
        }

        void base64::encode(OBuffer& ob, const uint8_t *data, size_t sz) {
            static char b64table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            ob.reserve(2+((sz+2)/3*4));

            char *out = ob.data();
            char *it = out;
            while (sz >= 3) {
                // |X|X|X|X|X|X|-|-|
                *it++ = b64table[((*data & 0xFC) >> 2)];
                // |-|-|-|-|-|-|X|X|
                uint8_t h = (uint8_t) (*data++ & 0x03) << 4;
                // |-|-|-|-|-|-|X|X|_|X|X|X|X|-|-|-|-|
                *it++ = b64table[h | ((*data & 0xF0) >> 4)];
                // |-|-|-|-|X|X|X|X|
                h = (uint8_t) (*data++ & 0x0F) << 2;
                // |-|-|-|-|X|X|X|X|_|X|X|-|-|-|-|-|-|
                *it++ = b64table[h | ((*data & 0xC0) >> 6)];
                // |-|-|X|X|X|X|X|X|
                *it++ = b64table[(*data++ & 0x3F)];
                sz -= 3;
            }

            if (sz == 1) {
                // pad with ==
                // |X|X|X|X|X|X|-|-|
                *it++ = b64table[((*data & 0xFC) >> 2)];
                // |-|-|-|-|-|-|X|X|
                uint8_t h = (uint8_t) (*data++ & 0x03) << 4;
                *it++ = b64table[h];
                *it++ = '=';
                *it++ = '=';
            } else if (sz == 2) {
                // pad with =
                // |X|X|X|X|X|X|-|-|
                *it++ = b64table[((*data & 0xFC) >> 2)];
                // |-|-|-|-|-|-|X|X|
                uint8_t h = (uint8_t) (*data++ & 0x03) << 4;
                // |-|-|-|-|-|-|X|X|_|X|X|X|X|-|-|-|-|
                *it++ = b64table[h | ((*data & 0xF0) >> 4)];
                // |-|-|-|-|X|X|X|X|
                h = (uint8_t) (*data++ & 0x0F) << 2;
                *it++ = b64table[h];
                *it++ = '=';
            }

            *it = '\0';
            // own the memory
            ob.bseek(it-out);
        }

        String base64::decode(const uint8_t *in, size_t size) {
            OBuffer b((uint32_t) (size/4)*3);
            decode(b, in, size);
            return String{b};
        }

        void base64::decode(OBuffer& ob, const uint8_t *in, size_t size) {
            ob.reserve((uint32_t) (size/4)*3);
            static const unsigned char ASCII_LOOKUP[256] =
            {
                /* ASCII table */
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
                52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
                64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
                15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
                64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
                41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
                64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
            };
            size_t sz = size, pos{0};
            const uint8_t *it = in;
            char *data = ob.data();

            while (sz > 4) {
                if (ASCII_LOOKUP[it[0]] == 64 ||
                    ASCII_LOOKUP[it[1]] == 64 ||
                    ASCII_LOOKUP[it[2]] == 64 ||
                    ASCII_LOOKUP[it[3]] == 64)
                {
                    // invalid base64 character
                    throw Exception::invalidArguments("utils::base64::decode - invalid base64 encoded string passed");
                }

                data[pos++] = ((uint8_t)(ASCII_LOOKUP[it[0]] << 2 | ASCII_LOOKUP[it[1]] >> 4));
                data[pos++] = ((uint8_t)(ASCII_LOOKUP[it[1]] << 4 | ASCII_LOOKUP[it[2]] >> 2));
                data[pos++] = ((uint8_t)(ASCII_LOOKUP[it[2]] << 6 | ASCII_LOOKUP[it[3]]));
                sz -= 4;
                it += 4;
            }
            int i = 0;
            while ((it[i] != '=') && (ASCII_LOOKUP[it[i]] != 64) && (i++ < 4));
            if ((sz-i) && (it[i] != '=')) {
                // invalid base64 character
                throw Exception::invalidArguments("utils::base64::decode - invalid base64 encoded string passed");
            }
            sz -= 4-i;

            if (sz > 1) {
                data[pos++] = ((uint8_t)(ASCII_LOOKUP[it[0]] << 2 | ASCII_LOOKUP[it[1]] >> 4));
            }
            if (sz > 2) {
                data[pos++] = ((uint8_t)(ASCII_LOOKUP[it[1]] << 4 | ASCII_LOOKUP[it[2]] >> 2));
            }
            if (sz > 3) {
                data[pos++] = ((uint8_t)(ASCII_LOOKUP[it[2]] << 6 | ASCII_LOOKUP[it[3]]));
            }

            ob.seek(pos);
        }
    }
}

#ifdef unit_test
#include <catch/catch.hpp>

using namespace suil;

TEST_CASE("base64 encoding", "[common][utils][base64]")
{
    String raw{"Hello World!"}; // SGVsbG8gV29ybGQh
    String b64{};

    SECTION("base encode decode") {
        b64 = utils::base64::encode(raw);
        CHECK(b64.compare("SGVsbG8gV29ybGQh") == 0);
        REQUIRE(raw == utils::base64::decode(b64));

        raw = "12345678";  // MTIzNDU2Nzg=
        b64 = utils::base64::encode(raw);
        CHECK(b64.compare("MTIzNDU2Nzg=") == 0);
        REQUIRE(raw == utils::base64::decode(b64));
    }

    SECTION("different character lengths") {
        raw = "1234567";  // MTIzNDU2Nw==
        b64 = utils::base64::encode(raw);
        CHECK(b64.compare("MTIzNDU2Nw==") == 0);
        REQUIRE(raw == utils::base64::decode(b64));

        raw = "123456";  // MTIzNDU2
        b64 = utils::base64::encode(raw);
        CHECK(b64.compare("MTIzNDU2") == 0);
        REQUIRE(raw == utils::base64::decode(b64));

        raw = "12345";  // MTIzNDU=
        b64 = utils::base64::encode(raw);
        CHECK(b64.compare("MTIzNDU=") == 0);
        REQUIRE(raw == utils::base64::decode(b64));
    }

    SECTION("data with spaces") {
        raw = "Sentence with spaces";  // U2VudGVuY2Ugd2l0aCBzcGFjZXM=
        b64 = utils::base64::encode(raw);
        CHECK(b64.compare("U2VudGVuY2Ugd2l0aCBzcGFjZXM=") == 0);
        REQUIRE(raw == utils::base64::decode(b64));
    }

    SECTION("punctuation in data") {
        // U2VudGVuY2UsIHdpdGghJCYjQCpefmAvPyIie1soKV19fFwrPS1fLC47Og==
        raw = R"(Sentence, with!$&#@*^~`/?""{[()]}|\+=-_,.;:)";
        b64 = utils::base64::encode(raw);
        CHECK(b64.compare("U2VudGVuY2UsIHdpdGghJCYjQCpefmAvPyIie1soKV19fFwrPS1fLC47Og==") == 0);
        REQUIRE(raw == utils::base64::decode(b64));
    }

    SECTION("binary data") {
        struct binary { uint64_t a; uint8_t  b[4]; };
        binary b{0xFFEEDDCCBBAA0011, {0x01, 0x02, 0x03, 0x04}};
        String ob((char *)&b, sizeof(binary), false);
        b64 = utils::base64::encode((const uint8_t *) &b, sizeof(binary));
        String db(utils::base64::decode(b64));
        REQUIRE(ob == db);
    }
}


#endif