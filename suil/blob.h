//
// Created by dc on 12/11/18.
//

#ifndef SUIL_BLOB_H
#define SUIL_BLOB_H

#include <iod/json.hh>
#include <suil/base64.h>

namespace suil {

    template <size_t N>
    struct Blob: std::array<uint8_t, N>, iod::MetaType {
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

        template<size_t NN>
        size_t copyfrom(const Blob<NN>& blob) {
            size_t n = MIN(N, NN);
            memcpy(Ego.begin(), blob.begin(), n);
            return n;
        }

        template <size_t S=0>
        inline void copy(const char* str) {
            size_t len{strlen(str)};
            Ego.copy<S>(str, len);
        };

        template  <size_t S=0>
        inline void copy(Data& d) {
            Ego.copy<S>(d.data(), d.size());
        }

        template  <size_t S=0>
        void copy(const void* data, size_t sz) {
            if ((S+sz)>N)
                throw Exception::create(Exception::InvalidArguments, "Data cannot fit into blob");
            memcpy(&Ego.begin()[S], data, sz);
        }

        template <size_t S, size_t NN>
        inline void copy(const Blob<NN> bb) {
            Ego.copy<S>(bb.begin(), bb.size());;
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

        template <size_t S=0, size_t C=N>
        inline bool nil() const {
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

        static Blob<N> fromJson(iod::json::parser& p);

        void toJson(iod::json::jstream& ss) const;
    } __attribute__((aligned(1)));

    // overhead is 0 byte
    static_assert(sizeof(Blob<1>) == 1);
}

#endif //SUIL_BLOB_H
