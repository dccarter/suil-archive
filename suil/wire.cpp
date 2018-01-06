//
// Created by dc on 14/12/17.
//

#include "wire.hpp"

namespace suil {

    void zbuffer::in(wire &w) {
        varint sz(0);
        w >> sz;
        uint64_t tmp{sz.read<uint64_t>()};
        Ego.reserve(tmp);
        if (w.pull(&Ego.data_[Ego.offset_], tmp)) {
            Ego.offset_ += tmp;
            return;
        }

        suil_error::create("pulling buffer failed");
    }

    void zbuffer::out(wire &w) const {
        zcstring tmp{(const char *)Ego.data_, Ego.size(), false};
        w << tmp;
    }

}