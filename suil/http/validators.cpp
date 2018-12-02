//
// Created by dc on 24/11/18.
//

#include "validators.h"
#include "suil/utils.h"

namespace suil::http::validators {

    bool Email::operator()(const suil::String &addr) {
        static std::regex emailRegex{"[\\w\\.]+\\@\\w+(\\.\\w+)*(\\.[a-zA-Z]{2,})"};
        return utils::regex::match(emailRegex, addr.data(), addr.size());
    }

#define isspecial(a) (((a) > 31 && (a) < 48) || ((a) > 58 && (a) < 65) || ((a) > 90 && (a) < 97) || ((a) > 123 && (a) < 127))

    bool Password::operator()(const suil::String &passwd) {

    }

    bool Password::operator()(suil::OBuffer &ob, const suil::String &passwd) {
        if (passwd.size() < minChars) {
            ob << "Password too short, at least '" << minChars << "' characters required";
            return false;
        }
        if (passwd.size() > maxChars) {
            ob << "Password too long, at most '" << maxChars << "' characters supported";
            return false;
        }
        if (!upper && !lower && !special && !nums)
            return true;

        int u{0}, l{0}, s{0}, n{0};
        const char* data = passwd.data();
        for (int i = 0; i < passwd.size(); i++) {
            if (isspecial(data[i])) s++;
            else if (isalpha(data[i]))
                if (isupper(data[i])) u++;
                else l++;
            else if (isdigit(data[i])) n++;
        }

        if (upper && u < upper)
            ob << "Password invalid, requires at least '" << upper << "' uppercase characters";
        if (lower && l < lower)
            ob << (ob.empty()? "":"\n") << "Password invalid, requires at least '" << lower << "' lowercase characters";
        if (special && s < special)
            ob << (ob.empty()? "":"\n") << "Password invalid, requires at least '" << special << "' special characters";
        if (nums && n < nums)
            ob << (ob.empty()? "":"\n") << "Password invalid, requires at least '" << lower << "' digits";

        return !ob.empty();
    }

#undef isspecial
}

#ifdef unit_test
#include <catch/catch.hpp>

using namespace suil::http;

TEST_CASE("suil::http::validators", "[http][validators]")
{
    SECTION("validating emails", "[validator][email]") {
        /* test Email validator */
        REQUIRE(validators::Email()("carter@github.com"));
        REQUIRE(validators::Email()("last.world@gmail.com"));
        REQUIRE(validators::Email()("last123_.world@gmail.com"));
    }

}

#endif