//
// Created by dc on 1/5/18.
//
#include "varint.h"

namespace suil {

    varint::varint(uint64_t v)
        : Blob()
    {
        write(v);
    }

    varint::varint()
            : varint(0)
    {}

    uint8_t *varint::raw() {
        return (uint8_t *) Ego.begin();
    }

    uint8_t varint::length() const {
        uint8_t sz{0};
        auto tmp = Ego.read<uint64_t>();
        while (tmp > 0) {
            tmp >>= 8;
            sz++;
        }
        return sz;
    }
}

#ifdef SUIL_TESTING
#include <catch/catch.hpp>

using namespace suil;

TEST_CASE("suil::varint", "[suil][varint]")
{
    SECTION("when initializing varint") {
        uint64_t num = 0xaa;
        varint   vnum(num);
        REQUIRE(vnum.length() == 1);
        REQUIRE(vnum.read<uint64_t>() == num);

        for (int i=1; i < (sizeof(uint64_t)/8)-1; i++) {
            // assignment
            uint64_t current = (num << i*8);
            vnum = current;
            REQUIRE(vnum.length() == (i+1));
        }
    }

    SECTION("operations on varint") {
        // Some basic operations
        varint v1(10), v2(10), v3(100);
        REQUIRE(v1 == v2);
        REQUIRE_FALSE(v2 == v3);
        REQUIRE(v1.read<uint64_t>() == v1.read<uint8_t>());
        REQUIRE(v1.read<uint16_t>() == v2.read<uint8_t>());
    }
}
#endif