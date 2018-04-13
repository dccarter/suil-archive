//
// Created by dc on 28/02/18.
//

#ifndef SUIL_MERKLE_HPP
#define SUIL_MERKLE_HPP

#include <suil/crypto.hpp>
#include <suil/wire.hpp>

namespace suil {

    template <typename Value>
    struct Merkle {
        Merkle(std::vector<Value>& vals, size_t dataMaxSize = 8912)
            : dataMaxSize(dataMaxSize),
              values(vals)
        {}

        inline suil::Hash computeHash() const {
            if (Ego.values.size() == 0) {
                return  suil::Hash(true);
            }
            std::vector<suil::Hash> hashes = Ego.hashValues();
            return std::move(Ego.computeRootHash(hashes));
        }

        inline suil::Hash operator()() const {
            return std::move(Ego.computeHash());
        }

    private:
        template <typename T>
        suil::Hash hashPair(T& val, T& var) const {
            suil::heapboard hb(Ego.dataMaxSize);
            hb << val << var;
            auto raw = hb.raw();
            // binary hash only
            suil::crypto::sha256_bin hash;
            suil::crypto::doubleSHA256(hash, raw.data(), raw.size());
            return std::move((suil::Hash) hash);
        }

        std::vector<suil::Hash> hashValues() const {
            size_t loop = values.size() & ~0x1;
            std::vector<suil::Hash> tree;
            int i = 0;
            for (i = 0; i < loop; i +=2) {
                // pair, hash and push into bucket
                tree.push_back(Ego.hashPair(values[i], values[i+1]));
            }

            if (values.size()&0x1) {
                // the last odd value pairs to itself
                tree.push_back(Ego.hashPair(values[i], values[i]));
            }

            return std::move(tree);
        }

        suil::Hash computeRootHash(std::vector<suil::Hash>& hashes) const {
            if (hashes.size() == 1) {
                // parent hash, return it
                return std::move(hashes.front());
            }

            size_t loop = hashes.size() & ~0x1;
            std::vector<suil::Hash> tree;
            int i = 0;
            for (i = 0; i < loop; i +=2) {
                // pair, hash and push into bucket
                tree.push_back(Ego.hashPair(hashes[i], hashes[i+1]));
            }

            if ((hashes.size()&0x1)) {
                // the last odd value pairs to itself
                tree.push_back(Ego.hashPair(hashes[i], hashes[i]));
            }

            // we need to be efficient with space
            hashes.clear();
            return std::move(Ego.computeRootHash(tree));
        }

    private:
        size_t dataMaxSize{8912};
        std::vector<Value>& values;
    };

}

#endif //SUIL_MERKLE_HPP
