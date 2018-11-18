//
// Created by dc on 09/11/18.
//

#include <libmill/utils.h>

#include "buffer.h"
#include "utils.h"
#include "zstring.h"

namespace suil {

    uint8_t& OBuffer::operator[](size_t index)
    {
        if (index <= m_offset) {
            return m_data[index];
        }
        throw Exception::indexOutOfBounds(index, " is too large");
    }

    OBuffer::OBuffer(size_t is)
            : m_data{nullptr},
              m_size(0),
              m_offset(0)
    {
        if (is)
            grow((uint32_t) is);
    }

    OBuffer::OBuffer(OBuffer && other) noexcept
            : m_data(other.m_data),
              m_size(other.m_size),
              m_offset(other.m_offset)
    {
        other.m_size = 0;
        other.m_offset = 0;
        other.m_data = nullptr;
    }

    OBuffer& OBuffer::operator=(OBuffer &&other) noexcept {
        m_size = other.m_size;
        m_data = other.m_data;
        m_offset = other.m_offset;
        other.m_data = nullptr;
        other.m_offset = other.m_size = 0;

        return *this;
    }

    OBuffer::~OBuffer() {
        if (m_data != nullptr) {
            ::free(m_data);
            m_data = nullptr;
            m_size = 0;
            m_offset = 0;
        }
    }

    ssize_t OBuffer::appendf(const char *fmt, ...) {
        ssize_t nwr{-1};
        va_list args;
        va_start(args, fmt);
        nwr = appendv(fmt, args);
        va_end(args);
        return nwr;
    }

    ssize_t OBuffer::appendv(const char *fmt, va_list args) {
        char	sb[2048];
        int ret;
        ret = vsnprintf(sb, sizeof(sb), fmt, args);
        if (ret == -1) {
            throw Exception::accessViolation("OBuffer::appendv(): ", errno_s);
        }
        return append(sb, (uint32_t) ret);
    }

    ssize_t OBuffer::appendnf(uint32_t hint, const char *fmt, ...) {
        ssize_t nwr{-1};
        va_list args;
        va_start(args, fmt);
        nwr = appendnv(hint, fmt, args);
        va_end(args);
        return  nwr;
    }

    ssize_t OBuffer::appendnv(uint32_t hint, const char *fmt, va_list args) {
        mill_assert(m_data && hint);

        if ((m_size-m_offset) < hint)
            grow((uint32_t) MAX(m_size, hint));

        int ret;
        ret = vsnprintf((char *)(m_data+m_offset), (m_size-m_offset), fmt, args);
        if (ret == -1 || (ret + m_offset) > m_size) {
            throw Exception::accessViolation("OBuffer::appendv - ", errno_s);
        }
        m_offset += ret;
        return ret;
    }

    ssize_t OBuffer::append(const void *data, size_t len) {
        if (len && data == nullptr)
            throw Exception::accessViolation("OBuffer::append - data cannot be null");

        if ((m_size-m_offset) < len) {
            grow((uint32_t) MAX(m_size, len));
        }

        memcpy((m_data+m_offset), data, len);
        m_offset += len;
        return len;
    }

    ssize_t OBuffer::append(time_t t, const char *fmt) {
        // reserve 64 bytes
        reserve(64);
        const char *pfmt = fmt? fmt : Datetime::HTTP_FMT;
        ssize_t sz = strftime((char*)(m_data+m_offset), 64, pfmt, localtime(&t));
        if (sz < 0)
            throw Exception::accessViolation("OBuffer::appendv - ", errno_s);

        m_offset += sz;
        return sz;
    }

#define __ALIGNED(x) ((x)&(sizeof(void*)-1))
#define __ALIGN2(x, d) ((d)? (sizeof(void*)-(d)) : (0))
#define __ALIGN(x) __ALIGN2(x, __ALIGNED(x))

    void OBuffer::grow(uint32_t add) {
        // check if the current OBuffer fits
        add += __ALIGN(add);
        m_data = (uint8_t *)::realloc(m_data,(m_size+add+1));
        if (m_data == nullptr)
            throw Exception::allocationFailure(
                    "OBuffer::grow failed ", std::string(errno_s));
        // change the size of the memory
        m_size = (uint32_t) m_size+add;
    }

    void OBuffer::reset(size_t size, bool keep) {
        m_offset = 0;
        if (!keep || (m_size < size)) {
            m_size = 0;
            size = (size < 8) ? 8 : size;
            grow((uint32_t) size);
        }
    }

    void OBuffer::seek(off_t off = 0) {
        off_t to = m_offset + off;
        bseek(to);
    }

    void OBuffer::bseek(off_t off) {
        if (off < m_size && off >= 0) {
            m_offset = (uint32_t) off;
        }
        else {
            throw Exception::outOfRange("OBuffer::bseek - seek offset out of range");
        }
    }

    void OBuffer::reserve(size_t size) {
        size_t  remaining = m_size - m_offset;
        if (size > remaining) {
            auto to = size - remaining;
            grow((uint32_t) MAX(m_size, to));
        }
    }

    char* OBuffer::release() {
        if (m_data) {
            char *raw = (char *) (*this);
            m_data = nullptr;
            clear();
            return raw;
        }
        return (char *)"";
    }

    void OBuffer::clear() {
        if (m_data) {
            ::free(m_data);
            m_data = nullptr;
        }
        m_offset = 0;
        m_size = 0;
    }

    OBuffer::operator char*() {
        if (m_data && m_data[m_offset] != '\0') {
            m_data[m_offset] = '\0';
        }
        return (char *) m_data;
    }

    OBuffer& OBuffer::operator<<(const suil::String &other) {
        if (other) append(other.c_str(), other.size());
        return *this;
    }

    ssize_t OBuffer::hex(const void *data, size_t size, bool caps) {
        reserve(size<<1);

        size_t rc = m_offset;
        const char *in = (char *)data;
        for (size_t i = 0; i < size; i++) {
            (char &) m_data[m_offset++] = utils::i2c((uint8_t) (0x0F&(in[i]>>4)), caps);
            (char &) m_data[m_offset++] = utils::i2c((uint8_t) (0x0F&in[i]), caps);
        }

        return m_offset - rc;
    }
}

#ifdef unit_test
#include <catch/catch.hpp>

using namespace suil;

#define __TALIGN(x) ((x)+ __ALIGN(x))

static_assert(__TALIGN(3)  == sizeof(void*));
static_assert(__TALIGN(8)  == sizeof(void*));
static_assert(__TALIGN(11) == 2*sizeof(void*));

auto __Check = [](OBuffer& ob, ssize_t from, const void *data, size_t count) {
    if ((from + count) > ob.m_offset) return false;
    return memcmp(data, &ob[(int)from], count) == 0;
};

TEST_CASE("OBuffer tests", "[common][obuffer]")
{
    SECTION("Creating a buffer") {
        OBuffer ob;
        REQUIRE(ob.m_size   == 0);
        REQUIRE(ob.m_data   == nullptr);
        REQUIRE(ob.m_offset == 0);

        OBuffer ob2(64);
        REQUIRE(ob2.m_size == __TALIGN(64));
        REQUIRE(ob2.m_offset == 0);
        REQUIRE(ob2.m_data != nullptr);

        ob2 = OBuffer(63);
        REQUIRE(ob2.m_size == __TALIGN(63));
        REQUIRE(ob2.m_offset == 0);
        REQUIRE(ob2.m_data != nullptr);

        OBuffer ob3 = std::move(ob2);
        // buffer should have been moved
        REQUIRE(ob3.m_size == __TALIGN(63));
        REQUIRE(ob3.m_offset == 0);
        REQUIRE(ob3.m_data != nullptr);
        REQUIRE(ob2.m_size == 0);
        REQUIRE(ob2.m_offset == 0);
        REQUIRE(ob2.m_data == nullptr);

        OBuffer ob4(std::move(ob3));
        // buffer should have been moved
        REQUIRE(ob4.m_size == __TALIGN(63));
        REQUIRE(ob4.m_offset == 0);
        REQUIRE(ob4.m_data != nullptr);
        REQUIRE(ob3.m_size == 0);
        REQUIRE(ob3.m_offset == 0);
        REQUIRE(ob3.m_data == nullptr);
    }

    SECTION("Appending to a buffer", "[buffer][append]") {
        // test append to a buffer
        OBuffer ob(128);
        WHEN("binary appending data") {
            // test appending numbers in binary format
            char c{(char )0x89};
            auto appendCheck = [&](auto t) {
                ssize_t  ss{0}, from = ob.m_offset;
                ss = ob.append(t);
                REQUIRE(ss == sizeof(t));
                REQUIRE(__Check(ob, from, &t, sizeof(t)));
            };

            appendCheck((char)               0x89);
            appendCheck((short)              0x89);
            appendCheck((int)                0x89);
            appendCheck((int64_t)            0x89);
            appendCheck((unsigned char)      0x89);
            appendCheck((unsigned short)     0x89);
            appendCheck((unsigned int)       0x89);
            appendCheck((uint64_t)           0x89);
            appendCheck((float)              0.99);
            appendCheck((double)             0.99);

            struct Bin {
                int    a{0};
                float  b{10};
                double c{0.0};
            };

            Bin bin{};
            ssize_t ss = 0, from = ob.m_offset;
            ss = ob.append(&bin, sizeof(bin));
            REQUIRE(ss == sizeof(Bin));
            REQUIRE(__Check(ob, from, &bin, sizeof(bin)));
        }

        WHEN("appending strings to buffer") {
            // clear buffer
            ob.clear();

            const char *cstr{"Hello World"};
            strview     svstr{cstr, strlen(cstr)};
            std::string str(cstr);

            ssize_t from{ob.m_offset};
            auto  ss = ob.append(cstr);
            REQUIRE(ss == strlen(cstr));
            REQUIRE(__Check(ob, from, cstr, ss));

            from = ob.m_offset;
            ss = ob.append(svstr);
            REQUIRE(ss == svstr.size());
            REQUIRE(__Check(ob, from, &svstr[0], ss));

            from = ob.m_offset;
            ss = ob.append(str);
            REQUIRE(ss == str.size());
            REQUIRE(__Check(ob, from, &str[0], ss));
        }
    }

    SECTION("Growing buffer", "[buffer][grow]") {
        // when growing buffer
        OBuffer ob(16);
        REQUIRE(ob.m_size == __TALIGN(16));
        auto ss = ob.append("0123456789123456");
        REQUIRE(ob.m_offset == 16);
        REQUIRE(ss == 16);
        REQUIRE(ob.m_size == 16);
        // append to buffer will force buffer to grow
        ss = ob.append("0123456789123456");
        REQUIRE(ob.m_offset == 32);
        REQUIRE(ss == 16);
        REQUIRE(ob.m_size == 32);
        // again, but now must grow buffer twice
        ss = ob.append("0123456789123456");
        REQUIRE(ob.m_offset == 48);
        REQUIRE(ss == 16);
        REQUIRE(ob.m_size == 64);
        // buffer should not grow, there is still room
        ss = ob.append("012345678912345");
        REQUIRE(ob.m_offset == 63);
        REQUIRE(ss == 15);
        REQUIRE(ob.m_size == 64);
        // buffer should grow, there is no room
        ss = ob.append("01");
        REQUIRE(ob.m_offset == 65);
        REQUIRE(ss == 2);
        REQUIRE(ob.m_size == 128);
    }

    SECTION("printf-style operations", "[appendf][appendnf]") {
        // test writing to buffer using printf-style API
        OBuffer ob(128);
        auto checkAppendf = [&](const char *fmt, auto... args) {
            char expected[128];
            ssize_t  sz = snprintf(expected, sizeof(expected), fmt, args...);
            ssize_t ss{0}, from = ob.m_offset;
            ss = ob.appendf(fmt, args...);
            REQUIRE(sz == ss);
            REQUIRE(__Check(ob, from, expected, ss));
        };

        checkAppendf("%d",     8);
        checkAppendf("%lu",    8lu);
        checkAppendf("%0.4f",  44.33333f);
        checkAppendf("%p",     nullptr);
        checkAppendf("%c",     'C');
        checkAppendf("Hello %s, your age is %d", "Carter", 27);

        // appendnf is a special case as it will throw an exception
        OBuffer ob2(16);
        REQUIRE_NOTHROW(ob2.appendnf(8, "%d", 1));
        // this should throw because the end formatted string will not
        // fit into buffer
        REQUIRE_THROWS(ob2.appendnf(8, "%s", "0123456789123456"));
    }

    SECTION("Stream and add operators", "[stream][add]") {
        // test using stream operators on buffer
        OBuffer ob(128);
        auto testStreamOperator = [&](OBuffer& ob, auto in, const char *expected) {
            ssize_t  ss{0}, from{ob.m_offset}, len = strlen(expected);
            ob << in;
            ss = ob.m_offset-from;
            REQUIRE(ss == len);
            REQUIRE(__Check(ob, from, expected, len));
        };

        std::string str("Hello World");
        OBuffer other{16};
        other.append(str);

        testStreamOperator(ob, 'c',                      "c");
        testStreamOperator(ob,  (short)1,                "1");
        testStreamOperator(ob,  -12,                     "-12");
        testStreamOperator(ob,  (int64_t)123,            "123");
        testStreamOperator(ob,  (unsigned char)1,        "1");
        testStreamOperator(ob,  (unsigned short)45,      "45");
        testStreamOperator(ob,  (unsigned int)23,        "23");
        testStreamOperator(ob,  (int64_t)1,              "1");
        testStreamOperator(ob,  1.6678f,                 "1.667800");
        testStreamOperator(ob,  1.3e-2,                  "0.013000");
        testStreamOperator(ob,  "Hello",                 "Hello");
        testStreamOperator(ob,  str,                     "Hello World");
        testStreamOperator(ob,  strview(str),            "Hello World");
        testStreamOperator(ob,  true,                    "true");
        testStreamOperator(ob,  std::move(other),        "Hello World");
        testStreamOperator(ob,  fmtbool(true),           "True");
        testStreamOperator(ob,  fmtbool(false),          "False");
        testStreamOperator(ob,  fmtnum("%04x",  0x20),    "0020");
        testStreamOperator(ob,  fmtnum("%0.4f", 1.2),     "1.2000");

        auto testAddOperator = [&](OBuffer& ob, auto in, const char *expected) {
            ssize_t  ss{0}, from{ob.m_offset}, len = strlen(expected);
            ob += in;
            ss = ob.m_offset-from;
            REQUIRE(ss == len);
            REQUIRE(__Check(ob, from, expected, len));
        };

        testAddOperator(ob,  str,                 "Hello World");
        testAddOperator(ob,  strview{str},        "Hello World");
        testAddOperator(ob,  "Hello world again", "Hello world again");

    }

    SECTION("Other buffer methods", "[reset][seek][bseek][release][clear][empty][capacity][size]") {
        // tests some other methods ot the OBuffer
        OBuffer b(16);
        REQUIRE(b.empty());
        b << "Hello";
        REQUIRE_FALSE(b.empty());

        WHEN("Getting buffer capacity") {
            // OBuffer::capacity() must return current available capacity
            OBuffer ob{16};
            REQUIRE(ob.capacity() == __TALIGN(16));
            ob << "01234";
            REQUIRE(ob.capacity() == 11);
            ob << "56789123456";
            REQUIRE(ob.capacity() == 0);
            ob << 'c';
            REQUIRE(ob.capacity() == 15);
        }

        WHEN("Getting buffer size") {
            // OBuffer::size() must return current number of bytes written into buffer
            OBuffer ob(16);
            REQUIRE(ob.size() == 0);
            ob << "01234";
            REQUIRE(ob.size() == 5);
            ob << "5678912345678";
            REQUIRE(ob.size() == 18);
        }

        WHEN("Clearing/Reseting buffer contents") {
            // OButter::[clear/reset] clears the buffer
            OBuffer ob{16};
            ob << "Hello";
            REQUIRE(ob.size() == 5);
            REQUIRE(ob.capacity() == 11);
            ob.clear();
            REQUIRE(ob.m_size == 0);
            REQUIRE(ob.m_offset == 0);
            REQUIRE(ob.m_data == nullptr);

            ob = OBuffer(16);
            ob << "Hello";
            REQUIRE(ob.size() == 5);
            REQUIRE(ob.capacity() == 11);
            const uint8_t* old = ob.m_data;
            ob.reset(7); // must relocate buffer since keep isn't specified
            REQUIRE(ob.m_offset == 0);
            REQUIRE(ob.m_size == __TALIGN(7));
            REQUIRE(ob.m_data == old); // realloc 8 fits into 8 so you get the same

            ob = OBuffer(16);
            ob << "Hello";
            old = ob.m_data;
            ob.reset(7, true);  // must not relocate buffer
            REQUIRE(ob.m_offset == 0);
            REQUIRE(ob.m_size   == __TALIGN(16));
            REQUIRE(old == ob.m_data);

            ob << "Hello";
            ob.reset(17, true); // must grow buffer since size is larger
            REQUIRE(ob.m_offset == 0);
            REQUIRE(ob.m_size   == __TALIGN(17));
        }

        WHEN("Seeking with buffer") {
            // OBuffer::[seek/bseek]
            OBuffer ob{16};
            ob.bseek(5);
            REQUIRE(ob.m_offset == 5);
            REQUIRE_THROWS(ob.bseek(-1));   // does not work with negative numbers
            REQUIRE(ob.m_offset == 5);
            REQUIRE_THROWS(ob.bseek(ob.m_size)); // cannot seek past the buffer size
            REQUIRE(ob.m_offset == 5);
            ob.bseek(0);
            REQUIRE(ob.m_offset == 0);

            ob.seek(10);
            REQUIRE(ob.m_offset == 10);
            REQUIRE_NOTHROW(ob.seek(-5));   // can seek backwards from current cursor
            REQUIRE(ob.m_offset == 5);
            REQUIRE_NOTHROW(ob.seek(7));    // always seeks from current cursor
            REQUIRE(ob.m_offset == 12);
            REQUIRE_THROWS(ob.seek(7));     // cannot seek past buffer size
            REQUIRE(ob.m_offset == 12);
            REQUIRE_THROWS(ob.seek(-13));   // cannot seek to negative offset
            REQUIRE(ob.m_offset == 12);
        }

        WHEN("Releasing buffer") {
            // releasing buffer gives the buffer memory and resets
            OBuffer ob{16};
            ob << "Hello Spain";
            REQUIRE_FALSE(ob.empty());
            auto buf = ob.release();
            REQUIRE_FALSE(buf == nullptr);
            REQUIRE(ob.m_data == nullptr);
            REQUIRE(ob.m_size == 0);
            REQUIRE(ob.m_offset == 0);
            REQUIRE(strcmp(buf, "Hello Spain") == 0);
            ::free(buf);
        }

        WHEN("Reserving space") {
            // memory can be reserved on buffer
            OBuffer ob{16};
            ob << "0123456789123456";
            REQUIRE(ob.capacity() == 0);
            ob.reserve(17);
            REQUIRE(ob.capacity() == 24);
        }

        WHEN("Appending hex to buffer") {
            // data can be hex printed onto buffer
            OBuffer ob{16};
            auto ss = ob.hex("Hello");
            REQUIRE(ss == 10);
            REQUIRE(__Check(ob, 0, "48656c6c6f", ss));
            std::string str{"Hello"};
            ss = ob.hex(str, true);
            REQUIRE(ss == 10);
            REQUIRE(__Check(ob, 10, "48656C6C6F", ss));
        }

        WHEN("Using other buffer operators") {
            OBuffer ob{16};
            char a[] = {'C', 'a', 'r', 't', 'e', 'r'};
            ob.append(a, sizeof(a));
            REQUIRE(ob.size() == sizeof(a));
            REQUIRE(__Check(ob, 0, a, sizeof(a)));
            // buffer will be string-fied on cast
            char *data = ob;
            REQUIRE(data[ob.m_offset] == '\0');

            ob.reset(16, true);
            ob << "Hello World";
            strview sv = ob;
            REQUIRE(__Check(ob, 0, &sv[0], sv.size()));
            std::string str = ob;
            REQUIRE(__Check(ob, 0, &str[0], str.size()));
        }
    }
}

#undef __TALIGN

#endif // unit_test

#undef __ALIGN
#undef __ALIGN2
#undef __ALIGNED