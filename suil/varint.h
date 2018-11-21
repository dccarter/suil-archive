//
// Created by dc on 12/11/18.
//

#ifndef SUIL_VARINT_H
#define SUIL_VARINT_H

#include <endian.h>

#include <suil/base.h>

namespace suil {

    struct VarInt {

        VarInt(uint64_t v);

        VarInt();

        template <typename T>
        T read() const {
            static_assert(std::is_integral<T>::value, "VarInt only works with integral values");
            return (T) be64toh(*((uint64_t *) Ego.mData));
        }

        template <typename T>
        void write(T v) {
            static_assert(std::is_integral<T>::value, "VarInt only works with integral values");
            *((uint64_t *) Ego.mData) = htobe64((uint64_t) v);
        }

        template <typename T>
        inline VarInt& operator=(T v) {
            Ego.write(v);
            return Ego;
        }

        template <typename T>
        inline operator T() {
            static_assert(std::is_integral<T>::value, "VarInt only works with integral values");
            return Ego.read<T>();
        }

        uint8_t *raw();

        uint8_t length() const;

        inline bool operator==(const VarInt& other) const {
            return *((uint64_t *) mData) == *((uint64_t *) other.mData);
        }

        inline bool operator!=(const VarInt& other) const {
            return *((uint64_t *) mData) != *((uint64_t *) other.mData);
        }

    private suil_ut:
        uint8_t mData[sizeof(uint64_t)];
    };
}


#endif //SUIL_VARINT_H
