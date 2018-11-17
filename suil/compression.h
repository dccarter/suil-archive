//
// Created by dc on 12/11/18.
//

#ifndef SUIL_COMPRESSION_H
#define SUIL_COMPRESSION_H

#include <suil/zstring.h>
#include <suil/buffer.h>

namespace suil {
    namespace utils {
        size_t compress(const uint8_t input[], size_t isz, uint8_t output[], size_t osz);

        Data compress(const uint8_t input[], size_t isz);

        inline Data compress(const OBuffer &in) {
            return compress((uint8_t *) in.data(), in.size());
        }

        inline Data compress(const String &in) {
            return compress((uint8_t *) in.data(), in.size());
        }

        inline Data compress(const Data &in) {
            return compress(in.cdata(), in.size());
        }

        bool uncompress(const uint8_t input[], size_t isz, uint8_t output[], size_t osz);

        Data uncompress(const uint8_t input[], size_t isz);

        inline Data uncompress(const OBuffer &in) {
            return uncompress((uint8_t *) in.data(), in.size());
        }

        inline Data uncompress(const String &in) {
            return uncompress((uint8_t *) in.data(), in.size());
        }

        inline Data uncompress(const Data &in) {
            return uncompress(in.cdata(), in.size());
        }
    }
}
#endif //SUIL_COMPRESSION_H
