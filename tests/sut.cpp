//
// Created by dc on 22/06/17.
//
#define CATCH_CONFIG_RUNNER

#include "catch/catch.hpp"

#include <suil/net.hpp>
#include <suil/config.hpp>

TEST_CASE("Suil Version Test", "[version]") {
    REQUIRE(SUIL_MAJOR_VERSION == suil::version::MAJOR);
    REQUIRE(SUIL_MINOR_VERSION == suil::version::MINOR);
    REQUIRE(SUIL_PATCH_VERSION == suil::version::PATCH);
    REQUIRE(SUIL_BUILD_NUMBER  == suil::version::BUILD);
}

int main(int argc, const char *argv[])
{
    int result = Catch::Session().run(argc, argv);
    return (result < 0xff ? result: 0xff);
}
