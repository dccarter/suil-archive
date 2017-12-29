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
#define SODOIN_PUBKEY_LEN       33
#define SODOIN_CHECKSUM_LEN     4
#define SODOIN_VERSION_LEN      1
#define SODOIN_BINARY_KEYLEN    (SODOIN_PUBKEY_LEN+SODOIN_CHECKSUM_LEN+SODOIN_VERSION_LEN)

namespace sodoin {

    typedef suil::blob_t<SODOIN_PUBKEY_LEN> pubkeyBLOB;
    typedef suil::blob_t<32> privkeyBLOB;
    typedef suil::blob_t<SHA256_DIGEST_LENGTH> sha256BLOB;
    typedef suil::blob_t<1 + RIPEMD160_DIGEST_LENGTH + SODOIN_CHECKSUM_LEN> addressBLOB;
    typedef suil::blob_t<BASE58_ADDR_MAX_LEN> base58BLOB;
    typedef suil::blob_t<64> ecdsasigBLOB;
    typedef suil::blob_t<SODOIN_BINARY_KEYLEN> netaddrBLOB;
    typedef suil::blob_t<SHA_DIGEST_LENGTH> Address;

    namespace crypto {

        struct address_bin;
        struct netaddr_bin;
        struct pubkey_bin : pubkeyBLOB {
            inline uint8_t *key() { return &Ego.bin(); }
            inline uint8_t &compressed() { return Ego.bin<SODOIN_PUBKEY_LEN-1>(); }
            inline const uint8_t *ckey() const { return &Ego.cbin(); }
            inline const uint8_t &ccompressed() const { return Ego.cbin<SODOIN_PUBKEY_LEN-1>(); }
            bool address(address_bin &) const;
            bool address(Address&) const;
        } __attribute__((aligned(1)));

        struct privkey_bin : privkeyBLOB {
            inline uint8_t *key()              { return Ego.begin(); }
            inline const uint8_t *ckey() const { return Ego.cbegin(); }
            bool  pub(pubkey_bin&) const;
            bool  pub(Address&) const;
        } __attribute__((aligned(1)));

        struct sha256_bin : sha256BLOB {
            inline uint8_t *sha()              { return Ego.begin(); };
            inline const uint8_t *csha() const { return Ego.cbegin(); };
        } __attribute__((aligned(1)));

        struct address_bin : addressBLOB {
            inline uint8_t &ver()  { return Ego.bin(); }
            inline Address &addr() { return (Address &)Ego.bin<1>(); }
            inline uint8_t *csum() { return &Ego.bin<1 + RIPEMD160_DIGEST_LENGTH>(); }
            inline const uint8_t &cver()  const { return Ego.cbin(); }
            inline const Address &caddr() const { return (Address &) Ego.cbin<1>(); }
            inline const uint8_t *ccsum() const { return &Ego.cbin<1 + RIPEMD160_DIGEST_LENGTH>(); }
        } __attribute__((aligned(1)));

        struct netaddr_bin: netaddrBLOB{
            inline uint8_t&     ver()  { return Ego.bin(); }
            inline pubkey_bin&  pub()  { return (pubkey_bin &) Ego.bin<1>(); }
            inline uint8_t*     csum() { return &Ego.bin<1+sizeof(pubkey_bin)>(); }
            inline const uint8_t&     cver()  const { return Ego.cbin(); }
            inline const pubkey_bin&  cpub()  const { return (pubkey_bin &) Ego.cbin<1>(); }
            inline const uint8_t*     ccsum() const { return &Ego.cbin<1+sizeof(pubkey_bin)>(); }
        } __attribute__((aligned(1)));

        struct base58_bin : base58BLOB {
            inline uint8_t *b58() { return Ego.begin(); }
            inline const uint8_t *cb58() const { return Ego.cbegin(); }
        };

        void doubleSHA256(sha256_bin &sha, const void *p, size_t len);
        inline suil::zcstring doubleSHA256(const void *p, size_t len) {
            sha256_bin sha;
            doubleSHA256(sha, p, len);
            return suil::utils::hexstr(sha.data(), sha.size());
        }

        bool genPrivKey(privkey_bin& priv);

        namespace base58 {
            bool decode(const uint8_t in[], size_t sin, uint8_t out[], size_t& sout);

            inline bool decode(const suil::zcstring& b58, uint8_t out[], size_t& sout) {
                return decode((uint8_t *)b58.str, b58.len, out, sout);
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

            struct signature_bin : ecdsasigBLOB {
                inline uint8_t *r()   { return &Ego.bin<0>();  }
                inline uint8_t *s()   { return &Ego.bin<32>(); }
                inline const uint8_t *cr()   const { return &Ego.cbin<0>();  }
                inline const uint8_t *cs()   const { return &Ego.cbin<32>(); }
            };

            bool sign(const uint8_t msg[], size_t len, const privkey_bin &key, signature_bin &sig);

            inline bool sign(const suil::zcstring &msg, const privkey_bin &key, signature_bin &sig) {
                    return sign((uint8_t *) msg.str, msg.len, key, sig);
            }

            inline bool sign(const suil::buffer_t &msg, const privkey_bin &key, signature_bin &sig) {
                    return sign((uint8_t *) msg.data(), msg.size(), key, sig);
            }

            template<size_t N>
            inline bool sign(const suil::blob_t<N> &msg, const privkey_bin &key, signature_bin &sig) {
                    return sign(msg.begin(), msg.size(), key, sig);
            }

            bool check(const uint8_t msg[], size_t len, const pubkey_bin& key, signature_bin &sig);

            inline bool check(const suil::zcstring &msg, const pubkey_bin &key, signature_bin &sig) {
                    return check((uint8_t *) msg.str, msg.len, key, sig);
            }

            inline bool check(const suil::buffer_t &msg, const pubkey_bin &key, signature_bin &sig) {
                    return check((uint8_t *) msg.data(), msg.size(), key, sig);
            }

            template<size_t N>
            inline bool check(const suil::blob_t<N> &msg, const pubkey_bin &key, signature_bin &sig) {
                    return check(msg.begin(), msg.size(), key, sig);
            }
        }
    }

}

#endif //SUIL_ALGS_HPP
