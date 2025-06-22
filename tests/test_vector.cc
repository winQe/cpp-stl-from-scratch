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

TEST_CASE("push_back increases size and preserves values",
          "[vector][push_back]") {
  stl::Vector<int> v;
  REQUIRE(v.size() == 0);
  REQUIRE(v.capacity() == 0);

  // first push should bump capacity to at least 1
  v.push_back(10);
  REQUIRE(v.size() == 1);
  REQUIRE(v[0] == 10);

  // push several more to force a reallocate
  for (int i = 1; i <= 8; ++i) {
    v.push_back(i * 2);
  }
  REQUIRE(v.size() == 9);

  // all elements are correct
  for (size_t i = 0; i < v.size(); ++i) {
    REQUIRE(v[i] == (i == 0 ? 10 : int(i * 2)));
  }

  // capacity should be >= size
  REQUIRE(v.capacity() >= v.size());
}

TEST_CASE("pop_back decreases size and destroys last element",
          "[vector][pop_back]") {
  stl::Vector<std::string> v;
  v.push_back("alpha");
  v.push_back("beta");
  v.push_back("gamma");

  REQUIRE(v.size() == 3);
  REQUIRE(v[2] == "gamma");

  // pop once
  v.pop_back();
  REQUIRE(v.size() == 2);
  REQUIRE(v[0] == "alpha");
  REQUIRE(v[1] == "beta");

  // pop twice more to empty
  v.pop_back();
  v.pop_back();
  REQUIRE(v.size() == 0);

  // popping when empty should either be a no-op or throw;
  // choose your behavior—here we assume no-op
  REQUIRE_NOTHROW(v.pop_back());
  REQUIRE(v.size() == 0);
}
