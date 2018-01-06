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
            tmp >>= 7;
            sz++;
        }
        return sz;
    }

    void varint::in(wire& w) {
        uint8_t sz{0};
        if (w.pull(&sz, 1)) {
            // read actual size
            uint64_t tmp{0};
            if (w.pull((uint8_t *)&tmp, sz)) {
                Ego.write<uint64_t>(tmp);
                return;
            }
        }
        suil_error::create("pulling varint failed");
    }

    void varint::out(wire& w) const {
        uint8_t sz{Ego.length()};
        if (w.push(&sz, 1)) {
            uint64_t val = Ego.read<uint64_t>();
            if (w.push((uint8_t *)&val, sz))
                return;
        }
        suil_error::create("pushing varint failed");
    }
}