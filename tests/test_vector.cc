#define CATCH_CONFIG_MAIN
#include "stl/vector.h"
#include <catch2/catch_test_macros.hpp>
#include <string>

using stl::Vector;

// A small move‐only type to test perfect‐forwarding:
struct MoveOnly {
  bool moved = false;
  MoveOnly() = default;
  MoveOnly(const MoveOnly &) = delete;
  MoveOnly(MoveOnly &&other) noexcept { other.moved = true; }
  MoveOnly &operator=(const MoveOnly &) = delete;
  MoveOnly &operator=(MoveOnly &&other) noexcept {
    if (this != &other)
      other.moved = true;
    return *this;
  }
};

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
// ──────────────────────────────────────────────────────────────────────────────
//  1) L-value vs. R-value vs. const-lvalue for a trivially copyable type
TEST_CASE("push_back works for lvalues, rvalues, and const lvalues",
          "[vector][push_back][value-cats]") {
  stl::Vector<std::string> v;

  std::string foo = "foo";
  const std::string bar = "bar";

  // l-value
  v.push_back(foo);
  REQUIRE(v.size() == 1);
  REQUIRE(v[0] == "foo");

  // const l-value
  v.push_back(bar);
  REQUIRE(v.size() == 2);
  REQUIRE(v[1] == "bar");

  // r-value
  v.push_back(std::string("baz"));
  REQUIRE(v.size() == 3);
  REQUIRE(v[2] == "baz");

  // check that original foo wasn't moved-from
  REQUIRE(foo == "foo");
}

// ──────────────────────────────────────────────────────────────────────────────
//  2) Push a move-only type to ensure perfect‐forwarding is correct
TEST_CASE("push_back perfectly forwards move-only types",
          "[vector][push_back][move-only]") {
  stl::Vector<MoveOnly> v;

  MoveOnly mo;
  REQUIRE(!mo.moved);

  // push the lvalue -> should copy-construct is deleted, so our template path
  // will forward-and-move
  v.push_back(std::move(mo));
  REQUIRE(v.size() == 1);

  // the original has been moved-from
  REQUIRE(mo.moved);

  // push another temporary directly
  v.push_back(MoveOnly{});
  REQUIRE(v.size() == 2);
}

// ──────────────────────────────────────────────────────────────────────────────
//  3) Value stability after multiple grow & pushes
TEST_CASE("push_back retains existing elements across reallocations",
          "[vector][push_back][growth]") {
  stl::Vector<int> v;
  const int N = 100;

  for (int i = 0; i < N; ++i) {
    v.push_back(i);
    // after each push, size == i+1 and the last element equals i
    REQUIRE(v.size() == static_cast<size_t>(i + 1));
    REQUIRE(v[i] == i);
    // capacity is always >= size
    REQUIRE(v.capacity() >= v.size());
    // earlier elements still intact
    if (i > 0) {
      REQUIRE(v[0] == 0);
    }
  }

  // final check
  for (int i = 0; i < N; ++i) {
    REQUIRE(v[i] == i);
  }
}
