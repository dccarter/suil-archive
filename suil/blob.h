//
// Created by dc on 12/11/18.
//

#ifndef SUIL_BLOB_H
#define SUIL_BLOB_H

#include <suil/utils.h>
#include <suil/base64.h>

namespace suil {

    template <size_t N>
    struct Blob: iod::jsonvalue, std::array<uint8_t, N> {
        Blob()
            : Blob(true)
        {}

        Blob(bool zero)
        {
            if (zero) memset(Ego.begin(),0, N);
        }

        Blob(std::initializer_list<uint8_t> l)
                : std::array<uint8_t, N>()
        {
            if (l.size() > N)
                throw std::out_of_range("the size of the int list cannot be greater than array size");
            memcpy(Ego.begin(), l.begin(), l.size());
        }

        inline String base64() const {
            return utils::base64::encode(Ego.begin(), N);
        }

        inline String hexstr() const {
            return utils::hexstr(Ego.begin(), N);
        }

        inline String str() const { return Ego.hexstr(); }

        inline void fromhex(const String& hex) {
            utils::bytes(hex, Ego.begin(), MIN(hex.size()/2, N));
        }

        template <typename S>
        void encjv(S& ss) const {
            ss << '"';
            const uint8_t *p = (uint8_t *) Ego.begin();
            for (int i=0; i < N; i++) {
                ss << utils::i2c(p[i]>>4) << utils::i2c((uint8_t) (p[i]&0xF));
            }
            ss << '"';
        }

        template<size_t NN>
        size_t copyfrom(const Blob<NN>& blob) {
            size_t n = MIN(N, NN);
            memcpy(Ego.begin(), blob.begin(), n);
            return n;
        }

        template <size_t S=0>
        bool copy(const char* str) {
            size_t len{strlen(str)};
            if ((S+len) > N)
                return false;
            memcpy(&Ego.begin()[S], str, len);
            return true;
        };

        template <size_t S, size_t NN>
        bool copy(const Blob<NN> bb) {
            if ((S+bb.size())>N)
                return false;
            memcpy(&Ego.begin()[S], bb.begin(), bb.size());
            return true;
        };

        template <size_t S = 0, size_t E = N>
        suil::Blob<E-S> slice() const {
            static_assert(((E>S)&&(E<=N)), "invalid slicing indices");

            suil::Blob<E-S> blob;
            auto *p = (uint8_t *) Ego.begin();
            memcpy(blob.begin(), &p[S], E);
            return std::move(blob);
        };

        template <size_t S=0, size_t E = N>
        void szero() {
            static_assert(((E>S)&&(E<=N)), "invalid zeroing indices");
            memset(&Ego.begin()[S], 0, E-S);
        }

        void zero(size_t s=0, size_t e = N) {
            if ((e > s) && (e <= N)) {
                memset(&Ego.begin()[s], 0, e-s);
            }
        }

        static void decjv(iod::jdecit& jit, Blob<N>& ba) {
            auto *bap = (uint8_t *) ba.begin();
            char c, c1;
            jit.eat('"');
            for (int i = 0; i < N; i++) {
                if ((c = jit.next('"')) == '\0') break;
                c1 = jit.next('"');
                bap[i] = (uint8_t) ((utils::c2i(c)<<4) | utils::c2i(c1));
            }
            jit.eat('"');
        }

        template <size_t S=0, size_t C=N>
        inline bool isnill() const {
            static_assert((C<=N)&&((C+S)<=N), "null check range ins invalid");
            size_t offset{S};
            for(; offset < S+C; offset++)
                if (Ego.begin()[offset] != 0)
                    return false;
            return true;
        }

        template <size_t I=0>
        inline uint8_t& bin() {
            static_assert(I<N, "index should be less than size");
            return Ego.begin()[I];
        }

        template <size_t I=0>
        inline const uint8_t& cbin() const {
            static_assert(I<N, "index should be less than size");
            return Ego.begin()[I];
        }

    } __attribute__((aligned(1)));

    // overhead is 0 byte
    static_assert(sizeof(Blob<1>) == 1);
}

#endif //SUIL_BLOB_H
