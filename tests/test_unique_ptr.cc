#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <utility>

#include "stl/unique_ptr.h"

// Basic functionality tests
TEST_CASE("UniquePtr basic construction and destruction") {
  SECTION("Default construction") {
    stl::UniquePtr<int> ptr;
    REQUIRE(ptr.get() == nullptr);
  }

  SECTION("Construction with raw pointer") {
    int* raw = new int(42);
    stl::UniquePtr<int> ptr(raw);
    REQUIRE(ptr.get() == raw);
    REQUIRE(*ptr == 42);
  }
}

TEST_CASE("UniquePtr move semantics") {
  SECTION("Move constructor") {
    stl::UniquePtr<int> ptr1(new int(42));
    int* raw_ptr = ptr1.get();

    stl::UniquePtr<int> ptr2 = std::move(ptr1);

    REQUIRE(ptr2.get() == raw_ptr);
    REQUIRE(ptr1.get() == nullptr);
    REQUIRE(*ptr2 == 42);
  }

  SECTION("Move assignment") {
    stl::UniquePtr<int> ptr1(new int(42));
    stl::UniquePtr<int> ptr2(new int(99));

    ptr2 = std::move(ptr1);

    REQUIRE(ptr1.get() == nullptr);
    REQUIRE(*ptr2 == 42);
  }
}

TEST_CASE("UniquePtr member access") {
  SECTION("Dereference operator") {
    stl::UniquePtr<int> ptr(new int(42));
    REQUIRE(*ptr == 42);
  }

  SECTION("Arrow operator") {
    struct TestStruct {
      int value = 123;
    };
    stl::UniquePtr<TestStruct> ptr(new TestStruct);
    REQUIRE(ptr->value == 123);
  }
}

TEST_CASE("UniquePtr utility functions") {
  SECTION("reset() with nullptr") {
    stl::UniquePtr<int> ptr(new int(42));
    ptr.reset();
    REQUIRE(ptr.get() == nullptr);
  }

  SECTION("reset() with new pointer") {
    stl::UniquePtr<int> ptr(new int(42));
    int* new_raw = new int(99);
    ptr.reset(new_raw);
    REQUIRE(ptr.get() == new_raw);
    REQUIRE(*ptr == 99);
  }

  SECTION("release()") {
    stl::UniquePtr<int> ptr(new int(42));
    int* raw = ptr.release();
    REQUIRE(ptr.get() == nullptr);
    REQUIRE(*raw == 42);
    delete raw;  // We now own it
  }
}

TEST_CASE("UniquePtr boolean conversion") {
  SECTION("Empty pointer is false") {
    stl::UniquePtr<int> ptr;
    REQUIRE_FALSE(ptr);
  }

  SECTION("Non-empty pointer is true") {
    stl::UniquePtr<int> ptr(new int(42));
    REQUIRE(ptr);
  }
}

TEST_CASE("Memory leak detection") {
  static int destroy_count = 0;

  struct TestObj {
    ~TestObj() { destroy_count++; }
  };

  SECTION("Move assignment should delete old object") {
    destroy_count = 0;
    stl::UniquePtr<TestObj> ptr1(new TestObj);
    stl::UniquePtr<TestObj> ptr2(new TestObj);

    ptr1 = std::move(ptr2);  // Should destroy ptr1's old object

    REQUIRE(destroy_count == 1);  // This will FAIL
  }

  SECTION("reset() should delete old object") {
    destroy_count = 0;
    stl::UniquePtr<TestObj> ptr(new TestObj);

    ptr.reset(new TestObj);  // Should destroy old object

    REQUIRE(destroy_count == 1);  // This will FAIL
  }
}

TEST_CASE("Custom deleter") {
  static int custom_delete_count = 0;

  struct CustomDeleter {
    void operator()(int* ptr) const {
      custom_delete_count++;
      delete ptr;
    }
  };

  SECTION("Custom deleter should be called") {
    custom_delete_count = 0;
    {
      stl::UniquePtr<int, CustomDeleter> ptr(new int(42));
    }  // Destructor called here

    REQUIRE(custom_delete_count == 1);  // This will FAIL
  }
}
