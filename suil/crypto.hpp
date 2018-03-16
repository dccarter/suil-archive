//
// Created by dc on 11/26/17.
//

#ifndef SUIL_ALGS_HPP
#define SUIL_ALGS_HPP

#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <openssl/ec.h>

#include <suil/sys.hpp>

#define BASE58_ADDR_MAX_LEN     36
#define BASE58_KEY_MAX_LEN      53
#define SUIL_PUBKEY_LEN         33
#define SUIL_CHECKSUM_LEN       4
#define SUIL_VERSION_LEN        1
#define SUIL_BINARY_KEYLEN      (SUIL_PUBKEY_LEN+SUIL_CHECKSUM_LEN+SUIL_VERSION_LEN)

namespace suil {

    typedef suil::Blob<SUIL_PUBKEY_LEN>         PubkeyBlob;
    typedef suil::Blob<32>                      PrivkeyBlob;
    typedef suil::Blob<SHA256_DIGEST_LENGTH>    SHA256Blob;
    typedef suil::Blob<BASE58_ADDR_MAX_LEN>     Base58Blob;
    typedef suil::Blob<64>                      ECDSASigBlob;
    typedef suil::Blob<RIPEMD160_DIGEST_LENGTH> RIPEMD160Blob;
    typedef SHA256Blob Hash;

    namespace crypto {
        typedef suil::Blob<1 + RIPEMD160_DIGEST_LENGTH + SUIL_CHECKSUM_LEN> AddressBLOB;
        typedef suil::Blob<SUIL_BINARY_KEYLEN>   NetAddrBlob;
        typedef suil::Blob<SHA_DIGEST_LENGTH>    Address;
        typedef decltype(iod::D(
                prop(pubkey,         PubkeyBlob),
                prop(privkey,        PrivkeyBlob)
        )) KeyPair;

        struct address_bin;
        struct netaddr_bin;

        struct pubkey_bin : PubkeyBlob {
            inline uint8_t *key() { return &Ego.bin(); }
            inline uint8_t &compressed() { return Ego.bin<SUIL_PUBKEY_LEN-1>(); }
            inline const uint8_t *ckey() const { return &Ego.cbin(); }
            inline const uint8_t &ccompressed() const { return Ego.cbin<SUIL_PUBKEY_LEN-1>(); }
            bool address(address_bin &) const;
            bool address(Address&) const;
        } __attribute__((aligned(1)));

        struct privkey_bin : PrivkeyBlob {
            inline uint8_t *key()              { return Ego.begin(); }
            inline const uint8_t *ckey() const { return Ego.cbegin(); }
            bool  pub(pubkey_bin&) const;
            bool  pub(Address&) const;
        } __attribute__((aligned(1)));

        struct sha256_bin : SHA256Blob {
            inline uint8_t *sha()              { return Ego.begin(); };
            inline const uint8_t *csha() const { return Ego.cbegin(); };
        } __attribute__((aligned(1)));

        struct address_bin : AddressBLOB {
            inline uint8_t &ver()  { return Ego.bin(); }
            inline Address &addr() { return (Address &)Ego.bin<1>(); }
            inline uint8_t *csum() { return &Ego.bin<1 + RIPEMD160_DIGEST_LENGTH>(); }
            inline const uint8_t &cver()  const { return Ego.cbin(); }
            inline const Address &caddr() const { return (Address &) Ego.cbin<1>(); }
            inline const uint8_t *ccsum() const { return &Ego.cbin<1 + RIPEMD160_DIGEST_LENGTH>(); }
        } __attribute__((aligned(1)));

        struct netaddr_bin: NetAddrBlob{
            inline uint8_t&     ver()  { return Ego.bin(); }
            inline pubkey_bin&  pub()  { return (pubkey_bin &) Ego.bin<1>(); }
            inline uint8_t*     csum() { return &Ego.bin<1+sizeof(pubkey_bin)>(); }
            inline const uint8_t&     cver()  const { return Ego.cbin(); }
            inline const pubkey_bin&  cpub()  const { return (pubkey_bin &) Ego.cbin<1>(); }
            inline const uint8_t*     ccsum() const { return &Ego.cbin<1+sizeof(pubkey_bin)>(); }
        } __attribute__((aligned(1)));

        struct base58_bin : Base58Blob {
            inline uint8_t *b58() { return Ego.begin(); }
            inline const uint8_t *cb58() const { return Ego.cbegin(); }
        };

        void doubleSHA256(sha256_bin &sha, const void *p, size_t len);
        void ripemd160(RIPEMD160Blob &ripemd, const void *p, size_t len);
        inline suil::zcstring doubleSHA256(const void *p, size_t len) {
            sha256_bin sha;
            doubleSHA256(sha, p, len);
            return suil::utils::hexstr(sha.data(), sha.size());
        }

        bool genPrivKey(privkey_bin& priv);

        namespace base58 {
            bool decode(const uint8_t in[], size_t sin, uint8_t out[], size_t& sout);

            inline bool decode(const suil::zcstring& b58, uint8_t out[], size_t& sout) {
                return decode((uint8_t *)b58.data(), b58.size(), out, sout);
            }
            char* encode(const uint8_t in[], size_t sin, uint8_t out[], size_t& nout);

            bool decode(const suil::zcstring &b58, privkey_bin& priv, pubkey_bin &pub);
            bool decode(const suil::zcstring &b58, pubkey_bin &pub);
            bool decode(const suil::zcstring &b58, privkey_bin &priv);
            bool decode(const suil::zcstring &b58, Address& addr);
            bool decode(const suil::zcstring& b58, address_bin& addr, bool vcs = false);
            suil::zcstring encode(const address_bin& addr);
            suil::zcstring encode(const netaddr_bin& addr);
            suil::zcstring encode(const privkey_bin& key);
            suil::zcstring encode(const pubkey_bin& key);
        }

        namespace ecdsa {

            struct signature_bin : ECDSASigBlob {
                inline uint8_t *r()   { return &Ego.bin<0>();  }
                inline uint8_t *s()   { return &Ego.bin<32>(); }
                inline const uint8_t *cr()   const { return &Ego.cbin<0>();  }
                inline const uint8_t *cs()   const { return &Ego.cbin<32>(); }
            };

            bool sign(const uint8_t msg[], size_t len, const privkey_bin &key, signature_bin &sig);

            inline bool sign(const suil::zcstring &msg, const privkey_bin &key, signature_bin &sig) {
                    return sign((uint8_t *) msg.data(), msg.size(), key, sig);
            }

            inline bool sign(const suil::zbuffer &msg, const privkey_bin &key, signature_bin &sig) {
                    return sign((uint8_t *) msg.data(), msg.size(), key, sig);
            }

            template<size_t N>
            inline bool sign(const suil::Blob<N> &msg, const privkey_bin &key, signature_bin &sig) {
                    return sign(msg.begin(), msg.size(), key, sig);
            }

            bool check(const uint8_t msg[], size_t len, const pubkey_bin& key, signature_bin &sig);

            inline bool check(const suil::zcstring &msg, const pubkey_bin &key, signature_bin &sig) {
                    return check((uint8_t *) msg.data(), msg.size(), key, sig);
            }

            inline bool check(const suil::zbuffer &msg, const pubkey_bin &key, signature_bin &sig) {
                    return check((uint8_t *) msg.data(), msg.size(), key, sig);
            }

            template<size_t N>
            inline bool check(const suil::Blob<N> &msg, const pubkey_bin &key, signature_bin &sig) {
                    return check(msg.begin(), msg.size(), key, sig);
            }
        }
    }

}

#endif //SUIL_ALGS_HPP
