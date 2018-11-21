//
// Created by dc on 1/5/18.
//
#include "varint.h"

namespace suil {

    VarInt::VarInt(uint64_t v)
    {
        write(v);
    }

    VarInt::VarInt()
            : VarInt(0)
    {}

    uint8_t *VarInt::raw() {
        return mData;
    }

    uint8_t VarInt::length() const {
        uint8_t sz{0};
        auto tmp = Ego.read<uint64_t>();
        while (tmp > 0) {
            tmp >>= 8;
            sz++;
        }
        return sz;
    }
}

#ifdef unit_test
#include <catch/catch.hpp>

using namespace suil;

TEST_CASE("suil::VarInt", "[suil][VarInt]")
{
    SECTION("when initializing VarInt") {
        uint64_t num = 0xaa;
        VarInt   vnum(num);
        REQUIRE(vnum.length() == 1);
        REQUIRE(vnum.read<uint64_t>() == num);

        for (int i=1; i < (sizeof(uint64_t)/8)-1; i++) {
            // assignment
            uint64_t current = (num << i*8);
            vnum = current;
            REQUIRE(vnum.length() == (i+1));
            REQUIRE(vnum.read<uint64_t>() == current);
        }
    }

    SECTION("operations on varint") {
        // Some basic operations
        VarInt v1(10), v2(10), v3(100);
        REQUIRE(v1 == v2);
        REQUIRE_FALSE(v2 == v3);
        REQUIRE(v1.read<uint64_t>() == v1.read<uint8_t>());
        REQUIRE(v1.read<uint16_t>() == v2.read<uint8_t>());
    }
}
#endif