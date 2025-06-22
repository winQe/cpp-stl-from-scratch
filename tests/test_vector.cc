#define CATCH_CONFIG_MAIN
#include "stl/vector.h"
#include <catch2/catch_test_macros.hpp>

using stl::Vector;

TEST_CASE("Default-constructed Vector has zero size and capacity",
          "[vector][ctor]") {
  Vector<int> v;
  REQUIRE(v.size() == 0);
  REQUIRE(v.capacity() == 0);
}

TEST_CASE("Vector constructed with nonzero capacity", "[vector][ctor]") {
  constexpr size_t N = 7;
  Vector<double> v(N);
  REQUIRE(v.size() == 0);
  REQUIRE(v.capacity() == N);
}
