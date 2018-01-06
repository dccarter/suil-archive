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

        SuilError::create("pulling buffer failed");
    }

    void zbuffer::out(Wire &w) const {
        zcstring tmp{(const char *)Ego.data_, Ego.size(), false};
        w << tmp;
    }

    void heapboard::out(Wire &w) const {
        if (&w == this) {
            SuilError::create("infinite loop detected, serializing heapboad to self");
        }
        zcstring tmp{(const char *) Ego.data, Ego.size(), false};
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

        SuilError::create("pulling buffer failed");
    }
}