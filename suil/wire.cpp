//
// Created by dc on 14/12/17.
//

#include "wire.hpp"

namespace suil {

    void zbuffer::in(Wire &w) {
        varint sz(0);
        w >> sz;
        uint64_t tmp{sz.read<uint64_t>()};
        Ego.reserve(tmp);
        if (w.pull(&Ego.data_[Ego.offset_], tmp)) {
            Ego.offset_ += tmp;
            return;
        }

        throw SuilError::create("pulling buffer failed");
    }

    void zbuffer::out(Wire &w) const {
        zcstring tmp{(const char *)Ego.data_, Ego.size(), false};
        w << tmp;
    }

    void breadboard::out(Wire &w) const {
        if (&w == this) {
            throw SuilError::create("infinite loop detected, serializing breadboard to self");
        }
        zcstring tmp{(const char *) &Ego.sink[Ego.H], Ego.size(), false};
        w << tmp;
    }

    void heapboard::in(Wire &w) {
        varint sz(0);
        w >> sz;
        uint64_t tmp{sz.read<uint64_t>()};
        clear();
        Ego.sink = Ego.data = w.rd();
        M    = tmp;
        own  = false;
        H    = 0;
        T    = tmp;
        // adjust w's size
        if (w.move(tmp))
            return;

        throw SuilError::create("pulling buffer failed");
    }

    void Data::in(Wire &w) {
        varint sz{0};
        w >> sz;
        uint64_t tmp{sz.read<uint64_t>()};
        clear();
        Ego._data = w.rd();
        Ego._own  = 0;
        Ego._size = (uint32_t) tmp;
        // adjust serializer size
        if (w.move(tmp))
            return;

        throw SuilError::create("pulling buffer failed");
    }

    void Data::out(Wire &w) const {
        zcstring tmp{(const char *) Ego._data, Ego.size(), false};
        w << tmp;
    }
}

#ifdef SUIL_TESTING
#include <catch/catch.hpp>
#include "tests/test_symbols.h"

using namespace suil;
using namespace test;

typedef decltype(iod::D(topt(a, int()), topt(b, int()))) A;

TEST_CASE("suil::Wire", "[suil][Wire]")
{
    SECTION("Using a breadboard", "[Wire][breadboard]")
    {
        uint8_t buffer[16] = {0};
        breadboard bb(buffer, sizeof(buffer));
        REQUIRE(bb.size() == 0); // nothing has been serialized to the buffer

        WHEN("serializing data to bread board") {
            // add different types of data and ensure it's deserialized correctly
            bb << (uint8_t)  8;
            REQUIRE(bb.size() == 1);
            bb << (uint16_t) 8;
            REQUIRE(bb.size() == 3);
            varint v(0x99AA);
            bb << v;
            REQUIRE(bb.size() == 6);

            bb.reset();
            bb << "Hello World"; // |ssz(1)|sz(1)|str(13)|
            REQUIRE(bb.size() == 13);

            bb.reset();
            bb << "Hello World"_zc;
            REQUIRE(bb.size() == 13);

            bb.reset();
            bb << std::string("Hello World");
            REQUIRE(bb.size() == 13);

            bb.reset();
            bb << (float) 1.0009;
            REQUIRE(bb.size() == sizeof(float));

            bb.reset();
            bb << (double) 1.0009;
            REQUIRE(bb.size() == sizeof(double));
            bb.reset();

            A a;
            a.a = 2;
            a.b = 4;
            bb << a;
            REQUIRE(bb.size() == 8);
            // self serialization not allowed
            REQUIRE_THROWS((bb<<bb));
            uint8_t  tmp[8];
            breadboard bb1(tmp, sizeof(tmp));
            bb1 << (uint16_t) 45;
            REQUIRE_NOTHROW((bb << bb1));
            REQUIRE(bb.size() == 12);
            // serialize to full breadboard not allowed
            REQUIRE_THROWS((bb << a));
        }

        WHEN("deserialize data from heapboard") {
            uint8_t a = 8, aa = 0;
            bb << a;
            bb >> aa;
            REQUIRE(a == aa);
            bb.reset();

            int16_t b = 100, ba = 0;
            bb << b;
            bb >> ba;
            REQUIRE(b == ba);
            bb.reset();

            zcstring c = "Hello world", cc{nullptr};
            bb << c;
            bb >> cc;
            REQUIRE(c == cc);
            bb.reset();

            std::string d = "Hello world", dd{""};
            bb << d;
            bb >> dd;
            REQUIRE(c == cc);
            bb.reset();

            float e = 100.1112, ee{0};
            bb << e;
            bb >> ee;
            REQUIRE(e == ee);
            bb.reset();

            double f = 1.02e-9, ff{0};
            bb << f;
            bb >> ff;
            REQUIRE(f == ff);
            bb.reset();

            A g, gg;
            g.a = 1002;
            g.b = 1003;
            bb << g;
            bb >> gg;
            REQUIRE((g.a == gg.a && g.b == gg.b));
            // empty stack board should not throw an error
            REQUIRE(bb.size() == 0);
            REQUIRE_NOTHROW((bb >> gg));
            // Stream lining
            uint8_t buffer2[512];
            breadboard bb2(buffer2, sizeof(buffer2));
            REQUIRE_NOTHROW((bb2 << a << b << c << d << e << f << g));
            REQUIRE_NOTHROW((bb2 >> aa >> ba >> cc >> dd >> ee >> ff >> gg));
            REQUIRE(a == aa);
            REQUIRE(b == ba);
            REQUIRE(c == cc);
            REQUIRE(d == dd);
            REQUIRE(e == ee);
            REQUIRE(f == ff);
            REQUIRE((g.a == gg.a && g.b == gg.b));
        }
    }
}
#endif