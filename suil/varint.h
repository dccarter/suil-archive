//
// Created by dc on 12/11/18.
//

#ifndef SUIL_VARINT_H
#define SUIL_VARINT_H

#include <suil/blob.h>

namespace suil {

    struct varint : Blob<8> {
        varint(uint64_t v);

        varint();

        template <typename T>
        T read() const {
            return (T) be64toh(*((uint64_t *) Ego.begin()));
        }

        template <typename T>
        void write(T v) {
            *((uint64_t *) Ego.begin()) = htobe64((uint64_t) v);
        }

        template <typename T>
        inline varint& operator=(T v) {
            Ego.write(v);
            return Ego;
        }

        template <typename T>
        inline operator T() {
            return Ego.read<T>();
        }

        uint8_t *raw();

        uint8_t length() const;
    };
}


#endif //SUIL_VARINT_H
