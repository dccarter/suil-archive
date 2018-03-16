//
// Created by dc on 28/02/18.
//

#include <suil/merkle.hpp>

namespace suil {

}

#ifdef SUIL_TESTING
#include <catch/catch.hpp>
using namespace suil;

// Test merkle tree implementation
TEST_CASE("suil::Merkle", "[Merkle]") {
    std::vector<zcstring> data{"one1one", "two2two", "three3three"};
    auto rootHash = Merkle<zcstring>(data, 128)();

    SECTION("Should validate successfully, when given similar data") {
        auto hash = Merkle<zcstring>(data, 128)();
        REQUIRE(hash == rootHash);

        // Create the same data and verify
        std::vector<zcstring> copy{"one1one", "two2two", "three3three"};
        hash = Merkle<zcstring>(copy, 128)();
        REQUIRE(hash == rootHash);
    }

    SECTION("Should fail validation when data modified slightly") {
        std::vector<zcstring> modified{"one1one", "two2two", "threethree"};
        auto hash = Merkle<zcstring>(modified, 128)();
        REQUIRE_FALSE(rootHash == hash);

        std::vector<zcstring> modified2{"one1one", "two2two", "three3three", "four4four"};
        hash = Merkle<zcstring>(modified, 128)();
        REQUIRE_FALSE(rootHash == hash);

        std::vector<zcstring> modified3{"one1one", "two2two", "three3three", "three3three"};
        hash = Merkle<zcstring>(modified, 128)();
        REQUIRE_FALSE(rootHash == hash);
    }
}
#endif