//
// Created by dc on 8/28/17.
//
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <suil/http/auth.hpp>

namespace suil {
    namespace http {

        bool jwt_t::decode(jwt_t& jwt,zcstring &&jwtstr, zcstring& secret) {
            /* header.payload.signature */
            char *tok_sig = strrchr(jwtstr.str, '.');
            if (tok_sig == NULL) {
                /* invalid json token */
                throw error::unauthorized(
                        "invalid token");
            }
            *tok_sig++ = '\0';
            zcstring data(jwtstr.cstr);

            /* get signature hash  */
            zcstring signature =
                    utils::HMAC_Sha256(secret, data, true);
            strace("token signature orig:%s, generated %s",
                   tok_sig, signature.cstr);

            if (strcmp(tok_sig, signature.cstr)) {
                /* token invalid */
                return false;
            }

            auto parts = std::move(utils::strsplit(data, "."));
            if (parts.size() != 2) {
                /* invalid token */
                /* invalid json token */
                throw error::unauthorized(
                        "invalid token");
            }

            zcstring header(base64::decode(parts[0]));
            strace("header: %s", header.cstr);
            iod::json_decode(jwt.header, header);
            zcstring payload(base64::decode(parts[1]));
            strace("header: %s", payload.cstr);
            iod::json_decode(jwt.payload, payload);
            return true;
        }

        bool jwt_t::verify(zcstring&& jwtstr, zcstring &secret) {
            /* header.payload.signature */
            char *tok_sig = strrchr(jwtstr.str, '.');
            if (tok_sig == NULL) {
                /* invalid json token */
                throw error::unauthorized(
                        "invalid token");
            }
            *tok_sig++ = '\0';
            zcstring data(jwtstr.cstr);

            /* get signature hash  */
            zcstring signature =
                    utils::HMAC_Sha256(secret, data);
            strace("token signature orig:%s, generated %s",
                   tok_sig, signature.cstr);

            return strcmp(tok_sig, signature.cstr) == 0;
        }

        zcstring jwt_t::encode(zcstring& secret) {
            /* encode jwt */
            /* 1. base64(hdr) & base64(payload)*/
            std::string raw_hdr(iod::json_encode(header));
            zcstring hdr(std::move(base64::encode(raw_hdr)));
            std::string raw_data(iod::json_encode(payload));
            zcstring data(std::move(base64::encode(raw_data)));

            /* 2. base64(hdr).base64(payload) */
            buffer_t tmp(hdr.len + data.len + 4);
            tmp << hdr << "." << data;

            /* 3. HMAC_Sha256 (base64(hdr).base64(payload))*/
            zcstring signature =
                    utils::HMAC_Sha256(secret,
                                       (const uint8_t *) tmp.data(),
                                       tmp.size(), true);

            /* 4. header.payload.signature */
            tmp << "." << signature;
            strace("encoded jwt: %s", tmp.data());

            return std::move(zcstring(tmp));
        }

        zcstring rand_8byte_salt::operator()(const zcstring &) {
            /* simple generate random bytes */
            uint8_t key[8];
            RAND_bytes(key, 8);
            return std::move(utils::bytestr(key, 8));
        }

        zcstring pbkdf2_sha1_hash::operator()(zcstring &passwd, zcstring& salt) {
            /* simply hash using SHA_256 appsalt.passwd.salt*/
            zcstring tmp = utils::catstr(appsalt, ".", salt);
            uint8_t out[SHA_DIGEST_LENGTH];
            int rc = PKCS5_PBKDF2_HMAC_SHA1(passwd.cstr, passwd.len,
                              (const uint8_t*)tmp.cstr, tmp.len,
                                   1000, SHA_DIGEST_LENGTH, out);

            if (rc) {
                /* convert buffer to byte string */
                return std::move(utils::bytestr(out, SHA_DIGEST_LENGTH));
            }

            return zcstring();
        }
    }
}