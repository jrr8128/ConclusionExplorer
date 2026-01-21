#include "catch_amalgamated.hpp"
#include "syntax.hpp"

TEST_CASE("syntax_tests") { 
    auto t = build_terms(3);
    CHECK(t.size() == 3);
    CHECK(t[0] == 'A');
    CHECK(t[1] == 'B');
    CHECK(t[2] == 'C');
}
