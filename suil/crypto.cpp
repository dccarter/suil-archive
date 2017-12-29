/* Converted to C by Rusty Russell, based on bitcoin source: */
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <openssl/obj_mac.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>

#include "crypto.hpp"

using namespace suil;

namespace sodoin {

    namespace crypto {

        static const char enc_16[] = "0123456789abcdef";
        static const char enc_58[] =
                "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

        static char encode_char(unsigned long val, const char *enc) {
            assert(val < strlen(enc));
            return enc[val];
        }

        static int decode_char(char c, const char *enc) {
            const char *pos = strchr(enc, c);
            if (!pos)
                return -1;
            return pos - enc;
        }

        /*
         * Encode a byte sequence as a base58-encoded string.  This is a bit
         * weird: returns pointer into buf (or NULL if wouldn't fit).
         */
        static char *encode_base58(char *buf, size_t buflen,
                                   const uint8_t *data, size_t data_len) {
            char *p;
            BIGNUM bn{};

            /* Convert to a bignum. */
            BN_init(&bn);
            BN_bin2bn(data, data_len, &bn);

            /* Add NUL terminator */
            if (!buflen) {
                p = NULL;
                goto out;
            }
            p = buf + buflen;
            *(--p) = '\0';

            /* Fill from the back, using a series of divides. */
            while (!BN_is_zero(&bn)) {
                int rem = BN_div_word(&bn, 58);
                if (--p < buf) {
                    p = NULL;
                    goto out;
                }
                *p = encode_char(rem, enc_58);
            }

            /* Now, this is really weird.  We pad with zeroes, but not at
             * base 58, but in terms of zero bytes.  This means that some
             * encodings are shorter than others! */
            while (data_len && *data == '\0') {
                if (--p < buf) {
                    p = NULL;
                    goto out;
                }
                *p = encode_char(0, enc_58);
                data_len--;
                data++;
            }

            out:
            BN_free(&bn);
            return p;
        }

/*
 * Decode a base_n-encoded string into a byte sequence.
 */
        bool raw_decode_base_n(BIGNUM *bn, const char *src, size_t len, int base) {
            const char *enc;

            BN_zero(bn);

            assert(base == 16 || base == 58);
            switch (base) {
                case 16:
                    enc = enc_16;
                    break;
                case 58:
                    enc = enc_58;
                    break;
            }

            while (len) {
                char current = *src;

                if (base == 16)
                    current = tolower(current);    /* TODO: Not in ccan. */
                int val = decode_char(current, enc);
                if (val < 0) {
                    BN_free(bn);
                    return false;
                }
                BN_mul_word(bn, base);
                BN_add_word(bn, val);
                src++;
                len--;
            }

            return true;
        }

        /*
         * Decode a base58-encoded string into a byte sequence.
         */
        bool rawDecodeBase58(BIGNUM *bn, const char *src, size_t len) {
            return raw_decode_base_n(bn, src, len, 58);
        }

        static inline void SHA256_UpdateFinal(SHA256_CTX *ctx, sha256_bin &sha) {
            SHA256_Final(sha.sha(), ctx);
            SHA256_Init(ctx);
            SHA256_Update(ctx, sha.sha(), SHA256_DIGEST_LENGTH);
            SHA256_Final(sha.sha(), ctx);
        }

        void doubleSHA256(sha256_bin &sha, const void *p, size_t len) {
            SHA256_CTX sha256{};
            SHA256_Init(&sha256);
            // Double SHA256
            SHA256_Update(&sha256, p, len);
            SHA256_UpdateFinal(&sha256, sha);
        }

        bool genPrivKey(privkey_bin& bin) {
            EC_KEY *key = EC_KEY_new_by_curve_name(NID_secp256k1);
            const BIGNUM    *privkey;

            size_t  keylen;

            if (key == nullptr) {
                serror("OpenSSL in use misses support for secp256k1");
                goto fail_free_priv;
            }
            if (EC_KEY_generate_key(key) != 1) {
                serror("generating wallet private key failed: %s", errno_s);
                goto fail_free_priv;
            }

            // get private key
            privkey = EC_KEY_get0_private_key(key);
            keylen = (size_t) BN_num_bytes(privkey);
            if (keylen != SODOIN_PUBKEY_LEN-1) {
                serror("unpacking private key (len=%lu)", keylen);
                goto fail_free_priv;
            }
            BN_bn2bin(privkey, bin.key());

            return true;

        fail_free_priv:
            return false;
        }

        static void base58Checksum(uint8_t *csum, const uint8_t *buf, size_t buflen) {
            sha256_bin dsha{};
            /* Form checksum, using double SHA2 (as per bitcoin standard) */
            doubleSHA256(dsha, buf, buflen);
            /* Use first four bytes of that as the checksum. */
            memcpy(csum, dsha.sha(), 4);
        }

        // Thus function based on bitcoin's key.cpp:
        // Copyright (c) 2009-2012 The Bitcoin developers
        // Distributed under the MIT/X11 software license, see the accompanying
        // file COPYING or http://www.opensource.org/licenses/mit-license.php.
        static bool EC_KEY_genKey(EC_KEY *eckey, BIGNUM *priv_key) {
            BN_CTX *ctx = nullptr;
            EC_POINT *pub_key = nullptr;
            const EC_GROUP *group = EC_KEY_get0_group(eckey);

            if ((ctx = BN_CTX_new()) == nullptr) {
                strace("allocate big number context failure");
                return false;
            }

            pub_key = EC_POINT_new(group);
            if (pub_key == nullptr) {
                strace("EC_POINT_new returns nullptr");
                return false;
            }

            if (!EC_POINT_mul(group, pub_key, priv_key, nullptr, nullptr, ctx)) {
                strace("EC_POINT_mul returns nullptr");
                return false;
            }

            EC_KEY_set_private_key(eckey, priv_key);
            EC_KEY_set_public_key(eckey, pub_key);

            BN_CTX_free(ctx);
            EC_POINT_free(pub_key);
            return true;
        }

        bool pubkey_bin::address(address_bin &ripemd) const {
            if (Ego.address(ripemd.addr())) {
                ripemd.ver() = 0x03;
                return true;
            }
            return false;
        }

        bool pubkey_bin::address(Address &addr) const {
            SHA256_CTX ctx{};
            uint8_t sha[SHA256_DIGEST_LENGTH];

            SHA256_Init(&ctx);
            SHA256_Update(&ctx, Ego.ckey(), SODOIN_PUBKEY_LEN-1);
            SHA256_Final(sha, &ctx);

            RIPEMD160(sha, sizeof(sha), &addr.bin());
            return true;
        }

        bool privkey_bin::pub(pubkey_bin &pub) const {
            size_t  keylen;
            uint8_t *pubkey;
            EC_KEY  *ppriv;
            BIGNUM  bn{};
            bool    status{false};

            ppriv = EC_KEY_new_by_curve_name(NID_secp256k1);
            /* We *always* used compressed form keys. */
            EC_KEY_set_conv_form(ppriv, POINT_CONVERSION_COMPRESSED);

            BN_init(&bn);
            // key to bignum, trim verison out
            if (!BN_bin2bn(Ego.ckey(), SHA256_DIGEST_LENGTH, &bn)) {
                serror("converting binary key to big number failed");
                goto fail_free_priv;
            }

            if (!EC_KEY_genKey(ppriv, &bn)) {
                serror("generating key failed");
                goto fail_free_bn;
            }

            /* Save keypair key */
            pubkey  = pub.key();
            keylen  = (size_t) i2o_ECPublicKey(ppriv, &pubkey);
            if (keylen != SODOIN_PUBKEY_LEN) {
                serror("packing public key (len=%lu)", keylen);
                goto fail_free_bn;
            }

            status = true;

        fail_free_bn:
            BN_free(&bn);

        fail_free_priv:
            EC_KEY_free(ppriv);

            return status;
        }

        bool privkey_bin::pub(Address &addr) const {
            pubkey_bin pub;
            if (Ego.pub(pub)) {
                return pub.address(addr);
            }
            return false;
        }

        namespace base58 {

            char* encode(const uint8_t in[], size_t sin, uint8_t out[], size_t& nout) {
                char *p = encode_base58((char *)in, sin, out, nout);
                nout = nout-(size_t)(p-(char *)out);
                return p;
            }

            bool decode(const uint8_t in[], size_t sin, uint8_t out[], size_t& sout) {
                BIGNUM bn{};
                BN_init(&bn);
                if (!rawDecodeBase58(&bn, (char *) in, sin)) {
                    BN_free(&bn);
                    return false;
                }

                int bytes = BN_num_bytes(&bn);
                if (bytes > sout) {
                    serror("insufficient output buffer size %lu/%d", sout, bytes);
                    BN_free(&bn);
                    return false;
                }
                BN_bn2bin(&bn, out);
                sout = (size_t) bytes;
                BN_free(&bn);

                return true;
            }

            bool encode(const privkey_bin& key, base58BLOB& out) {
                netaddr_bin addr{};
                char *p{nullptr};

                // mark this as a compressed key.
                addr.pub().compressed() = 1;
                addr.ver() = 0x03;

                // copy key to address
                memcpy(addr.pub().key(), key.ckey(), sizeof(key));

                // append checksum
                base58Checksum(addr.csum(), &addr.bin(), (1 + SODOIN_PUBKEY_LEN));
                sinfo("before_encode: %s", addr.hexstr()());
                p = encode_base58((char *)out.begin(), BASE58_KEY_MAX_LEN, &addr.bin(), sizeof(addr));

                return p != nullptr;
            }

            suil::zcstring encode(const privkey_bin& key) {
                netaddr_bin addr{};
                char out[BASE58_KEY_MAX_LEN];
                char *p{nullptr};

                // mark this as a compressed key.
                addr.pub().compressed() = 1;
                addr.ver() = 0x03;

                // copy key to address
                memcpy(addr.pub().key(), key.ckey(), sizeof(key));

                // append checksum
                base58Checksum(addr.csum(), &addr.bin(), (1 + SODOIN_PUBKEY_LEN));
                p = encode_base58(out, BASE58_KEY_MAX_LEN, &addr.bin(), sizeof(addr));

                return zcstring{p}.dup();
            }

            suil::zcstring encode(const pubkey_bin& key) {
                netaddr_bin addr{};
                char out[BASE58_KEY_MAX_LEN];
                char *p{nullptr};
                // set the version
                addr.ver() = 0x03;
                // copy key to address
                memcpy(addr.pub().key(), key.ckey(), sizeof(key));

                // append checksum
                base58Checksum(addr.csum(), &addr.bin(), (1 + SODOIN_PUBKEY_LEN));
                p = encode_base58(out, BASE58_KEY_MAX_LEN, &addr.bin(), sizeof(addr));

                return zcstring{p}.dup();
            }

            suil::zcstring encode(const Address& addr) {
                char out[BASE58_ADDR_MAX_LEN];
                char *p = encode_base58(out, BASE58_KEY_MAX_LEN, &addr.cbin(), addr.size());
                return zcstring{p}.dup();
            }

            suil::zcstring encode(const netaddr_bin& addr) {
                char out[BASE58_KEY_MAX_LEN];
                char *p{nullptr};
                p =  encode_base58(out, BASE58_ADDR_MAX_LEN,
                                   &addr.cbin(), sizeof(addr));
                return zcstring{p}.dup();
            }

            suil::zcstring encode(const address_bin& ripemd) {
                char out[BASE58_ADDR_MAX_LEN];
                char *p{nullptr};
                address_bin tmp;
                tmp.copyfrom(ripemd);
                base58Checksum(tmp.csum(), &tmp.cbin(), (1 + RIPEMD160_DIGEST_LENGTH));
                // encode the ripemd160
                p = encode_base58(out, BASE58_ADDR_MAX_LEN, &tmp.cbin(), sizeof(tmp));
                return zcstring{p}.dup();
            }

            bool decode(const suil::zcstring &b58, pubkey_bin &pub) {
                size_t keylen;
                netaddr_bin addr{};
                uint8_t csum[4];

                BIGNUM bn{};
                bool   status{false};

                BN_init(&bn);
                if (!rawDecodeBase58(&bn, b58.cstr, b58.len)) {
                    serror("decoding invalid base58 '%s' failed", b58("nil"));
                    goto fail_free_bn;
                }

                keylen = (size_t) BN_num_bytes(&bn);
                /* sodoin always uses compressed keys. */
                if (keylen != SODOIN_BINARY_KEYLEN) {
                    serror("sodoin key length %d invalid (expected: %d)",
                           keylen, SODOIN_BINARY_KEYLEN);
                    goto fail_free_bn;
                }

                BN_bn2bin(&bn, &addr.bin());
                base58Checksum(csum, &addr.bin(), keylen - sizeof(csum));

                if (memcmp(csum, addr.csum(), sizeof(csum)) != 0) {
                    serror("checksum validation failed %0X/%0X",
                           *((uint32_t *) csum), *((uint32_t *) addr.csum()));
                    goto fail_free_bn;
                }

                if (addr.ver() != 0x03) {
                    serror("unsupported version '%hhu' in base58", addr.ver());
                    goto fail_free_bn;
                }

                memcpy(pub.key(), addr.pub().key(), sizeof(pub));
                status = true;

            fail_free_bn:
                BN_free(&bn);
                return status;
            }

            bool decode(const suil::zcstring &b58, privkey_bin &priv) {
                size_t keylen;
                netaddr_bin addr{};
                uint8_t csum[4];

                BIGNUM bn{};
                bool   status{false};

                BN_init(&bn);
                if (!rawDecodeBase58(&bn, b58.cstr, b58.len)) {
                    serror("decoding invalid base58 '%s' failed", b58("nil"));
                    goto fail_free_bn;
                }

                keylen = (size_t) BN_num_bytes(&bn);
                /* sodoin always uses compressed keys. */
                if (keylen != SODOIN_BINARY_KEYLEN) {
                    serror("sodoin key length %d invalid (expected: %d)",
                           keylen, SODOIN_BINARY_KEYLEN);
                    goto fail_free_bn;
                }

                BN_bn2bin(&bn, &addr.bin());
                base58Checksum(csum, &addr.bin(), keylen - sizeof(csum));

                if (memcmp(csum, addr.csum(), sizeof(csum)) != 0) {
                    serror("checksum validation failed %0X/%0X",
                           *((uint32_t *) csum), *((uint32_t *) addr.csum()));
                    goto fail_free_bn;
                }

                if (addr.ver() != 0x03) {
                    serror("unsupported version '%hhu' in base58", addr.ver());
                    goto fail_free_bn;
                }

                memcpy(priv.key(), addr.pub().key(), sizeof(priv));
                status = true;

                fail_free_bn:
                BN_free(&bn);
                return status;
            }

            bool decode(const suil::zcstring &b58, privkey_bin& priv, pubkey_bin &pub) {
                size_t keylen;
                uint8_t *pubkey, *privkey;
                netaddr_bin addr{};
                uint8_t csum[4];
                EC_KEY *ppriv;
                BIGNUM bn{};
                bool   status{false};

                BN_init(&bn);
                if (!rawDecodeBase58(&bn, b58.cstr, b58.len)) {
                    serror("decoding invalid base58 '%s' failed", b58("nil"));
                    goto fail_free_bn;
                }

                keylen = (size_t) BN_num_bytes(&bn);
                /* sodoin always uses compressed keys. */
                if (keylen != SODOIN_BINARY_KEYLEN) {
                    serror("sodoin key length %d invalid (expected: %d)",
                           keylen, SODOIN_BINARY_KEYLEN);
                    goto fail_free_bn;
                }

                BN_bn2bin(&bn, &addr.bin());
                base58Checksum(csum, &addr.bin(), keylen - sizeof(csum));

                if (memcmp(csum, addr.csum(), sizeof(csum)) != 0) {
                    serror("checksum validation failed %0X/%0X",
                           *((uint32_t *) csum), *((uint32_t *) addr.csum()));
                    goto fail_free_bn;
                }

                /* Byte after key should be 1 to represent a compressed key.*/
                if (addr.pub().compressed() != 1) {
                    serror("base58 key '%s' is not compressed %X",
                           b58.cstr, addr.pub().compressed());
                    goto fail_free_bn;
                }

                if (addr.ver() != 0x03) {
                    serror("unsupported version '%hhu' in base58", addr.ver());
                    goto fail_free_bn;
                }

                ppriv = EC_KEY_new_by_curve_name(NID_secp256k1);
                /* We *always* used compressed form keys. */
                EC_KEY_set_conv_form(ppriv, POINT_CONVERSION_COMPRESSED);

                BN_free(&bn);
                BN_init(&bn);
                // key to bignum, trim verison out
                if (!BN_bin2bn(addr.pub().key(), SHA256_DIGEST_LENGTH, &bn)) {
                    serror("converting binary key to big number failed");
                    goto fail_free_priv;
                }
                if (!EC_KEY_genKey(ppriv, &bn)) {
                    serror("generating key failed");
                    goto fail_free_priv;
                }

                /* Save keypair key */
                pubkey  = pub.key();
                keylen  = (size_t) i2o_ECPublicKey(ppriv, &pubkey);
                if (keylen != SODOIN_PUBKEY_LEN) {
                    serror("packing public key (len=%lu)", keylen);
                    goto fail_free_priv;
                }

                // copy private key
                memcpy(priv.key(), addr.pub().key(), sizeof(priv));
                status = true;

            fail_free_priv:
                EC_KEY_free(ppriv);
            fail_free_bn:
                BN_free(&bn);

                return status;
            }

            bool decode(const suil::zcstring& b58, address_bin& addr, bool vcs) {
                uint8_t buf[1 + RIPEMD160_DIGEST_LENGTH + 4];
                uint8_t csum[4];
                BIGNUM  bn{};
                size_t  len{0};

                if (b58.len > (BASE58_ADDR_MAX_LEN-1)) {
                    sdebug("base58 '%s' address to long", b58("nil"));
                    return false;
                }

                BN_init(&bn);
                if (!rawDecodeBase58(&bn, b58.cstr, b58.len)) {
                    sdebug("decoding base58 '%s' address failure", b58("nil"));
                    BN_free(&bn);
                    return false;
                }

                /* Too big? */
                len = (size_t) BN_num_bytes(&bn);
                if (len > sizeof(buf)) {
                    sdebug("decoded base58 address larger than output buffer %d/%d",
                           len, sizeof(buf));
                    BN_free(&bn);
                    return false;
                }

                /* Fill start with zeroes. */
                addr.zero(0, sizeof(buf) - len);
                BN_bn2bin(&bn, &addr[(sizeof(buf) - len)]);
                BN_free(&bn);

                if (vcs) {
                    /* Check checksum is correct. */
                    base58Checksum(csum, &addr.bin(), sizeof(buf));
                    if (memcmp(csum, addr.csum(), 4) != 0) {
                        sdebug("ripemd160 checksum verification failed: %u/%u",
                               *((uint32_t*)csum), *((uint32_t*) addr.csum()));
                        return false;
                    }
                }

                return true;
            }
        }

        namespace ecdsa {

            bool check(const uint8_t msg[], size_t len, const pubkey_bin& key, signature_bin &sig) {
                bool   status{false};
                BIGNUM        r, s;
                ECDSA_SIG     ssig{&r, &s};
                EC_KEY        *eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
                const uint8_t *k = key.ckey();
                sha256_bin    sha;

                // get double hash of message
                doubleSHA256(sha, msg, len);

                // unpack public key
                if (!o2i_ECPublicKey(&eckey, &k, SODOIN_PUBKEY_LEN)) {
                    strace("unpack public key '%s' failed: %s",
                           key.hexstr()(), errno_s);
                    goto ecdsa_check_out;
                }

                // S must be even: https://github.com/sipa/bitcoin/commit/a81cd9680
                if (sig.s()[31]&1) {
                    strace("signature not even (%hhu)", sig.s()[31]);
                    goto ecdsa_check_out;
                }

                // unpack signature
                BN_init(&r);
                BN_init(&s);

                if (!BN_bin2bn(sig.r(), 32, &r) ||
                    !BN_bin2bn(sig.s(), 32, &s))
                {
                    strace("unpacking signature failed");
                    goto ecdsa_check_free_bn;
                }

                // verify hash with pubkey and signature
                switch (ECDSA_do_verify(sha.sha(), sizeof(sha), &ssig, eckey)) {
                    case 0: {
                        strace("message signature is invalid");
                        goto ecdsa_check_free_bn;
                    }
                    case -1: {
                        strace("verifying message failed: %s", errno_s);
                        goto ecdsa_check_free_bn;
                    }
                    default: {
                        status = true;
                    }
                }

            ecdsa_check_free_bn:
                BN_free(&r);
                BN_free(&s);

            ecdsa_check_out:
                EC_KEY_free(eckey);
                return status;
            }

            bool sign(const uint8_t msg[], size_t len, const privkey_bin &key, signature_bin &sig) {
                ECDSA_SIG  *ssig;
                sha256_bin  sha;
                bool        status{false};
                EC_KEY     *eckey = EC_KEY_new_by_curve_name(NID_secp256k1);
                const uint8_t *k = key.ckey();
                int        sz{0};
                BIGNUM     bn{};

                // unpack private key
                BN_init(&bn);
                if (!BN_bin2bn(key.ckey(), sizeof(key), &bn)) {
                    strace("unpack private key '%s'", key.hexstr()(), errno_s);
                    goto ecdsa_check_sign_free_bn;
                }

                if (!EC_KEY_set_private_key(eckey, &bn)) {
                    strace("unpack private key '%s' failed: %s",
                           key.hexstr()(), errno_s);
                    goto ecdsa_check_sign_free_bn;
                }

                doubleSHA256(sha, msg, len);

                ssig = ECDSA_do_sign(sha.sha(), SHA256_DIGEST_LENGTH, eckey);
                if (nullptr == ssig) {
                    strace("signing message failed: %s", errno_s);
                    goto ecdsa_check_sign_free_bn;
                }

                /* See https://github.com/sipa/bitcoin/commit/a81cd9680.
                 * There can only be one signature with an even S, so make sure we
                 * get that one. */
                if (BN_is_odd(ssig->s)) {
                    const EC_GROUP *group;
                    BIGNUM order{};

                    BN_init(&order);
                    group = EC_KEY_get0_group(eckey);
                    EC_GROUP_get_order(group, &order, NULL);
                    BN_sub(ssig->s, &order, ssig->s);
                    BN_free(&order);

                    if (BN_is_odd(ssig->s)) {
                        strace("signature is still odd");
                        goto ecdsa_check_free_sig;
                    }
                }

                sig.szero();

                sz = BN_num_bytes(ssig->r);
                if (sz > 32) {
                    strace("unexpected %d bytes ECDSA_SIG.r", sz);
                    goto ecdsa_check_free_sig;
                }
                // big endian stuff
                BN_bn2bin(ssig->r, &sig.r()[32-sz]);

                sz = BN_num_bytes(ssig->s);
                if (sz > 32) {
                    strace("unexpected %d bytes ECDSA_SIG.s", sz);
                    goto ecdsa_check_free_sig;
                }
                // big endian stuff
                BN_bn2bin(ssig->s, &sig.s()[32-sz]);

                status = true;

            ecdsa_check_free_sig:
                ECDSA_SIG_free(ssig);
            ecdsa_check_sign_free_bn:
                BN_free(&bn);
            ecdsa_check_sign:
                EC_KEY_free(eckey);
                return status;
            }
        }
    }
}