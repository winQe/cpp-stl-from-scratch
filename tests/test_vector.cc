#define CATCH_CONFIG_MAIN
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <utility>
#include <vector>

#include "stl/vector.h"

using stl::Vector;

// A small move‐only type to test perfect‐forwarding:
struct MoveOnly {
  bool moved = false;
  MoveOnly() = default;
  MoveOnly(const MoveOnly &) = delete;
  MoveOnly(MoveOnly &&other) noexcept { other.moved = true; }
  MoveOnly &operator=(const MoveOnly &) = delete;
  MoveOnly &operator=(MoveOnly &&other) noexcept {
    if (this != &other) other.moved = true;
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

// Helper to fill a vector<int> of given size with sequential values
static Vector<int> make_seq(int n) {
  Vector<int> v;
  for (int i = 0; i < n; ++i) v.push_back(i);
  return v;
}

TEST_CASE("copy constructor produces deep copy", "[vector][copy]") {
  auto original = make_seq(10);
  Vector<int> copy = original;

  // Same size / capacity
  REQUIRE(copy.size() == original.size());
  REQUIRE(copy.capacity() == original.capacity());

  // But different buffers (deep copy)
  REQUIRE(&copy[0] != &original[0]);

  // And contents match
  for (size_t i = 0; i < original.size(); ++i) REQUIRE(copy[i] == original[i]);
}

TEST_CASE("copy assignment produces deep copy and handles self-assign",
          "[vector][copy]") {
  Vector<std::string> a;
  a.push_back("one");
  a.push_back("two");

  Vector<std::string> b;
  b = a;  // copy assign

  REQUIRE(b.size() == 2);
  REQUIRE(b[0] == "one");
  REQUIRE(b[1] == "two");
  REQUIRE(&b[0] != &a[0]);  // deep

  // self-assign should be safe
  REQUIRE_NOTHROW(a = std::move(a));
  REQUIRE(a.size() == 2);
  REQUIRE(a[1] == "two");
}

TEST_CASE("move constructor steals resources", "[vector][move]") {
  auto source = make_seq(5);
  int *source_data = &source[0];
  size_t source_cap = source.capacity();
  size_t source_size = source.size();

  Vector<int> moved = std::move(source);

  // moved has the old buffer & matching metadata
  REQUIRE(moved.size() == source_size);
  REQUIRE(moved.capacity() == source_cap);
  REQUIRE(&moved[0] == source_data);

  // source is left in a valid empty state
  REQUIRE(source.size() == 0);
  REQUIRE(source.capacity() == 0);
}

TEST_CASE("move assignment steals and leaves source empty", "[vector][move]") {
  auto src = make_seq(7);
  auto dst = make_seq(3);

  int *src_data = &src[0];
  size_t src_cap = src.capacity();
  size_t src_size = src.size();

  dst = std::move(src);

  REQUIRE(dst.size() == src_size);
  REQUIRE(dst.capacity() == src_cap);
  REQUIRE(&dst[0] == src_data);

  REQUIRE(src.size() == 0);
  REQUIRE(src.capacity() == 0);
}

TEST_CASE("swap exchanges internals in O(1)", "[vector][swap]") {
  auto v1 = make_seq(4);
  auto v2 = make_seq(8);

  int *d1 = &v1[0];
  int *d2 = &v2[0];
  size_t c1 = v1.capacity(), c2 = v2.capacity();

  swap(v1, v2);

  REQUIRE(v1.size() == 8);
  REQUIRE(v1.capacity() == c2);
  REQUIRE(&v1[0] == d2);

  REQUIRE(v2.size() == 4);
  REQUIRE(v2.capacity() == c1);
  REQUIRE(&v2[0] == d1);
}

TEST_CASE("push_back only participates for convertible types",
          "[vector][push_back][concepts]") {
  stl::Vector<int> vi;

  // int literal
  vi.push_back(123);
  REQUIRE(vi.size() == 1);
  REQUIRE(vi[0] == 123);

  // short is convertible to int
  short s = 7;
  vi.push_back(s);
  REQUIRE(vi.size() == 2);
  REQUIRE(vi[1] == 7);

  // const int&
  const int ci = 42;
  vi.push_back(ci);
  REQUIRE(vi.size() == 3);
  REQUIRE(vi[2] == 42);

  // The following line should *not* compile if uncommented:
  //   std::string str = "oops";
  //   vi.push_back(str);
  //
  // (This static check can’t run at test runtime, but you can verify by
  // uncommenting.)
}

TEST_CASE("emplace_back only participates when T is constructible from Args",
          "[vector][emplace_back]") {
  stl::Vector<std::pair<int, std::string>> vp;

  // Construct from (int, const char*)
  vp.emplace_back(5, "five");
  REQUIRE(vp.size() == 1);
  auto &p = vp[0];
  REQUIRE(p.first == 5);
  REQUIRE(p.second == "five");

  // Construct from lvalue string + rvalue int
  std::string word = "ten";
  vp.emplace_back(10, word);
  REQUIRE(vp.size() == 2);
  REQUIRE(vp[1].first == 10);
  REQUIRE(vp[1].second == "ten");

  // Move-only type with in-place emplace
  stl::Vector<MoveOnly> vm;
  vm.emplace_back();  // default-construct
  REQUIRE(vm.size() == 1);

  // The following line should fail to compile if uncommented:
  //   stl::Vector<int> v_int;
  //   v_int.emplace_back("not an int");
}
