//
// Created by dc on 01/06/17.
//

#include <dirent.h>
#include <csignal>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <syslog.h>

#include <suil/utils.h>
#include <suil/base64.h>

namespace suil {
    
    void* utils::memfind(void *src, size_t slen, const void *needle, size_t len) {
        size_t pos;

        for (pos = 0; pos < slen; pos++) {
            if ( *((u_int8_t *)src + pos) != *(u_int8_t *)needle)
                continue;

            if ((slen - pos) < len)
                return (nullptr);

            if (!memcmp((u_int8_t *)src + pos, needle, len))
                return ((u_int8_t *)src + pos);
        }

        return (nullptr);
    }

    size_t utils::hexstr(const uint8_t *in, size_t ilen, char *out, size_t olen) {
        if (in == nullptr || out == nullptr || olen < (ilen<<1))
            return 0;

        size_t rc = 0;
        for (size_t i = 0; i < ilen; i++) {
            out[rc++] = i2c((uint8_t) (0x0F&(in[i]>>4)));
            out[rc++] = i2c((uint8_t) (0x0F&in[i]));
        }
        return rc;
    }

    String utils::hexstr(const uint8_t *buf, size_t len) {
        if (buf == nullptr)
            return String{};
        size_t rc = 2+(len<<1);
        auto *OUT = (char *)malloc(rc);

        String tmp{nullptr};
        rc = hexstr(buf, len, OUT, rc);
        if (rc) {
            OUT[rc] = '\0';
            tmp = String(OUT, (size_t)rc, true);
        } else
            ::free(OUT);

        return std::move(tmp);
    }

    void utils::bytes(const String &str, uint8_t *out, size_t olen) {
        size_t size = str.size()>>1;
        if (out == nullptr || olen < size)
            throw Exception::create("utils::bytes - output buffer invalid");

        char v;
        const char *p = str.data();
        for (int i = 0; i < size; i++) {
            out[i] = (uint8_t) (utils::c2i(*p++) << 4 | utils::c2i(*p++));
        }
    }

    String utils::urlencode(const String &str) {
        auto *buf((uint8_t *) malloc(str.size()*3));
        const char *src = str.data(), *end = src + str.size();
        uint8_t *dst = buf;
        uint8_t c;
        while (src != end) {
            c = (uint8_t) *src++;
            if (!isalnum(c) && strchr("-_.~", c) == nullptr) {
                static uint8_t hexchars[] = "0123456789ABCDEF";
                dst[0] = '%';
                dst[1] = hexchars[(c&0xF0)>>4];
                dst[2] = hexchars[(c&0x0F)];
                dst += 3;
            }
            else {
                *dst++ = c;
            }
        }
        *dst = '\0';

        return String((char *)buf, (dst - buf), true);
    }

    char *__urldecode(const char *src, const int src_len, char *out, int& out_sz)
    {
#define IS_HEX_CHAR(ch) \
        (((ch) >= '0' && (ch) <= '9') || \
         ((ch) >= 'a' && (ch) <= 'f') || \
         ((ch) >= 'A' && (ch) <= 'F'))

#define HEX_VALUE(ch, value) \
        if ((ch) >= '0' && (ch) <= '9') \
        { \
            (value) = (ch) - '0'; \
        } \
        else if ((ch) >= 'a' && (ch) <= 'f') \
        { \
            (value) = (ch) - 'a' + 10; \
        } \
        else \
        { \
            (value) = (ch) - 'A' + 10; \
        }

        const unsigned char *start;
        const unsigned char *end;
        char *dest;
        unsigned char c_high;
        unsigned char c_low;
        int v_high;
        int v_low;

        dest = out;
        start = (unsigned char *)src;
        end = (unsigned char *)src + src_len;
        while (start < end)
        {
            if (*start == '%' && start + 2 < end)
            {
                c_high = *(start + 1);
                c_low  = *(start + 2);

                if (IS_HEX_CHAR(c_high) && IS_HEX_CHAR(c_low))
                {
                    HEX_VALUE(c_high, v_high);
                    HEX_VALUE(c_low, v_low);
                    *dest++ = (char) ((v_high << 4) | v_low);
                    start += 3;
                }
                else
                {
                    *dest++ = *start;
                    start++;
                }
            }
            else if (*start == '+')
            {
                *dest++ = ' ';
                start++;
            }
            else
            {
                *dest++ = *start;
                start++;
            }
        }

        out_sz = (int) (dest - out);
        return dest;
    }

    String utils::urldecode(const char *src, size_t len)
    {
        char out[1024];
        int  size{1023};
        (void)__urldecode(src, (int)len, out, size);
        return String{out, (size_t)size, false}.dup();
    }

    void utils::randbytes(uint8_t out[], size_t size) {
        RAND_bytes(out, (int) size);
    }

    String utils::randbytes(size_t size) {
        uint8_t buf[size];
        RAND_bytes(buf, (int) size);
        return hexstr(buf, size);
    }

    String utils::md5(const uint8_t *data, size_t len) {
        if (data == nullptr)
            return String{};

        uint8_t RAW[MD5_DIGEST_LENGTH];
        MD5(data, len, RAW);
        return std::move(hexstr(RAW, MD5_DIGEST_LENGTH));
    }

    String utils::SHA_HMAC256(String &secret, const uint8_t *data, size_t len, bool b64) {
        if (data == nullptr)
            return String{};

        uint8_t *result = HMAC(EVP_sha256(), secret.data(), (int) secret.size(),
                                  data, len, nullptr, nullptr);
        if (b64) {
            return base64::encode(result, SHA256_DIGEST_LENGTH);
        }
        else {
            return std::move(hexstr(result, SHA256_DIGEST_LENGTH));
        }
    }

    String utils::sha256(const uint8_t *data, size_t len, bool b64) {
        if (data == nullptr)
            return String{nullptr};

        uint8_t *result = SHA256(data, len, nullptr);
        if (b64) {
            return base64::encode(hexstr(result, SHA256_DIGEST_LENGTH));
        }
        else {
            return hexstr(result, SHA256_DIGEST_LENGTH);
        }
    }

    static void _AES_init(uint8_t *keyData, int keyDataLen, uint8_t *salt, EVP_CIPHER_CTX *ctx, int rounds) {
        /* generate 256 bit key */
        uint8_t key[32], iv[32];

    }

    String AES_Encrypt(String &key, const uint8_t *buf, size_t size, bool b64 = true) {

    }

    uint8_t utils::c2i(char c) {
        if (c >= '0' && c <= '9') {
            return (uint8_t) (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            return (uint8_t) (c - 'W');
        } else if (c >= 'A' && c <= 'F') {
            return (uint8_t) (c - '7');
        }
        throw Exception::outOfRange("utils::c2i - character out range");
    };

    char utils::i2c(uint8_t c, bool caps) {

        if (c <= 0x9) {
            return c + '0';
        }
        else if (c <= 0xF) {
            if (caps)
                return c + '7';
            else
                return c + 'W';
        }
        throw Exception::outOfRange("utils::i2c - byte out of range");
    };

    int64_t utils::strtonum(const String &str, int base, long long int min, long long int max) {
        long long l;
        char *ep;

        if (min > max)
            throw Exception::outOfRange("utils::strtonum - min value specified is greater than max");

        errno = 0;
        l = strtoll(str.data(), &ep, base);
        if (errno != 0 || str.data() == ep || !matchany(*ep, '.', '\0')) {
            //@TODO
            // printf("strtoll error: (str = %p, ep = %p), *ep = %02X, errno = %d",
            //       str(), ep, *ep, errno);
            throw Exception::invalidArguments("utils::strtonum - %s", errno_s);
        }

        if (l < min)
            throw Exception::outOfRange("utils::strtonum -converted value is less than min value");

        if (l > max)
            throw Exception::outOfRange("utils::strtonum -converted value is greator than max value");

        return (l);
    }

    String utils::uuidstr(uuid_t id) {
        static uuid_t UUID;
        if (id == nullptr) {
            id = uuid(UUID);
        }
        size_t olen{(size_t)(20+(sizeof(uuid_t)))};
        char out[olen];
        int i{0}, rc{0};
        for(i; i<4; i++) {
            out[rc++] = i2c((uint8_t) (0x0F&(UUID[i]>>4)));
            out[rc++] = i2c((uint8_t) (0x0F& UUID[i]));
        }
        out[rc++] = '-';

        for (i; i < 10; i++) {
            out[rc++] = i2c((uint8_t) (0x0F&(UUID[i]>>4)));
            out[rc++] = i2c((uint8_t) (0x0F& UUID[i]));
            if (i&0x1) {
                out[rc++] = '-';
            }
        }

        for(i; i<sizeof(uuid_t); i++) {
            out[rc++] = i2c((uint8_t) (0x0F&(UUID[i]>>4)));
            out[rc++] = i2c((uint8_t) (0x0F& UUID[i]));
        }

        return String{out, (size_t) rc, false}.dup();
    }

    const char *utils::mimetype(const String&& filename) {
        const char *ext = strrchr(filename.data(), '.');
        static CaseMap<const char*> mimetypes = {
                {".html", "text/html"},
                {".css",  "text/css"},
                {".csv",  "text/csv"},
                {".txt",  "text/plain"},
                {".sgml", "text/sgml"},
                {".tsv",  "text/tab-separated-values"},
                // add compressed mime types
                {".bz",   "application/x-bzip"},
                {".bz2",  "application/x-bzip2"},
                {".gz",   "application/x-gzip"},
                {".tgz",  "application/x-tar"},
                {".tar",  "application/x-tar"},
                {".zip",  "application/zip, application/x-compressed-zip"},
                {".7z",   "application/zip, application/x-compressed-zip"},
                // add image mime types
                {".jpg",  "image/jpeg"},
                {".png",  "image/png"},
                {".svg",  "image/svg+xml"},
                {".gif",  "image/gif"},
                {".bmp",  "image/bmp"},
                {".tiff", "image/tiff"},
                {".ico",  "image/x-icon"},
                // add video mime types
                {".avi",  "video/avi"},
                {".mpeg", "video/mpeg"},
                {".mpg",  "video/mpeg"},
                {".mp4",  "video/mp4"},
                {".qt",   "video/quicktime"},
                // add audio mime types
                {".au",   "audio/basic"},
                {".midi", "audio/x-midi"},
                {".mp3",  "audio/mpeg"},
                {".ogg",  "audio/vorbis, application/ogg"},
                {".ra",   "audio/x-pn-realaudio, audio/vnd.rn-realaudio"},
                {".ram",  "audio/x-pn-realaudio, audio/vnd.rn-realaudio"},
                {".wav",  "audio/wav, audio/x-wav"},
                // Other common mime types
                {".json", "application/json"},
                {".js",   "application/javascript"},
                {".ttf",  "font/ttf"},
                {".xhtml","application/xhtml+xml"},
                {".xml",  "application/xml"}
        };

        if (ext != nullptr) {
            String tmp(ext);
            auto it = mimetypes.find(tmp);
            if (it != mimetypes.end())
                return it->second;
        }

        return "text/plain";
    }
}

#ifdef unit_test
#include <catch/catch.hpp>

using namespace suil;

TEST_CASE("Utils tests", "[common][utils]") {
    // tests some of the utility API's
    SECTION("c2i/i2c tests", "[common][utils][c2i][i2c]") {
        // test the boundaries of this functions
        char hello[] = {'H', 'e', 'l', 'l', 'o'};
        char olleh[] = {'4', '8', '6', '5', '6', 'c', '6', 'c', '6', 'f'};
        char OLLEH[] = {'4', '8', '6', '5', '6', 'C', '6', 'C', '6', 'F'};

        int j = 0;
        for (auto c: hello) {
            REQUIRE(utils::i2c((c >> 4) & 0xF) == olleh[j++]);
            REQUIRE(utils::i2c(c & 0xF) == olleh[j++]);
        }

        j = 0;
        for (auto c: hello) {
            REQUIRE(utils::i2c((c >> 4) & 0xF, true) == OLLEH[j++]);
            REQUIRE(utils::i2c(c & 0xF, true) == OLLEH[j++]);
        }

        j = 0;
        for (auto c: hello) {
            // it is just that simple
            REQUIRE(((utils::c2i(olleh[j++]) << 4) | utils::c2i(olleh[j++])) == c);
        }
    }

    SECTION("scoped resource test", "[common][utils][scoped]") {
        // test usage of a scoped resources
        struct Resource {

            void close() {
                Closed = true;
            }

            bool Closed{false};
        };
        Resource res{};
        REQUIRE_FALSE(res.Closed);
        {
            scoped(res1, res);
            // resource remains open until end of block
            REQUIRE_FALSE(res.Closed);
        }
        // after block resource should have been closed
        REQUIRE(res.Closed);

        res.Closed = false;
        auto fn = [&]() {
            scoped(res2, res);
            REQUIRE_FALSE(res.Closed);
            throw "";
        };
        // even when exceptions are thrown, scoped resource
        // must be closed
        try {
            fn();
        } catch (...) {}
        REQUIRE(res.Closed);
    }

    SECTION("cast tests", "[common][utils][cast][strtonum]") {
        // various casting test cases
        WHEN("Casting a String to a number") {
            int64_t num;
            double real;

            String str("100");
            utils::cast(str, num);
            REQUIRE(num == 100);
            utils::cast(str, real);
            REQUIRE(real == 100.0);

            str = "-5689.1299999";
            utils::cast(str, num);
            REQUIRE(num == -5689);
            utils::cast(str, real);
            REQUIRE(real == Approx(-5689.1299999));
        }

        WHEN("Casting a String to number with invalid arguments") {
            String str("167");
            // minimum can never be more than maximum
            REQUIRE_THROWS(utils::strtonum(str, 10, 100, 99));
            // number out of range
            REQUIRE_THROWS(utils::strtonum(str, 10, 99, 100));
            REQUIRE_THROWS(utils::strtonum(str, 10, 168, 200));
        }

        WHEN("Casting an invalid String to a number") {
            uint64_t num;
            double real;

            // has letters
            String str("100za3");
            REQUIRE_THROWS(utils::cast(str, num));
            REQUIRE_THROWS(utils::cast(str, real));

            // starts with letters
            str = "za3100";
            REQUIRE_THROWS(utils::cast(str, num));
            REQUIRE_THROWS(utils::cast(str, real));
        }

        WHEN("Castring String to std::string") {
            String from{"Hello World"};
            std::string to;
            utils::cast(from, to);
            CHECK("Hello World" == to);
        }

        WHEN("Casting a String to type using explicit cast operator") {
            String from{"1556.8"};
            int64_t i{0};
            float f{0.0};
            i = (int) from;
            f = (float) from;
            CHECK(i == 1556);
            CHECK(f == Approx(1556.8));
        }

        WHEN("Casting a String to String") {
            String from{"Hello World"};
            String to{};
            utils::cast(from, to);
            // same as input string
            CHECK(to.compare("Hello World") == 0);
            // exact reference to from string
            REQUIRE(to == from);
        }
    }

    SECTION("tostr tests", "[common][utils][tozcstr]") {
        WHEN("Converting a number number to String") {
            int64_t num{100};
            double real{100.05};

            // Converting non  float
            String str = utils::tostr(num);
            CHECK(str.compare("100") == 0);
            // Converting float
            str = utils::tostr(real);
            CHECK(str.compare("100.050000") == 0);

            // Converting from negative numbers
            num = -1005;
            real = -1.000009e3;
            str = utils::tostr(num);
            CHECK(str.compare("-1005") == 0);
            str = utils::tostr(real);
            CHECK(str.compare("-1000.009000") == 0);
        }

        WHEN("Converting other strings to String") {
            std::string s{"hello"};
            const char *cs{"hello"};
            String zc{"hello"};
            String tmp{};

            tmp = utils::tostr(s);
            CHECK(tmp.compare("hello") == 0);
            tmp = utils::tostr(cs);
            CHECK(tmp.compare("hello") == 0);
            tmp = utils::tostr(zc);
            CHECK(tmp.compare("hello") == 0);
            REQUIRE(tmp == zc);
        }
    }

    SECTION("utils::hashing", "[utils][hashing]")
    {
        WHEN("MD5 hashing") {
            String data[][2] = {
                    {"The quick brown fox",  "a2004f37730b9445670a738fa0fc9ee5"},
                    {"Jumped over the fire", "b6283454ffe0eb2b97c8d8d1b94062dc"},
                    {"Simple world",         "1b5b2eb127a6f47cad4ddcde1b5a5205"},
                    {"Suilman of SuilCity",  "f6ce722986cb80b57f153faf0ccf51f1"}
            };

            for (auto &i : data) {
                String out = utils::md5(i[0]);
                REQUIRE(out.compare(i[1]) == 0);
            }
        }

        WHEN("HMAC_Sha256 hashing") {
            // 8559f3e178983e4e83b20b5688f335c133c88015c51f3a9f7fcfde5cd4f613dc
            String sentence =
                    "The quick brown fox jumped over the lazy dog.\n"
                    "The lazy dog just lied there thinking 'the world is \n"
                    "circle, i'll get that quick brown fox'";
            String secret = "awfulSecret12345";

            // compute the hash
            String hashed = utils::SHA_HMAC256(secret, sentence);
            REQUIRE(hashed.compare("8559f3e178983e4e83b20b5688f335c133c88015c51f3a9f7fcfde5cd4f613dc") == 0);
        }
    }



    SECTION("Other utils", "[common][utils][catstr][mimetype]") {
        // other utils tests cases
        WHEN("Concatenating strings with catstr") {
            // test concatenating a string
            REQUIRE(String{"Hello World"} == utils::catstr("Hello", " World"));
            REQUIRE(String{"1 2 true 0.091000, Carter"} ==
                    utils::catstr(1, ' ', 2, ' ', true, ' ', 0.091, ',', ' ', "Carter"));
        }

        WHEN("Finding MIME type with mimetype") {
            // this are just basic tests since the API uses std::map
            const char* mmt{utils::mimetype("home.html")};
            REQUIRE_FALSE(mmt == nullptr);
            REQUIRE(strcmp(mmt, "text/html") == 0);
            mmt = utils::mimetype("api/data.json");
            REQUIRE_FALSE(mmt == nullptr);
            REQUIRE(strcmp(mmt, "application/json") == 0);
            mmt = utils::mimetype("api/data");
            REQUIRE_FALSE(mmt == nullptr);
            REQUIRE(strcmp(mmt, "text/plain") == 0);
            mmt = utils::mimetype("api/data.json.jj");
            REQUIRE_FALSE(mmt == nullptr);
            REQUIRE(strcmp(mmt, "text/plain") == 0);
            mmt = utils::mimetype("api/data.json.mp4");
            REQUIRE_FALSE(mmt == nullptr);
            REQUIRE(strcmp(mmt, "video/mp4") == 0);
        }
    }
}

#endif