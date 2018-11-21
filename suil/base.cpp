//
// Created by dc on 07/11/18.
//

#include <fcntl.h>

#include <libmill/utils.h>

#include "base.h"

namespace suil {

    int nonblocking(int fd, bool on)
    {
        int opt = fcntl(fd, F_GETFL, 0);
        if (opt == -1)
            opt = 0;
        opt = on? opt | O_NONBLOCK : opt & ~O_NONBLOCK;
        return fcntl(fd, F_SETFL, opt);
    }

    Exception Exception::fromCurrent()
    {
        auto ex = std::current_exception();
        if (ex == nullptr) {
            // Exception::fromCurrent cannot be called outside a catch block
            throw  Exception("Exception::fromCurrent cannot be called outside a catch block");
        }

        try {
            // rethrown exception
            std::rethrow_exception(ex);
        }
        catch (const Exception& ex) {
            // catch as current and return copy
            return Exception{std::string{ex.what()}, ex.Code};
        }
        catch (const std::exception& e) {
            // catch and wrap in Exception class
            return Exception(std::string(e.what()));
        }
    }

    Datetime::Datetime(time_t t)
    : m_t(t)
    {
        gmtime_r(&m_t, &m_tm);
    }

    Datetime::Datetime()
        : Datetime(time(nullptr))
    {}

    Datetime::Datetime(const char *fmt, const char *str)
    {
        const char *tmp = HTTP_FMT;
        if (fmt) {
            tmp = fmt;
        }
        strptime(str, tmp, &m_tm);
    }

    Datetime::Datetime(const char *http_time)
        : Datetime(HTTP_FMT, http_time)
    {}

    const char* Datetime::str(char *out, size_t sz, const char *fmt)  {
        if (!out || !sz || !fmt) {
            return nullptr;
        }
        (void) strftime(out, sz, fmt, &m_tm);
        return out;
    }

    Datetime::operator time_t()  {
        if (m_t == 0) {
            m_t = timegm(&m_tm);
        }
        return m_t;
    }

    Data::Data()
        : Data((void *)nullptr, 0)
    {}

    Data::Data(void *data, size_t size, bool own)
        : m_data((uint8_t*) data),
          m_size((uint32_t)(size)),
          m_own(own)
    {}

    Data::Data(const void *data, size_t size, bool own)
        : m_cdata((uint8_t*) data),
          m_size((uint32_t)(size)),
          m_own(own)
    {}

    Data::Data(const Data& d) noexcept
        : Data()
    {
        if (d.m_size) {
            Ego.m_data = (uint8_t *) malloc(d.m_size);
            Ego.m_size = d.m_size;
            memcpy(Ego.m_data, d.m_data, d.m_size);
            Ego.m_own  = true;
        }
    }

    Data& Data::operator=(const Data& d) noexcept {
        Ego.clear();
        if (d.m_size) {
            Ego.m_data = (uint8_t *) malloc(d.m_size);
            Ego.m_size = d.m_size;
            memcpy(Ego.m_data, d.m_data, d.m_size);
            Ego.m_own  = true;
        }

        return Ego;
    }

    Data::Data(Data&& d) noexcept
        : Data(d.m_data, d.m_size, d.m_own)
    {
        d.m_data = nullptr;
        d.m_size = 0;
        d.m_own  = 0;
    }

    Data& Data::operator=(Data&& d) noexcept {
        Ego.clear();
        Ego.m_data = d.m_data;
        Ego.m_size = d.m_size;
        Ego.m_own  = d.m_own;
        d.m_data = nullptr;
        d.m_size = 0;
        d.m_own  = false;
        return Ego;
    }

    Data Data::copy() const {
        Data tmp{};
        if (Ego.m_size) {
            tmp.m_data = (uint8_t *) malloc(Ego.m_size);
            tmp.m_size = Ego.m_size;
            memcpy(tmp.m_data, Ego.m_data, Ego.m_size);
            tmp.m_own  = true;
        }
        return std::move(tmp);
    }

    void Data::clear() {
        if (Ego.m_own && Ego.m_data) {
            free(Ego.m_data);
            Ego.m_data = nullptr;
            Ego.m_size = 0;
            Ego.m_own  = 0;
        }
    }
}

#ifdef unit_test
#include <catch/catch.hpp>

using namespace suil;

TEST_CASE("common::Literals", "[common][literals]")
{
    REQUIRE(1_Mb == 1000000);
    REQUIRE(1_Kb == 1000);
    REQUIRE(1_Mb == 1000_Kb);
    REQUIRE(1_Kb == 1000_B);
    REQUIRE(1_Mb == 1000000_B);

    REQUIRE(1_us == 1/1000);
    REQUIRE(1000_us == 1_ms);
    REQUIRE(1000_ms == 1_sec);
    REQUIRE(60_sec  == 1_min);
    REQUIRE(60_min  == 1_hr);
}

TEST_CASE("common::Exception", "[common][exception]")
{
    SECTION("constructing and assigning exceptions") {
        // basic exception operation
        Exception ex;
        REQUIRE(ex.Code == 0);
        REQUIRE(strcmp("", ex.what())==0);

        ex = Exception("error message");
        REQUIRE(ex.Code == 0);
        REQUIRE(strcmp("error message", ex.what()) == 0);
        REQUIRE(strcmp(ex.what(), ex()) == 0);
        REQUIRE(ex.what() == ex());
        REQUIRE(ex.Msg == "error message");

        ex = Exception("message", Exception::UnsupportedOperation);
        REQUIRE(ex.Code == Exception::UnsupportedOperation);
        REQUIRE(strcmp("message", ex.what()) == 0);

        Exception ex2{"hello", Exception::AccessViolation};
        Exception ex3 = ex2;
        REQUIRE(ex3.Code == ex2.Code);
        REQUIRE(strcmp(ex3.what(), ex2.what()) == 0);
        REQUIRE(ex3 == ex2);
        //REQUIRE(ex3 != ex);
    }

    SECTION("creating exception using helper functions") {
        // exception helper functions
        Exception ex{Exception::create("hello")};
        REQUIRE(ex.Code == 0);
        REQUIRE(strcmp("hello", ex.what()) == 0);

        ex = Exception::create(Exception::AccessViolation, "hello");
        REQUIRE(ex.Code == Exception::AccessViolation);
        REQUIRE(strcmp("hello", ex.what()) == 0);

        ex = Exception::create("Hello ", 'C', 'a', 'r', "ter! ", 6, " is ", false);
        REQUIRE(ex.Code == 0);
        REQUIRE(strcmp("Hello Carter! 6 is 0", ex.what()) == 0);

        ex = Exception::accessViolation("oops");
        REQUIRE(ex.Code == Exception::AccessViolation);
        REQUIRE(strcmp("AccessViolation: oops", ex.what()) == 0);

        ex = Exception::indexOutOfBounds("oops");
        REQUIRE(ex.Code == Exception::IndexOutOfBounds);
        REQUIRE(strcmp("IndexOutOfBounds: oops", ex.what()) == 0);

        ex = Exception::unsupportedOperation("oops");
        REQUIRE(ex.Code == Exception::UnsupportedOperation);
        REQUIRE(strcmp("UnsupportedOperation: oops", ex.what()) == 0);
    }

    SECTION("getting current exception in try block") {
        // fromCurrent exception test
        REQUIRE_THROWS_AS(throw Exception::fromCurrent(), Exception);
        try {
            throw std::runtime_error("error");
        }
        catch (...) {
            Exception ex{Exception::fromCurrent()};
            REQUIRE(ex.Code == 0);
            REQUIRE(strcmp("error", ex()) == 0);
        }

        try {
            throw Exception::accessViolation("error");
        }
        catch (...) {
            Exception ex{Exception::fromCurrent()};
            REQUIRE(ex.Code == Exception::AccessViolation);
            REQUIRE(strcmp("AccessViolation: error", ex()) == 0);
        }
    }
}

#endif //SAPI_UNIT_TEST