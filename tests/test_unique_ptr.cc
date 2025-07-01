#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <utility>

#include "stl/unique_ptr.h"

// Helper class for testing destruction
struct DestructorCounter {
  static int count;
  int value;

  DestructorCounter(int v = 0) : value(v) {}
  ~DestructorCounter() { count++; }

  static void reset() { count = 0; }
};
int DestructorCounter::count = 0;

// Basic functionality tests
TEST_CASE("UniquePtr basic construction and destruction") {
  SECTION("Default construction") {
    stl::UniquePtr<int> ptr;
    REQUIRE(ptr.get() == nullptr);
    REQUIRE_FALSE(ptr);
  }

  SECTION("Nullptr construction") {
    stl::UniquePtr<int> ptr(nullptr);
    REQUIRE(ptr.get() == nullptr);
    REQUIRE_FALSE(ptr);
  }

  SECTION("Construction with raw pointer") {
    int* raw = new int(42);
    stl::UniquePtr<int> ptr(raw);
    REQUIRE(ptr.get() == raw);
    REQUIRE(*ptr == 42);
    REQUIRE(ptr);
  }

  SECTION("Destruction calls deleter") {
    DestructorCounter::reset();
    {
      stl::UniquePtr<DestructorCounter> ptr(new DestructorCounter(42));
      REQUIRE(ptr->value == 42);
    }  // Destructor should be called here
    REQUIRE(DestructorCounter::count == 1);
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
    REQUIRE_FALSE(ptr1);
    REQUIRE(ptr2);
  }

  SECTION("Move assignment") {
    DestructorCounter::reset();
    stl::UniquePtr<DestructorCounter> ptr1(new DestructorCounter(42));
    stl::UniquePtr<DestructorCounter> ptr2(new DestructorCounter(99));

    ptr2 = std::move(ptr1);

    REQUIRE(ptr1.get() == nullptr);
    REQUIRE(ptr2->value == 42);
    REQUIRE(DestructorCounter::count ==
            1);  // ptr2's original object should be destroyed
  }

  SECTION("Self-move assignment") {
    stl::UniquePtr<int> ptr(new int(42));
    int* original = ptr.get();

    ptr = std::move(ptr);  // Self-assignment

    REQUIRE(ptr.get() == original);
    REQUIRE(*ptr == 42);
  }
}

TEST_CASE("UniquePtr member access") {
  SECTION("Dereference operator") {
    stl::UniquePtr<int> ptr(new int(42));
    REQUIRE(*ptr == 42);

    *ptr = 99;
    REQUIRE(*ptr == 99);
  }

  SECTION("Arrow operator") {
    struct TestStruct {
      int value = 123;
      void setValue(int v) { value = v; }
    };
    stl::UniquePtr<TestStruct> ptr(new TestStruct);
    REQUIRE(ptr->value == 123);

    ptr->setValue(456);
    REQUIRE(ptr->value == 456);
  }

  SECTION("Const access") {
    const stl::UniquePtr<int> ptr(new int(42));
    REQUIRE(*ptr == 42);
    REQUIRE(ptr.get() != nullptr);
  }
}

TEST_CASE("UniquePtr utility functions") {
  SECTION("reset() with nullptr") {
    DestructorCounter::reset();
    stl::UniquePtr<DestructorCounter> ptr(new DestructorCounter(42));

    ptr.reset();

    REQUIRE(ptr.get() == nullptr);
    REQUIRE(DestructorCounter::count == 1);
  }

  SECTION("reset() with new pointer") {
    DestructorCounter::reset();
    stl::UniquePtr<DestructorCounter> ptr(new DestructorCounter(42));
    DestructorCounter* new_obj = new DestructorCounter(99);

    ptr.reset(new_obj);

    REQUIRE(ptr.get() == new_obj);
    REQUIRE(ptr->value == 99);
    REQUIRE(DestructorCounter::count == 1);  // Original object destroyed
  }

  SECTION("release()") {
    stl::UniquePtr<int> ptr(new int(42));
    int* raw = ptr.release();

    REQUIRE(ptr.get() == nullptr);
    REQUIRE(*raw == 42);
    REQUIRE_FALSE(ptr);

    delete raw;  // We now own it
  }

  SECTION("swap()") {
    stl::UniquePtr<int> ptr1(new int(42));
    stl::UniquePtr<int> ptr2(new int(99));
    int* raw1 = ptr1.get();
    int* raw2 = ptr2.get();

    ptr1.swap(ptr2);

    REQUIRE(ptr1.get() == raw2);
    REQUIRE(ptr2.get() == raw1);
    REQUIRE(*ptr1 == 99);
    REQUIRE(*ptr2 == 42);
  }
}

TEST_CASE("UniquePtr boolean conversion") {
  SECTION("Empty pointer is false") {
    stl::UniquePtr<int> ptr;
    REQUIRE_FALSE(ptr);
    REQUIRE_FALSE(static_cast<bool>(ptr));
  }

  SECTION("Non-empty pointer is true") {
    stl::UniquePtr<int> ptr(new int(42));
    REQUIRE(ptr);
    REQUIRE(static_cast<bool>(ptr));
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

    REQUIRE(custom_delete_count == 1);
  }

  SECTION("Custom deleter with state") {
    struct StatefulDeleter {
      int* counter;
      StatefulDeleter(int* c) : counter(c) {}
      void operator()(int* ptr) const {
        (*counter)++;
        delete ptr;
      }
    };

    int counter = 0;
    {
      stl::UniquePtr<int, StatefulDeleter> ptr(new int(42),
                                               StatefulDeleter(&counter));
    }

    REQUIRE(counter == 1);
  }

  SECTION("Function pointer deleter") {
    static int func_delete_count = 0;
    auto deleter = [](int* ptr) {
      func_delete_count++;
      delete ptr;
    };

    func_delete_count = 0;
    {
      stl::UniquePtr<int, decltype(deleter)> ptr(new int(42), deleter);
    }

    REQUIRE(func_delete_count == 1);
  }
}

TEST_CASE("make_unique functionality") {
  SECTION("make_unique for single object") {
    auto ptr = stl::make_unique<int>(42);
    REQUIRE(*ptr == 42);
    REQUIRE(ptr.get() != nullptr);
  }

  SECTION("make_unique for object with constructor") {
    struct TestObj {
      int a, b;
      TestObj(int x, int y) : a(x), b(y) {}
    };

    auto ptr = stl::make_unique<TestObj>(10, 20);
    REQUIRE(ptr->a == 10);
    REQUIRE(ptr->b == 20);
  }
}

TEST_CASE("UniquePtr template move constructor") {
  SECTION("Move from derived to base") {
    struct Base {
      virtual ~Base() = default;
      int base_value = 100;
    };

    struct Derived : Base {
      int derived_value = 200;
    };

    stl::UniquePtr<Derived> derived_ptr(new Derived);
    Derived* raw_derived = derived_ptr.get();

    stl::UniquePtr<Base> base_ptr = std::move(derived_ptr);

    REQUIRE(base_ptr.get() == raw_derived);
    REQUIRE(derived_ptr.get() == nullptr);
    REQUIRE(base_ptr->base_value == 100);
  }
}

TEST_CASE("UniquePtr edge cases") {
  SECTION("Reset with same pointer") {
    stl::UniquePtr<int> ptr(new int(42));
    int* raw = ptr.get();

    // This should not cause double-delete
    ptr.reset(raw);
    REQUIRE(ptr.get() == raw);
    REQUIRE(*ptr == 42);
  }

  SECTION("Multiple resets") {
    DestructorCounter::reset();
    stl::UniquePtr<DestructorCounter> ptr(new DestructorCounter(1));

    ptr.reset(new DestructorCounter(2));
    ptr.reset(new DestructorCounter(3));
    ptr.reset();

    REQUIRE(DestructorCounter::count == 3);
    REQUIRE(ptr.get() == nullptr);
  }
}

TEST_CASE("Memory safety verification") {
  SECTION("No double deletion on move") {
    DestructorCounter::reset();

    stl::UniquePtr<DestructorCounter> ptr1(new DestructorCounter(42));
    stl::UniquePtr<DestructorCounter> ptr2 = std::move(ptr1);

    // Only one destruction should occur when ptr2 goes out of scope
    REQUIRE(DestructorCounter::count == 0);
  }
  // When this section ends, ptr2's destructor runs
  REQUIRE(DestructorCounter::count == 1);
}
