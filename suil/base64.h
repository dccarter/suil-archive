//
// Created by dc on 09/11/18.
//

#ifndef SUIL_BASE64_H
#define SUIL_BASE64_H

#include <suil/zstring.h>

namespace suil {

    namespace utils::base64 {

        void encode(OBuffer& ob, const uint8_t *, size_t);

        String encode(const uint8_t *, size_t);

        static String encode(const String &str) {
            return encode((const uint8_t *) str.data(), str.size());
        }

        static String encode(const std::string &str) {
            return encode((const uint8_t *) str.data(), str.size());
        }

        void decode(OBuffer& ob, const uint8_t *in, size_t len);

        String decode(const uint8_t *in, size_t len);

        static String decode(const char *in) {
            return decode((const uint8_t *) in, strlen(in));
        }

        static String decode(strview &sv) {
            return std::move(decode((const uint8_t *) sv.data(), sv.size()));
        }

        static String decode(const String &zc) {
            return std::move(decode((const uint8_t *) zc.data(), zc.size()));
        }
    }
}
#endif //SUIL_BASE64_H
