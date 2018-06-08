//
// Created by dc on 1/5/18.
//
#include "sys.hpp"

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

    void varint::in(Wire& w) {
        uint8_t sz{0};
        if (w.pull(&sz, 1)) {
            // read actual size
            uint64_t tmp{0};
            if (w.pull((uint8_t *)&tmp, sz)) {
                Ego.write<uint64_t>(tmp);
                return;
            }
        }
        SuilError::create("pulling varint failed");
    }

    void varint::out(Wire& w) const {
        uint8_t sz{Ego.length()};
        if (w.push(&sz, 1)) {
            uint64_t val = Ego.read<uint64_t>();
            if (w.push((uint8_t *)&val, sz))
                return;
        }
        SuilError::create("pushing varint failed");
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