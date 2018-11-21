//
// Created by dc on 20/11/18.
//

#include "wire.h"
#include "utils.h"

namespace suil {

    void Breadboard::toHexStr(suil::OBuffer &ob) const {
        auto tmp = Ego.raw();
        ob.reserve((tmp.size()*2)+2);
        ssize_t sz = utils::hexstr(tmp.cdata(), tmp.size(), (char *)&ob[ob.size()], ob.capacity());
        ob.seek(sz);
    }

    bool Breadboard::fromHexStr(suil::String &str) {
        if ((str.size() >> 1) > (M-T))
            return false;
        utils::bytes(str, &sink[T], (M-T));
        T += (str.size() >> 1);
        return true;
    }

    Heapboard::Heapboard(size_t size)
        : data((uint8_t *)malloc(size)),
          Breadboard(data, size),
          own{true}
    {
        Ego.sink = Ego.data;
        Ego.M    = size;
    }

    Heapboard::Heapboard(const uint8_t *buf, size_t size)
        : _cdata(buf),
          Breadboard(data, size)
    {
        // push some data into it;
        Ego.sink = data;
        Ego.M    = size;
        Ego.T    = size;
        Ego.H    = 0;
    }

    Heapboard::Heapboard(suil::Heapboard &&hb)
            : Heapboard(hb.data, hb.M)
    {
        Ego.own = hb.own;
        Ego.H = hb.H;
        Ego.T = hb.T;
        hb.own  = false;
        hb.data = hb.sink = nullptr;
        hb.M = hb.H = hb.T = 0;
    }

    Heapboard& Heapboard::operator=(suil::Heapboard &&hb) {
        Ego.sink = Ego.data = hb.data;
        Ego.own = hb.own;
        Ego.H = hb.H;
        Ego.T = hb.T;
        hb.own  = false;
        hb.data = hb.sink = nullptr;
        hb.M = hb.H = hb.T = 0;
        return  Ego;
    }

    void Heapboard::copyfrom(const uint8_t *data, size_t sz) {
        if (data && sz>0) {
            clear();
            Ego.data = (uint8_t *)malloc(sz);
            Ego.sink = Ego.data;
            Ego.M    = sz;
            Ego.own  = true;
            Ego.H    = 0;
            Ego.T    = sz;
            memcpy(Ego.data, data, sz);
        }
    }

    bool Heapboard::seal() {
        size_t tmp{size()};
        if (Ego.own && tmp) {
            /* 65 K maximum size
             * if (size()) < 75% */
            if (tmp < (M-(M>>1))) {
                void* td = malloc(tmp);
                size_t tm{M}, tt{T}, th{H};
                memcpy(td, &Ego.data[H], tmp);
                clear();
                Ego.sink = Ego.data = (uint8_t *)td;
                Ego.own = true;
                Ego.M   = tm;
                Ego.T   = tt;
                Ego.H   = th;

                return true;
            }
        }
        return false;
    }

    Data Heapboard::release() {
        if (Ego.size()) {
            // seal buffer
            Ego.seal();
            Data tmp{Ego.data, Ego.size(), Ego.own};
            Ego.own = false;
            Ego.clear();
            return std::move(tmp);
        }

        return Data{};
    }

    Wire& operator>>(suil::Wire &w, suil::String &s)  {
        Data tmp{};
        w >> tmp;
        if (!tmp.empty()) {
            // string is not empty
            s = String((const char*)tmp.data(), tmp.size(), false).dup();
        }
        return w;
    }

    Wire& operator<<(suil::Wire &w, const suil::String &s) {
        return (w << Data(s.m_cstr, s.size(), false));
    }
}

#ifdef unit_test
#include <catch/catch.hpp>
#include "tests/test_symbols.h"

using namespace suil;

using namespace test;

typedef decltype(iod::D(topt(a, int()), topt(b, int()))) A;

TEST_CASE("suil::Wire", "[suil][Wire]")
{
    SECTION("Using a Breadboard", "[Wire][Breadboard]")
    {
        uint8_t buffer[16] = {0};
        Breadboard bb(buffer, sizeof(buffer));
        REQUIRE(bb.size() == 0); // nothing has been serialized to the buffer

        WHEN("serializing data to bread board") {
            // add different types of data and ensure it's deserialized correctly
            bb << (uint8_t)  8;
            REQUIRE(bb.size() == 1);
            bb << (uint16_t) 8;
            REQUIRE(bb.size() == 3);
            VarInt v(0x99AA);
            bb << v;
            REQUIRE(bb.size() == 6);

            bb.reset();
            bb << "Hello World"; // |ssz(1)|sz(1)|str(11)|
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
            Breadboard bb1(tmp, sizeof(tmp));
            bb1 << (uint16_t) 45;
            REQUIRE_NOTHROW((bb << bb1));
            REQUIRE(bb.size() == 12);
            // serialize to full Breadboard not allowed
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

            uint16_t vaa{0xAF09}, vab(0);
            VarInt via(vaa), vib(0);
            bb << via;
            bb >> vib;
            REQUIRE(via.length() == vib.length());
            REQUIRE(memcmp(via.mData, vib.mData, sizeof(vib.mData)) == 0);
            vab = vib;
            REQUIRE(vab == vaa);
            bb.reset();

            String c = "Hello world", cc{nullptr};
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
            // empty stack board should throw an error
            REQUIRE(bb.size() == 0);
            REQUIRE_THROWS((bb >> gg));
            // Stream lining
            uint8_t buffer2[512];
            Breadboard bb2(buffer2, sizeof(buffer2));
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

#endif // unit_test