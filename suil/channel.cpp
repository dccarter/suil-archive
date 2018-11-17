//
// Created by dc on 12/11/18.
//

#include "channel.h"

namespace suil {

    Void_t Void{};
}

#ifdef unit_test
#include <catch/catch.hpp>

using namespace suil;

TEST_CASE("Channel tests", "[common][Channel]") {
    // tests the Channel abstraction API
    SECTION("Constructor/Assignment test") {
        Channel<bool> ch{Void}; // creating a useless channel
        REQUIRE(ch.ch == nullptr);
        Channel<bool> ch2{false};  // creating a useful channel
        REQUIRE_FALSE(ch2.ch == nullptr);
        REQUIRE(ch2.ddline == -1);
        REQUIRE(ch2.waitn == -1);
        Channel<int> ch3{-10};  // creating a useful channel
        REQUIRE_FALSE(ch3.ch == nullptr);
        REQUIRE(ch3.term == -10);
        Channel<int> ch4(std::move(ch3));
        REQUIRE(ch3.ch == nullptr);
        REQUIRE_FALSE(ch4.ch == nullptr);
        REQUIRE(ch4.term == -10);
        Channel<int> ch5 = std::move(ch4);
        REQUIRE(ch4.ch == nullptr);
        REQUIRE_FALSE(ch5.ch == nullptr);
        REQUIRE(ch5.term == -10);

        struct Point {
            int  a;
            int  b;
        };
        Channel<Point> ch6{Point{-1, 0}};
        REQUIRE_FALSE(ch6.ch == nullptr);
        REQUIRE(ch6.term.a == -1);
        REQUIRE(ch6.term.b == 0);
    }
}

#endif